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
    int N, num_producers, num_consumers, buffer_size;
    cout << "Enter the number of top congested traffic lights to display per hour: ";
    cin >> N;
    cout << "Enter number of producer threads: ";
    cin >> num_producers;
    cout << "Enter number of consumer threads: ";
    cin >> num_consumers;
    cout << "Enter buffer size: ";
    cin >> buffer_size;

    //Output file stream
    std::ofstream out("TrafficSimulatorOpenMPResults.txt");
    if (!out) {
        cout << "Error opening TrafficSimulatorOpenMPResults.txt" << endl;
        return 1;
    }

    //Start timing
    auto start = chrono::high_resolution_clock::now();

    //Read all lines from data file
    vector<string> lines;
    ifstream in("data.txt");
    if (!in) {
        out << "Error opening data.txt" << endl;
        return 1;
    }
    string line;
    while (getline(in, line)) {
        lines.push_back(line);
    }
    in.close();
    int total_lines = lines.size();

    //Shared resources
    queue<TrafficData> buffer;
    mutex mtx;
    condition_variable cv_full, cv_empty;
    map<string, map<int, int>> hour_to_light;
    mutex map_mtx;
    atomic<int> current_line(0);
    atomic<int> active_producers(num_producers);

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
                cv_full.wait(lock, [&]() { return buffer.size() < buffer_size; });
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
                hour_to_light[hour][data.light_id] += data.cars;
            }
        }
    };

    //Launch producer and consumer threads
    #pragma omp parallel num_threads(num_producers + num_consumers)
    {
        int tid = omp_get_thread_num();
        if (tid < num_producers) {
            producer();
        } else {
            consumer();
        }
    }

    //Stop timing
    auto end = chrono::high_resolution_clock::now();
    auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start);

    //Output results
    for (auto& hour_entry : hour_to_light) {
        string hour = hour_entry.first;
        auto& lights = hour_entry.second;

        vector<pair<int, int>> vec; 
        for (auto& p : lights) {
            vec.push_back({p.second, p.first});
        }
        sort(vec.rbegin(), vec.rend()); 

        out << "Top " << N << " congested lights for hour " << hour << ":" << endl;
        for (int i = 0; i < min(N, (int)vec.size()); i++) {
            out << "  Light " << vec[i].second << ": " << vec[i].first << " cars" << endl;
        }
        out << endl;
    }

    out << "Processing time: " << formatWithCommas(elapsed.count()) << " microseconds" << endl;
    out.close();
    return 0;
}
