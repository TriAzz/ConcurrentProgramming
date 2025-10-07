#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <sstream>
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


//Main function
int main() {
    //User inputs
    int N;
    cout << "Enter the number of top congested traffic lights to display per hour: ";
    cin >> N;

    //Output file stream
    std::ofstream out("TrafficSimulatorSeqResults.txt");
    if (!out) {
        cout << "Error opening TrafficSimulatorSeqResults.txt" << endl;
        return 1;
    }

    //Start timing
    auto start = chrono::high_resolution_clock::now();

    //Read all lines from data file
    ifstream in("data.txt");
    if (!in) {
        out << "Error opening data.txt" << endl;
        return 1;
    }

    //Store lines in vector
    map<string, map<int, int>> hour_to_light;

    //Process each line
    string line;
    while (getline(in, line)) {
        string date, time, lid_str, cars_str;
        stringstream ss(line);
        if (ss >> date >> time >> lid_str >> cars_str) {
            int light = stoi(lid_str);
            int cars = stoi(cars_str);
            string hour = date + " " + time.substr(0,2); 
            hour_to_light[hour][light] += cars;
        }
    }
    in.close();

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
