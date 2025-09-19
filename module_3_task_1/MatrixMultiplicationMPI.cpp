#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <string>
#include <fstream>
#include <iomanip>
#include <vector>
#include <mpi.h>

using namespace std::chrono;
using namespace std;

//CONFIGURATION SECTION -----------------------------------------------------------------------------------

const vector<int> MATRIX_SIZES = {128, 256, 512, 768, 1024, 1536, 2048};
const int NUM_RUNS = 10;

//UTILITY FUNCTIONS SECTION --------------------------------------------------------------------------------

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

//MPI IMPLEMENTATION SECTION ------------------------------------------------------------------

//Function to run the MPI benchmark for a specific matrix size
long long runMPI(int size, int world_size, int world_rank) {
    double **A = nullptr, **B = nullptr, **C = nullptr;

    //Root process allocates the full matrices
    if (world_rank == 0) {
        A = allocateMatrix(size, size);
        B = allocateMatrix(size, size);
        C = allocateMatrix(size, size);
    } else {
        //Worker processes only need to allocate matrix B to receive the broadcast
        B = allocateMatrix(size, size);
    }

    //Each process allocates its local portion of A and C
    int rows_per_proc = size / world_size;
    double **local_A = allocateMatrix(rows_per_proc, size);
    double **local_C = allocateMatrix(rows_per_proc, size);

    long long total_duration = 0;

    //Run the benchmark multiple times to get an average
    for (int run = 0; run < NUM_RUNS; ++run) {
        //Root process initializes matrices for this run
        if (world_rank == 0) {
            initializeMatrix(A, size);
            initializeMatrix(B, size);
        }

        //Synchronize all processes before starting the timer
        MPI_Barrier(MPI_COMM_WORLD);
        auto start = high_resolution_clock::now();

        //Scatter rows of A from root to all processes
        MPI_Scatter(A ? A[0] : NULL, rows_per_proc * size, MPI_DOUBLE,
                    local_A[0], rows_per_proc * size, MPI_DOUBLE,
                    0, MPI_COMM_WORLD);

        //Broadcast matrix B to all processes
        MPI_Bcast(B[0], size * size, MPI_DOUBLE, 0, MPI_COMM_WORLD);

        //Each process performs its portion of the matrix multiplication
        for (int i = 0; i < rows_per_proc; i++) {
            for (int j = 0; j < size; j++) {
                local_C[i][j] = 0.0;
                for (int k = 0; k < size; k++) {
                    local_C[i][j] += local_A[i][k] * B[k][j];
                }
            }
        }

        //Gather results from all processes back to the root
        MPI_Gather(local_C[0], rows_per_proc * size, MPI_DOUBLE,
                   C ? C[0] : NULL, rows_per_proc * size, MPI_DOUBLE,
                   0, MPI_COMM_WORLD);

        //Synchronize before stopping timer
        MPI_Barrier(MPI_COMM_WORLD);
        auto stop = high_resolution_clock::now();

        //Only the root process records the time
        if (world_rank == 0) {
            total_duration += duration_cast<microseconds>(stop - start).count();
        }
    }

    freeMatrix(local_A);
    freeMatrix(local_C);
    if (world_rank == 0) {
        freeMatrix(A);
        freeMatrix(C);
    }
    freeMatrix(B);

    //Return the average time (only relevant on root)
    return (world_rank == 0) ? (total_duration / NUM_RUNS) : 0;
}

//MAIN FUNCTION ------------------------------------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    //Seed the random number generator on the root process
    if (world_rank == 0) {
        srand(time(0));
    }

    //Loop over all specified matrix sizes
    for (int size : MATRIX_SIZES) {
        //Check if the current matrix size is divisible by the number of processes
        if (size % world_size != 0) {
            if (world_rank == 0) {
                cout << "MPI " << size << "x" << size << " (" << world_size << " processes): SKIPPED (not divisible)" << endl;
            }
            continue; 
        }

        long long avg_time = runMPI(size, world_size, world_rank);

        //The root process prints the final result for this size
        if (world_rank == 0) {
            cout << "MPI " << size << "x" << size << " (" << world_size << " processes): " << avg_time << endl;
        }
    }

    //Finalise MPI
    MPI_Finalize();
    return 0;
}