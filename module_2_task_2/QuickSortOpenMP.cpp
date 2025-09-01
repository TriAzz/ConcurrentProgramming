#include <iostream>
#include <cstdlib>
#include <time.h>
#include <chrono>
#include <string>
#include <iomanip>
#include <fstream>
#include <vector>
#include <omp.h>

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

//OpenMP QuickSort function with threshold for small arrays
void quickSort(int array[], int low, int high)
{
    const int Threshold = 100000;
    
    if (low < high)
    {
        int pivot = partition(array, low, high);

        if (high - low > Threshold)
        {
            #pragma omp task
            quickSort(array, low, pivot - 1);
            
            #pragma omp task
            quickSort(array, pivot + 1, high);
            
            #pragma omp taskwait
        }
        else
        {
            quickSort(array, low, pivot - 1);
            quickSort(array, pivot + 1, high);
        }
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

    string filename = "QuickSortOpenMPResults.txt";
    ofstream outfile(filename);
    if (!outfile.is_open())
    {
        cout << "Error: Could not create output file " << filename << endl;
        return 1;
    }

    outfile << "---------------------------------------------" << endl;
    outfile << "OpenMP QuickSort Results" << endl;
    outfile << "---------------------------------------------" << endl;

    outfile << endl;

    cout << "Starting OpenMP QuickSort" << endl;
    cout << "Number of threads: " << omp_get_max_threads() << endl;
    cout << endl;

    //Inputs array sizes and thread counts to test
    int threadCounts[] = {2, 6, 10, 16, 30, 50, 100};
    int numThreadCounts = sizeof(threadCounts) / sizeof(threadCounts[0]);

    int testSizes[] = {10, 100, 1000, 10000, 100000, 1000000, 10000000};
    int numSizes = sizeof(testSizes) / sizeof(testSizes[0]);    

    vector<vector<long long>> results(numSizes, vector<long long>(numThreadCounts, 0));

    for (int sizeIndex = 0; sizeIndex < numSizes; sizeIndex++)
    {
        int size = testSizes[sizeIndex];

        cout << "Testing array size: " << formatWithCommas(size) << endl;

        outfile << "Array Size: " << formatWithCommas(size) << endl;
        outfile << "------------------------" << endl;

        int* array = new int[size];

        for (int threadIndex = 0; threadIndex < numThreadCounts; threadIndex++)
        {
            int numThreads = threadCounts[threadIndex];
            omp_set_num_threads(numThreads);

            cout << "  Threads: " << numThreads << endl;

            long long durations[10];

            for (int run = 0; run < 10; run++)
            {
                initializeArray(array, size);

                auto start = high_resolution_clock::now();

                #pragma omp parallel
                {
                    #pragma omp single
                    quickSort(array, 0, size - 1);
                }

                auto stop = high_resolution_clock::now();

                auto duration = duration_cast<microseconds>(stop - start);
                durations[run] = duration.count();
            }

            //Calculates average
            long long total_time = 0;
            for (int i = 0; i < 10; i++)
            {
                total_time += durations[i];
            }
            double average = (double)total_time / 10.0;
            long long avgLong = (long long)average;

            results[sizeIndex][threadIndex] = avgLong;

            outfile << "Threads " << numThreads << " - Average: " << formatWithCommas(avgLong) << " microseconds" << endl;

            //Verifies array is sorted correctly (check the array from the last timed run)
            if (!verifySorted(array, size))
            {
                outfile << "Array was not sorted correctly" << endl;
                cout << "Array size " << formatWithCommas(size) << " with " << numThreads << " threads was not sorted correctly" << endl;
            }
        }

        outfile << endl;

        delete[] array;
    }

    //Write final summary using stored averages in table format
    outfile << "Performance Table" << endl;
    outfile << "----------------------------------------------------" << endl;
    outfile << endl;

    outfile << "| Array Size |";
    for (int threadIndex = 0; threadIndex < numThreadCounts; threadIndex++)
    {
        outfile << " " << threadCounts[threadIndex] << " threads |";
    }
    outfile << endl;

    outfile << "|------------|";
    for (int threadIndex = 0; threadIndex < numThreadCounts; threadIndex++)
    {
        outfile << "------------|";
    }
    outfile << endl;

    for (int sizeIndex = 0; sizeIndex < numSizes; sizeIndex++)
    {
        outfile << "| " << formatWithCommas(testSizes[sizeIndex]) << " |";
        for (int threadIndex = 0; threadIndex < numThreadCounts; threadIndex++)
        {
            outfile << " " << formatWithCommas(results[sizeIndex][threadIndex]) << " |";
        }
        outfile << endl;
    }

    outfile << endl;

    cout << "\nTesting Complete" << endl;
    cout << "Results written to: " << filename << endl;
}
