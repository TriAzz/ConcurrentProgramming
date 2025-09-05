#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <algorithm>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <omp.h>
#include <chrono>

using namespace std;

//Formats number with commas
string formatWithCommas(long long number)
{
    string str = to_string(number);
    string result = "";
    int count = 0;

    for (int i = str.length() - 1; i >= 0; i--)
    {
        if (count == 3)
        {
            result = "," + result;
            count = 0;
        }
        result = str[i] + result;
        count++;
    }

    return result;
}

//Construct to hold traffic data
struct TrafficData {
    string timestamp;
    int light_id;
    int cars;
};

//Main function
int main() {
    //User inputs
    int N = 5;
    vector<int> thread_counts = {2, 5, 10, 50, 100};
    vector<int> buffer_sizes = {2, 10, 50, 100, 1000};

    //Read all lines from data file
    vector<string> lines;
    ifstream in("data.txt");
    if (!in) {
        cout << "Error opening data.txt" << endl;
        return 1;
    }
    string line;
    while (getline(in, line)) {
        lines.push_back(line);
    }
    in.close();
    int total_lines = lines.size();

    //Output file stream
    std::ofstream out("TrafficSimulatorCombinedResults.md");
    if (!out) {
        cout << "Error opening TrafficSimulatorCombinedResults.md" << endl;
        return 1;
    }

    //Sequential timing
    long long seq_total = 0;
    for (int run = 0; run < 10; ++run) {
        auto start_seq = chrono::high_resolution_clock::now();
        map<string, map<int, int>> hour_to_light_seq;
        for (const auto& ln : lines) {
            string date, time, lid_str, cars_str;
            stringstream ss(ln);
            if (ss >> date >> time >> lid_str >> cars_str) {
                int light = stoi(lid_str);
                int cars = stoi(cars_str);
                string hour = date + " " + time.substr(0,2);
                hour_to_light_seq[hour][light] += cars;
            }
        }
        auto end_seq = chrono::high_resolution_clock::now();
        auto elapsed_seq = chrono::duration_cast<chrono::microseconds>(end_seq - start_seq);
        seq_total += elapsed_seq.count();
    }
    long long seq_avg = seq_total / 10;

    //Write header
    out << "# Traffic Simulator Combined Results\n";
    out << "**Sequential average processing time (10 runs):** " << formatWithCommas(seq_avg) << " microseconds\n\n";
    out << "## OpenMP Results Table\n";
    out << "| Thread Count ";
    for (int buf : buffer_sizes) out << "| Buffer Size " << buf << " ";
    out << "|\n";
    out << "|---";
    for (size_t i = 0; i < buffer_sizes.size(); ++i) out << "|---";
    out << "|\n";

    //OpenMP timing
    for (int threads : thread_counts) {
        out << "| " << threads << " ";
        for (int buf : buffer_sizes) {
            long long omp_total = 0;
            for (int run = 0; run < 10; ++run) {
                queue<TrafficData> buffer;
                mutex mtx;
                condition_variable cv_full, cv_empty;
                map<string, map<int, int>> hour_to_light_omp;
                mutex map_mtx;
                atomic<int> current_line(0);
                atomic<int> active_producers(threads);

                //Producer function
                auto producer = [&]() {
                    while (true) {
                        int idx = current_line++;
                        if (idx >= total_lines) break;

                        stringstream ss(lines[idx]);
                        string date, time, lid_str, cars_str;
                        if (ss >> date >> time >> lid_str >> cars_str) {
                            TrafficData data;
                            data.timestamp = date + " " + time;
                            data.light_id = stoi(lid_str);
                            data.cars = stoi(cars_str);

                            unique_lock<mutex> lock(mtx);
                            cv_full.wait(lock, [&]() { return buffer.size() < buf; });
                            buffer.push(data);
                            cv_empty.notify_one();
                        }
                    }
                    active_producers--;
                    cv_empty.notify_all();
                };

                //Consumer function
                auto consumer = [&]() {
                    while (true) {
                        TrafficData data;
                        {
                            unique_lock<mutex> lock(mtx);
                            cv_empty.wait(lock, [&]() { return !buffer.empty() || active_producers == 0; });
                            if (buffer.empty() && active_producers == 0) break;
                            data = buffer.front();
                            buffer.pop();
                            cv_full.notify_one();
                        }

                        string hour = data.timestamp.substr(0, 13);
                        {
                            lock_guard<mutex> lock(map_mtx);
                            hour_to_light_omp[hour][data.light_id] += data.cars;
                        }
                    }
                };

                //Launch producer and consumer threads
                auto start_omp = chrono::high_resolution_clock::now();
                #pragma omp parallel num_threads(threads * 2)
                {
                    int tid = omp_get_thread_num();
                    if (tid < threads) {
                        producer();
                    } else {
                        consumer();
                    }
                }
                auto end_omp = chrono::high_resolution_clock::now();
                auto elapsed_omp = chrono::duration_cast<chrono::microseconds>(end_omp - start_omp);
                omp_total += elapsed_omp.count();
            }
            long long omp_avg = omp_total / 10;
            out << "| " << formatWithCommas(omp_avg) << " ";
        }
        out << "|\n";
    }

    //Detailed ratio analysis
    out << "\n## OpenMP Producer-Consumer Ratio Table\n";
    out << "| Producer Threads | Consumer Threads | Avg Time (microseconds) |\n";
    out << "|---|---|---|\n";
    vector<pair<int, int>> ratios = {{1,4},{2,3},{2,2},{3,2},{4,1}};
    for (auto ratio : ratios) {
        int producers = ratio.first;
        int consumers = ratio.second;
        long long omp_total = 0;
        for (int run = 0; run < 10; ++run) {
            queue<TrafficData> buffer;
            mutex mtx;
            condition_variable cv_full, cv_empty;
            map<string, map<int, int>> hour_to_light_omp;
            mutex map_mtx;
            atomic<int> current_line(0);
            atomic<int> active_producers(producers);

            //Producer function
            auto producer = [&]() {
                while (true) {
                    int idx = current_line++;
                    if (idx >= total_lines) break;
                    stringstream ss(lines[idx]);
                    string date, time, lid_str, cars_str;
                    if (ss >> date >> time >> lid_str >> cars_str) {
                        TrafficData data;
                        data.timestamp = date + " " + time;
                        data.light_id = stoi(lid_str);
                        data.cars = stoi(cars_str);
                        unique_lock<mutex> lock(mtx);
                        cv_full.wait(lock, [&]() { return buffer.size() < 100; });
                        buffer.push(data);
                        cv_empty.notify_one();
                    }
                }
                active_producers--;
                cv_empty.notify_all();
            };

            //Consumer function
            auto consumer = [&]() {
                while (true) {
                    TrafficData data;
                    {
                        unique_lock<mutex> lock(mtx);
                        cv_empty.wait(lock, [&]() { return !buffer.empty() || active_producers == 0; });
                        if (buffer.empty() && active_producers == 0) break;
                        data = buffer.front();
                        buffer.pop();
                        cv_full.notify_one();
                    }
                    string hour = data.timestamp.substr(0, 13);
                    {
                        lock_guard<mutex> lock(map_mtx);
                        hour_to_light_omp[hour][data.light_id] += data.cars;
                    }
                }
            };

            //Launch producer and consumer threads
            auto start_omp = chrono::high_resolution_clock::now();
            #pragma omp parallel num_threads(producers + consumers)
            {
                int tid = omp_get_thread_num();
                if (tid < producers) {
                    producer();
                } else {
                    consumer();
                }
            }
            auto end_omp = chrono::high_resolution_clock::now();
            auto elapsed_omp = chrono::duration_cast<chrono::microseconds>(end_omp - start_omp);
            omp_total += elapsed_omp.count();
        }
        long long omp_avg = omp_total / 10;
        out << "| " << producers << " | " << consumers << " | " << formatWithCommas(omp_avg) << " |\n";
    }
    out.close();
    return 0;
}
