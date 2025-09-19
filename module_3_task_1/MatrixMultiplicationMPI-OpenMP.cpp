#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <string>
#include <fstream>
#include <iomanip>
#include <vector>
#include <mpi.h>
#include <omp.h> 

using namespace std::chrono;
using namespace std;

//CONFIGURATION SECTION ------------------------------------------------------------------------------

const vector<int> MATRIX_SIZES = {128, 256, 512, 768, 1024, 1536, 2048};
const int NUM_RUNS = 10;

//UTILITY FUNCTIONS SECTION ------------------------------------------------------------------------

//Function to allocate a matrix
double** allocateMatrix(int rows, int cols) {
    double **matrix = (double**)malloc(rows * sizeof(double*));
    //Allocate one contiguous block for better data locality and easier MPI transfers
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

//HYBRID MPI+OPENMP IMPLEMENTATION SECTION -------------------------------------------------------------

//Function to run the hybrid MPI+OpenMP test
long long runMPI_OpenMP(int size, int world_size, int world_rank) {
    double **A = nullptr, **B = nullptr, **C = nullptr;

    //Root process allocates the full matrices
    if (world_rank == 0) {
        A = allocateMatrix(size, size);
        B = allocateMatrix(size, size);
        C = allocateMatrix(size, size);
    } else {
        B = allocateMatrix(size, size);
    }

    //Each process allocates its local portion of A and C
    int rows_per_proc = size / world_size;
    double **local_A = allocateMatrix(rows_per_proc, size);
    double **local_C = allocateMatrix(rows_per_proc, size);

    long long total_duration = 0;

    //Run the test multiple times
    for (int run = 0; run < NUM_RUNS; ++run) {

        //Root process initializes matrices for this run
        if (world_rank == 0) {
            initializeMatrix(A, size);
            initializeMatrix(B, size);
        }

        //Synchronize all processes before starting the timer
        MPI_Barrier(MPI_COMM_WORLD);
        auto start = high_resolution_clock::now();

        //Scatter the rows of matrix A to all processes
        MPI_Scatter(A ? A[0] : NULL, rows_per_proc * size, MPI_DOUBLE,
                    local_A[0], rows_per_proc * size, MPI_DOUBLE,
                    0, MPI_COMM_WORLD);

        //Broadcast matrix B to all processes
        MPI_Bcast(B[0], size * size, MPI_DOUBLE, 0, MPI_COMM_WORLD);

        //Perform local matrix multiplication using OpenMP
        #pragma omp parallel for
        for (int i = 0; i < rows_per_proc; i++) {
            for (int j = 0; j < size; j++) {
                local_C[i][j] = 0.0;
                for (int k = 0; k < size; k++) {
                    local_C[i][j] += local_A[i][k] * B[k][j];
                }
            }
        }

        //Gather the results back to the root process
        MPI_Gather(local_C[0], rows_per_proc * size, MPI_DOUBLE,
                   C ? C[0] : NULL, rows_per_proc * size, MPI_DOUBLE,
                   0, MPI_COMM_WORLD);

        MPI_Barrier(MPI_COMM_WORLD);
        auto stop = high_resolution_clock::now();

        //Record the duration for this run
        if (world_rank == 0) {
            total_duration += duration_cast<microseconds>(stop - start).count();
        }
    }

    //Free allocated memory
    freeMatrix(local_A);
    freeMatrix(local_C);
    if (world_rank == 0) {
        freeMatrix(A);
        freeMatrix(C);
    }
    freeMatrix(B);

    return (world_rank == 0) ? (total_duration / NUM_RUNS) : 0;
}

//MAIN FUNCTION ----------------------------------------------------------------------------------------------------

//Function to run the MPI+OpenMP test
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

    //Run tests for each matrix size
    for (int size : MATRIX_SIZES) {
        if (size % world_size != 0) {
            //Ensure matrix size is divisible by number of processes
            if (world_rank == 0) {
                int num_threads = omp_get_max_threads();
                cout << "MPI+OpenMP " << size << "x" << size << " (Processes: " << world_size << ", Threads: " << num_threads << "): SKIPPED (not divisible)" << endl;
            }
            continue; 
        }

        //Run the MPI+OpenMP implementation
        long long avg_time = runMPI_OpenMP(size, world_size, world_rank);

        //Print the results
        if (world_rank == 0) {
            int num_threads = omp_get_max_threads();
            cout << "MPI+OpenMP " << size << "x" << size << " (Processes: " << world_size << ", Threads: " << num_threads << "): " << avg_time << endl;
        }
    }

    //Finalize MPI
    MPI_Finalize();
    return 0;
}