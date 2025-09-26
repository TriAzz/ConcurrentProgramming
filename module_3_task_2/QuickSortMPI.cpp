#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <string>
#include <iomanip>
#include <fstream>
#include <vector>
#include <mpi.h>
#include <algorithm>

using namespace std::chrono;
using namespace std;

//CONFIGURATION SECTION ------------------------------------------------------------------

const vector<long long> ARRAY_SIZES = {16, 96, 1024, 10000, 100000, 1000000, 10000000, 100000000};
const int NUM_RUNS = 10;

//UTILITY FUNCTIONS SECTION -----------------------------------------------------------------

//Function to format numbers with commas
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

//Function to initialize array with random values
void initializeArray(int array[], int size)
{
    for (int i = 0; i < size; i++)
    {
        array[i] = rand();
    }
}

//Function to verify if array is sorted
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

//MPI IMPLEMENTATION SECTION ----------------------------------------------------------------

//Standard sequential QuickSort partition function
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

//Standard sequential QuickSort function, used by each process on its local data
void quickSort(int array[], int low, int high)
{
    if (low < high)
    {
        int pi = partition(array, low, high);
        quickSort(array, low, pi - 1);
        quickSort(array, pi + 1, high);
    }
}

//Function to merge two sorted arrays
void merge(int* arr, int l, int m, int r) {
    int i, j, k;
    int n1 = m - l + 1;
    int n2 = r - m;

    int* L = new int[n1];
    int* R = new int[n2];

    for (i = 0; i < n1; i++) L[i] = arr[l + i];
    for (j = 0; j < n2; j++) R[j] = arr[m + 1 + j];

    i = 0; j = 0; k = l;
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k] = L[i];
            i++;
        } else {
            arr[k] = R[j];
            j++;
        }
        k++;
    }

    while (i < n1) { arr[k] = L[i]; i++; k++; }
    while (j < n2) { arr[k] = R[j]; j++; k++; }

    delete[] L;
    delete[] R;
}


//MAIN FUNCTION -----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);

    int rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (rank == 0) {
        srand(time(0));
        cout << "Starting MPI QuickSort" << endl;
        cout << endl;
    }

    for (long long size : ARRAY_SIZES)
    {
        // Ensure the array can be split evenly among processes
        if (size % world_size != 0) {
            if (rank == 0) {
                cout << "MPI QuickSort (" << world_size << " processes, size " << formatWithCommas(size) << "): SKIPPED (not divisible)" << endl;
            }
            continue;
        }

        if (rank == 0) {
            cout << "Testing array size: " << formatWithCommas(size) << endl;
        }

        int* array = nullptr;
        if (rank == 0) {
            array = new int[size];
        }

        int local_size = size / world_size;
        int* local_array = new int[local_size];
        
        long long total_duration = 0;

        for (int run = 0; run < 10; run++)
        {
            if (rank == 0) {
                initializeArray(array, size);
            }
            
            MPI_Barrier(MPI_COMM_WORLD);
            auto start = high_resolution_clock::now();

            MPI_Scatter(array, local_size, MPI_INT, local_array, local_size, MPI_INT, 0, MPI_COMM_WORLD);

            quickSort(local_array, 0, local_size - 1);

            MPI_Gather(local_array, local_size, MPI_INT, array, local_size, MPI_INT, 0, MPI_COMM_WORLD);
            
            if (rank == 0) {
                for (int i = 1; i < world_size; i++) {
                    merge(array, 0, (i * local_size) - 1, ((i + 1) * local_size) - 1);
                }
            }
            
            MPI_Barrier(MPI_COMM_WORLD);
            auto stop = high_resolution_clock::now();

            if (rank == 0) {
                total_duration += duration_cast<microseconds>(stop - start).count();
            }
        }
        
        if (rank == 0) {
            long long avg_time = total_duration / NUM_RUNS;
            cout << "MPI QuickSort (" << world_size << " processes, size " << formatWithCommas(size) << "): " << formatWithCommas(avg_time) << " microseconds" << endl;
            if (!verifySorted(array, size)) {
                cout << "Array was not sorted correctly." << endl;
            }
            delete[] array;
        }
        delete[] local_array;
    }
    
    if (rank == 0) {
        cout << "\nTesting Complete" << endl;
    }

    MPI_Finalize();
    return 0;
}