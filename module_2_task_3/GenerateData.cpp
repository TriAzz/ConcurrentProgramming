#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <ctime>

using namespace std;

int main() {
    srand(time(0));

    int numLights, numHours;
    cout << "Enter number of traffic lights: ";
    cin >> numLights;
    cout << "Enter number of hours: ";
    cin >> numHours;

    ofstream out("data.txt");
    if (!out) {
        cout << "Error opening file" << endl;
        return 1;
    }

    int totalMeasurements = numHours * 12;
    int hour = 0;
    int minute = 0;
    int day = 1;
    string date = "2025-01-01";

    for(int i = 0; i < totalMeasurements; i++) {
        string timestamp = date + " " + (hour < 10 ? "0" : "") + to_string(hour) + ":" +
                           (minute < 10 ? "0" : "") + to_string(minute) + ":00";

        for(int light = 1; light <= numLights; light++) {
            int cars = rand() % 101;
            out << timestamp << " " << light << " " << cars << endl;
        }

        minute += 5;
        if(minute == 60) {
            minute = 0;
            hour++;
            if(hour == 24) {
                hour = 0;
                day++;
                date = "2025-01-" + string(day < 10 ? "0" : "") + to_string(day);
            }
        }
    }

    out.close();
    cout << "Data generated in data.txt" << endl;
    return 0;
}
