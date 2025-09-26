#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <string>
#include <iomanip>
#include <fstream>
#include <vector>
#include <stack>
#include <mpi.h>
#include <CL/cl.h>

using namespace std::chrono;
using namespace std;

//CONFIGURATION SECTION ------------------------------------------------------------------

const vector<long long> ARRAY_SIZES = {16, 96, 1024, 10000, 100000, 1000000, 10000000, 100000000};
const int NUM_RUNS = 10;
const int GPU_SORT_THRESHOLD = 4096; // Use sequential sort for small sub-arrays

//UTILITY FUNCTIONS SECTION -----------------------------------------------------------------

//Function to load an OpenCL kernel source from a file
string loadKernelFromFile(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open kernel file: " << filename << endl;
        exit(1);
    }
    return string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
}

//Function to format numbers with commas
string formatWithCommas(long long number)
{
    string str = to_string(number);
    string result = "";
    int count = 0;
    for (int i = str.length() - 1; i >= 0; i--) {
        if (count == 3) { result = "," + result; count = 0; }
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

//Function to get the name of the OpenCL device
string getOpenCLDeviceName(cl_device_id device) {
    char device_name[1024] = {0};
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL);
    string name_str = device_name;
    name_str.erase(name_str.find_last_not_of(" \n\r\t")+1);
    return name_str;
}

//Standard sequential QuickSort partition function
int partitionCPU(int arr[], int low, int high) {
    int pivot = arr[high];
    int i = (low - 1);
    for (int j = low; j <= high - 1; j++) {
        if (arr[j] <= pivot) {
            i++;
            swap(arr[i], arr[j]);
        }
    }
    swap(arr[i + 1], arr[high]);
    return (i + 1);
}

//Sequential QuickSort for small sub-arrays
void quickSortCPU(int arr[], int low, int high) {
    if (low < high) {
        int pi = partitionCPU(arr, low, high);
        quickSortCPU(arr, low, pi - 1);
        quickSortCPU(arr, pi + 1, high);
    }
}


//Hybrid QuickSort on the GPU
void quickSortGPU(int* data, int size, cl_context context, cl_command_queue queue, cl_kernel kernel) {
    if (size <= 1) return;

    cl_int err;
    stack<pair<int, int>> s;
    s.push({0, size - 1});

    cl_mem data_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, size * sizeof(int), data, &err);
    cl_mem less_count_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int), NULL, &err);
    cl_mem greater_count_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int), NULL, &err);

    //Iterative QuickSort using stack
    while (!s.empty()) {
        int low = s.top().first;
        int high = s.top().second;
        s.pop();

        if (low >= high) continue;

        if (high - low + 1 < GPU_SORT_THRESHOLD) {
            int chunk_size = high - low + 1;
            int* cpu_chunk = new int[chunk_size];
            clEnqueueReadBuffer(queue, data_buffer, CL_TRUE, low * sizeof(int), chunk_size * sizeof(int), cpu_chunk, 0, NULL, NULL);
            quickSortCPU(cpu_chunk, 0, chunk_size - 1);
            clEnqueueWriteBuffer(queue, data_buffer, CL_TRUE, low * sizeof(int), chunk_size * sizeof(int), cpu_chunk, 0, NULL, NULL);
            delete[] cpu_chunk;
            continue;
        }

        int pivot;
        clEnqueueReadBuffer(queue, data_buffer, CL_TRUE, high * sizeof(int), sizeof(int), &pivot, 0, NULL, NULL);
        
        int zero = 0;
        clEnqueueWriteBuffer(queue, less_count_buffer, CL_TRUE, 0, sizeof(int), &zero, 0, NULL, NULL);
        clEnqueueWriteBuffer(queue, greater_count_buffer, CL_TRUE, 0, sizeof(int), &zero, 0, NULL, NULL);

        clSetKernelArg(kernel, 0, sizeof(cl_mem), &data_buffer);
        clSetKernelArg(kernel, 1, sizeof(int), &low);
        clSetKernelArg(kernel, 2, sizeof(int), &high);
        clSetKernelArg(kernel, 3, sizeof(int), &pivot);
        clSetKernelArg(kernel, 4, sizeof(cl_mem), &less_count_buffer);
        clSetKernelArg(kernel, 5, sizeof(cl_mem), &greater_count_buffer);

        size_t global_size = high - low + 1;
        clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, NULL, 0, NULL, NULL);

        clFinish(queue);

        int less_count;
        clEnqueueReadBuffer(queue, less_count_buffer, CL_TRUE, 0, sizeof(int), &less_count, 0, NULL, NULL);
        
        int pivot_idx = low + less_count;
        
        s.push({low, pivot_idx - 1});
        s.push({pivot_idx + 1, high});
    }

    clEnqueueReadBuffer(queue, data_buffer, CL_TRUE, 0, size * sizeof(int), data, 0, NULL, NULL);
    clReleaseMemObject(data_buffer);
    clReleaseMemObject(less_count_buffer);
    clReleaseMemObject(greater_count_buffer);
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
        cout << "Starting Hybrid MPI+OpenCL QuickSort" << endl;
        cout << endl;
    }
    
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    cl_int err;

    clGetPlatformIDs(1, &platform, NULL);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    queue = clCreateCommandQueue(context, device, 0, &err);
    string kernelSource = loadKernelFromFile("QuickSortMPI-OpenCL.cl");
    const char* kernelSourceCStr = kernelSource.c_str();
    program = clCreateProgramWithSource(context, 1, &kernelSourceCStr, NULL, &err);
    clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    kernel = clCreateKernel(program, "parallel_partition", &err);
    string deviceName = getOpenCLDeviceName(device);

    for (long long size : ARRAY_SIZES)
    {
         if (size % world_size != 0) {
             if (rank == 0) {
                cout << "MPI+OpenCL QuickSort (Processes: " << world_size << ", Device: " << deviceName << ", size " << formatWithCommas(size) << "): SKIPPED (not divisible)" << endl;
             }
             continue;
        }

        if (rank == 0) {
            cout << "Testing array size: " << formatWithCommas(size) << endl;
        }
        
        long long total_duration = 0;
        int* array = nullptr;

        for (int run = 0; run < 10; ++run) {
            if (rank == 0) {
                array = new int[size];
                initializeArray(array, size);
            }

            int local_size = size / world_size;
            int* local_array = new int[local_size];
            
            MPI_Barrier(MPI_COMM_WORLD);
            auto start = high_resolution_clock::now();
            
            MPI_Scatter(array, local_size, MPI_INT, local_array, local_size, MPI_INT, 0, MPI_COMM_WORLD);

            quickSortGPU(local_array, local_size, context, queue, kernel);

            MPI_Gather(local_array, local_size, MPI_INT, array, local_size, MPI_INT, 0, MPI_COMM_WORLD);
            
            
            MPI_Barrier(MPI_COMM_WORLD);
            auto stop = high_resolution_clock::now();

            if (rank == 0) {
                total_duration += duration_cast<microseconds>(stop - start).count();
                if(array) delete[] array;
            }
            delete[] local_array;
        }

        if (rank == 0) {
            long long avg_time = total_duration / NUM_RUNS;
            cout << "MPI+OpenCL QuickSort (Processes: " << world_size << ", Device: " << deviceName << ", size " << formatWithCommas(size) << "): " << formatWithCommas(avg_time) << " microseconds" << endl;
        }
    }
    
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    if (rank == 0) {
        cout << "\nTesting Complete" << endl;
    }

    MPI_Finalize();
    return 0;
}