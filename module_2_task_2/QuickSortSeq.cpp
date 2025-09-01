#include <iostream>
#include <cstdlib>
#include <time.h>
#include <chrono>
#include <string>
#include <iomanip>
#include <fstream>
#include <vector>

using namespace std::chrono;
using namespace std;

//Function to format numbers with commas
string formatWithCommas(long long number)
{
    string str = to_string(number);
    string result = "";
    int count = 0;

    //Insert commas from right to left
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

//Function to initialize array with random values
void initializeArray(int array[], int size)
{
    for (int i = 0; i < size; i++)
    {
        array[i] = rand();
    }
}

//Function to perform partitioning
int partition(int array[], int low, int high)
{
    int pivot = array[high];
    int i = (low - 1); 

    for (int j = low; j <= high - 1; j++)
    {
        if (array[j] <= pivot)
        {
            i++; 
            swap(array[i], array[j]);
        }
    }
    swap(array[i + 1], array[high]);
    return (i + 1);
}

//Sequential QuickSort function
void quickSort(int array[], int low, int high)
{
    if (low < high)
    {
        int pivot = partition(array, low, high);

        quickSort(array, low, pivot - 1);
        quickSort(array, pivot + 1, high);
    }
}

//Function to verify if array is sorted (not timed)
bool verifySorted(int array[], int size)
{
    for (int i = 0; i < size - 1; i++)
    {
        if (array[i] > array[i + 1])
        {
            return false;
        }
    }
    return true;
}

//Main function that calls other functions
int main()
{
    //Setup text output file and write heading
    srand(time(0));

    string filename = "QuickSortSeqResults.txt";
    ofstream outfile(filename);
    if (!outfile.is_open())
    {
        cout << "Error: Could not create output file " << filename << endl;
        return 1;
    }

    outfile << "---------------------------------------------" << endl;
    outfile << "Sequential QuickSort Results" << endl;
    outfile << "---------------------------------------------" << endl;
    
    outfile << endl;

    cout << "Starting Sequential QuickSort" << endl;
    cout << endl;

    //Stores average times for each array size to display them at end of file
    vector<pair<int, long long>> sizeAverages;

    //Inputs array sizes to test
    int testSizes[] = {10, 100, 1000, 10000, 100000, 1000000, 10000000};
    int numSizes = sizeof(testSizes) / sizeof(testSizes[0]);

    for (int sizeIndex = 0; sizeIndex < numSizes; sizeIndex++)
    {
        int size = testSizes[sizeIndex];

        cout << "Testing array size: " << formatWithCommas(size) << endl;


        outfile << "Array Size: " << formatWithCommas(size) << endl;
        outfile << "------------------------" << endl;

        int* array = new int[size];

        long long durations[10];

        for (int run = 0; run < 10; run++)
        {
            initializeArray(array, size);

            auto start = high_resolution_clock::now();

            quickSort(array, 0, size - 1);

            auto stop = high_resolution_clock::now();

            auto duration = duration_cast<microseconds>(stop - start);
            durations[run] = duration.count();

            outfile << "Run " << (run + 1) << " - Time taken: " << formatWithCommas(durations[run]) << " microseconds" << endl;
        }

        //Calculates average
        long long total_time = 0;
        for (int i = 0; i < 10; i++)
        {
            total_time += durations[i];
        }
        double average = (double)total_time / 10.0;
        long long avgLong = (long long)average;

        outfile << "Average time over 10 runs: " << formatWithCommas(avgLong) << " microseconds" << endl;

        sizeAverages.push_back(make_pair(size, avgLong));

        //Verifies array is sorted correctly
        if (!verifySorted(array, size))
        {
            outfile << "Array was not sorted correctly" << endl;
            cout << "Array size " << formatWithCommas(size) << " was not sorted correctly" << endl;
        }

        outfile << endl;

        delete[] array;
    }

    //Write final summary using stored averages
    outfile << "FINAL SUMMARY - Average Times by Array Size" << endl;
    outfile << "------------------------------------------" << endl;

    cout << "Final Summary of Averages:" << endl;

    for (size_t i = 0; i < sizeAverages.size(); i++)
    {
        string line = "Array Size " + formatWithCommas(sizeAverages[i].first) + ": " + formatWithCommas(sizeAverages[i].second) + " microseconds";
        cout << line << endl;
        outfile << line << endl;
    }

    outfile << endl;
    outfile << "Total array sizes tested: " << numSizes << endl;

    outfile.close();
    cout << "\nResults written to: " << filename << endl;

    return 0;
}
