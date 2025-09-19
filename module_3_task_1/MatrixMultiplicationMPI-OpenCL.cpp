#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <string>
#include <fstream>
#include <iomanip>
#include <vector>
#include <mpi.h>
#include <CL/cl.h>

using namespace std::chrono;
using namespace std;

//CONFIGURATION SECTION ----------------------------------------------------

const vector<int> MATRIX_SIZES = {128, 256, 512, 768, 1024, 1536, 2048};
const int NUM_RUNS = 10;

//UTILITY FUNCTIONS SECTION ----------------------------------------------------

//Function to load an OpenCL kernel source from a file
string loadKernelFromFile(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open kernel file: " << filename << endl;
        exit(1); // Exit if the kernel file is essential and not found
    }
    return string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
}

//Function to allocate a matrix
double** allocateMatrix(int rows, int cols) {
    double **matrix = (double**)malloc(rows * sizeof(double*));
    double *data = (double*)malloc(rows * cols * sizeof(double));
    for (int i = 0; i < rows; i++) {
        matrix[i] = &(data[i * cols]);
    }
    return matrix;
}

//Function to free a matrix
void freeMatrix(double **matrix) {
    free(matrix[0]);
    free(matrix);
}

//Function to initialize matrix with random values
void initializeMatrix(double **matrix, int size) {
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            matrix[i][j] = (double)(rand() % 100) / 10.0;
        }
    }
}

//HYBRID MPI+OPENCL IMPLEMENTATION SECTION ----------------------------------------------------

//Function to get the name of the OpenCL device
string getOpenCLDeviceName(cl_device_id device) {
    char device_name[1024] = {0};
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL);
    string name_str = device_name;
    name_str.erase(name_str.find_last_not_of(" \n\r\t")+1);
    return name_str;
}

//Function to run the hybrid MPI-OpenCL implementation
long long runMPI_OpenCL(int size, int world_size, int world_rank, cl_context context, cl_command_queue queue, cl_kernel kernel) {
    double **A = nullptr, **B = nullptr, **C = nullptr;
    cl_int err;

    //Allocate matrices
    if (world_rank == 0) {
        A = allocateMatrix(size, size);
        B = allocateMatrix(size, size);
        C = allocateMatrix(size, size);
    } else {
        B = allocateMatrix(size, size);
    }
    int rows_per_proc = size / world_size;
    double **local_A = allocateMatrix(rows_per_proc, size);
    double **local_C = allocateMatrix(rows_per_proc, size);

    long long total_duration = 0;

    //Run the test multiple times
    for (int run = 0; run < NUM_RUNS; ++run) {
        if (world_rank == 0) {
            initializeMatrix(A, size);
            initializeMatrix(B, size);
        }

        MPI_Barrier(MPI_COMM_WORLD);
        auto start = high_resolution_clock::now();

        MPI_Scatter(A ? A[0] : NULL, rows_per_proc * size, MPI_DOUBLE,
                    local_A[0], rows_per_proc * size, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Bcast(B[0], size * size, MPI_DOUBLE, 0, MPI_COMM_WORLD);

        //OpenCL matrix multiplication
        cl_mem bufferA = clCreateBuffer(context, CL_MEM_READ_ONLY, rows_per_proc * size * sizeof(double), NULL, &err);
        cl_mem bufferB = clCreateBuffer(context, CL_MEM_READ_ONLY, size * size * sizeof(double), NULL, &err);
        cl_mem bufferC = clCreateBuffer(context, CL_MEM_WRITE_ONLY, rows_per_proc * size * sizeof(double), NULL, &err);

        //Copy data to device
        clEnqueueWriteBuffer(queue, bufferA, CL_TRUE, 0, rows_per_proc * size * sizeof(double), local_A[0], 0, NULL, NULL);
        clEnqueueWriteBuffer(queue, bufferB, CL_TRUE, 0, size * size * sizeof(double), B[0], 0, NULL, NULL);

        //Set kernel arguments
        clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferA);
        clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferB);
        clSetKernelArg(kernel, 2, sizeof(cl_mem), &bufferC);
        clSetKernelArg(kernel, 3, sizeof(int), &size);
        clSetKernelArg(kernel, 4, sizeof(int), &rows_per_proc);

        //Execute the kernel and read back the result
        size_t global_work_size[2] = { (size_t)rows_per_proc, (size_t)size };
        clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global_work_size, NULL, 0, NULL, NULL);
        
        clEnqueueReadBuffer(queue, bufferC, CL_TRUE, 0, rows_per_proc * size * sizeof(double), local_C[0], 0, NULL, NULL);

        //Free OpenCL buffers
        clReleaseMemObject(bufferA);
        clReleaseMemObject(bufferB);
        clReleaseMemObject(bufferC);

        MPI_Gather(local_C[0], rows_per_proc * size, MPI_DOUBLE,
                   C ? C[0] : NULL, rows_per_proc * size, MPI_DOUBLE, 0, MPI_COMM_WORLD);

        MPI_Barrier(MPI_COMM_WORLD);
        auto stop = high_resolution_clock::now();

        if (world_rank == 0) {
            total_duration += duration_cast<microseconds>(stop - start).count();
        }
    }

    //Free memory
    freeMatrix(local_A);
    freeMatrix(local_C);
    if (world_rank == 0) {
        freeMatrix(A);
        freeMatrix(C);
    }
    freeMatrix(B);

    return (world_rank == 0) ? (total_duration / NUM_RUNS) : 0;
}

//MAIN FUNCTION ----------------------------------------------------

//Function to run the MPI+OpenCL test
int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    //Get the number of processes and the rank of this process
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    //Seed random number generator only on root
    if (world_rank == 0) {
        srand(time(0));
    }

    // OpenCL setup
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    cl_int err;

    //Find platform and device
    clGetPlatformIDs(1, &platform, NULL);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    queue = clCreateCommandQueue(context, device, 0, &err);

    //Load and build kernel
    string kernelSource = loadKernelFromFile("MatrixMultiplicationMPI-OpenCL.cl");
    const char* kernelSourceCStr = kernelSource.c_str();
    program = clCreateProgramWithSource(context, 1, &kernelSourceCStr, NULL, &err);

    //Build the program
    clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    kernel = clCreateKernel(program, "matrix_mul", &err);
    string deviceName = getOpenCLDeviceName(device);

    //Loop over all specified matrix sizes
    for (int size : MATRIX_SIZES) {
        if (size % world_size != 0) {
            //Ensure matrix size is divisible by number of processes
            if (world_rank == 0) {
                cout << "MPI+OpenCL " << size << "x" << size << " (Processes: " << world_size << ", Device: " << deviceName << "): SKIPPED" << endl;
            }
            continue;
        }

        //Run the MPI+OpenCL implementation
        long long avg_time = runMPI_OpenCL(size, world_size, world_rank, context, queue, kernel);

        //Print the results
        if (world_rank == 0) {
            cout << "MPI+OpenCL " << size << "x" << size << " (Processes: " << world_size << ", Device: " << deviceName << "): " << avg_time << endl;
        }
    }

    //Final OpenCL Cleanup
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    //Finalise MPI
    MPI_Finalize();
    return 0;
}