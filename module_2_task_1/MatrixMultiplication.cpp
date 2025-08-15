#include <iostream>
#include <cstdlib>
#include <time.h>
#include <chrono>
#include <pthread.h>
#include <omp.h>
#include <string>
#include <fstream>
#include <iomanip>

#define NUM_THREADS 8

using namespace std::chrono;
using namespace std;

//INITIALISATION SECTION ----------------------------------------------------------------------------------------------------------------------------------------------------------

//Global variables for matrix dimensions
int N;

//Structure to pass data to random matrix initialisation threads
struct randomTask
{
    double **matrix;
    int start_row;
    int end_row;
    int matrix_size;
};

//Structure to pass data to matrix multiplication threads
struct multiplyTask
{
    double **A;
    double **B;
    double **C;
    int start_row;
    int end_row;
    int matrix_size;
};

//Function to allocate a matrix
double** allocateMatrix(int size)
{
    double **matrix = (double**)malloc(size * sizeof(double*));
    for (int i = 0; i < size; i++)
    {
        matrix[i] = (double*)malloc(size * sizeof(double));
    }
    return matrix;
}

//Function to free a matrix
void freeMatrix(double **matrix, int size)
{
    for (int i = 0; i < size; i++)
    {
        free(matrix[i]);
    }
    free(matrix);
}

//Function to initialise a matrix with zeros
void initialiseResultMatrix(double **matrix, int size)
{
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            matrix[i][j] = 0.0;
        }
    }
}

//Function to format numbers with commas (Written completely by AI)
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

//MENU FUNCTIONS ---------------------------------------------------------------------------------------------------------------------------------------------------------------------

//Function to get matrix size from user
int getMatrixSize()
{
    int size;
    cout << "\nWelcome to Parallel Matrix Multiplication" << endl;
    
    do {
        cout << "Enter matrix size: ";
        cin >> size;
        
        if (size <= 0)
        {
            cout << "Invalid matrix size. Please enter a valid number." << endl;
        }
    } while (size <= 0);
    
    return size;
}

//Function to display menu and get user choice
int getMenuChoice()
{
    cout << "\nHow would you like to multiply the matrix?" << endl;
    cout << "Choose implementation:" << endl;
    cout << "1. Serial" << endl;
    cout << "2. Pthread" << endl;
    cout << "3. OpenMP" << endl;
    cout << "0. Exit" << endl;
    cout << "Enter choice: ";
    
    int choice;
    cin >> choice;
    return choice;
}

//Function to get number of threads from user
int getThreadCount()
{
    int threads;
    cout << "Enter number of threads to use: ";
    cin >> threads;
    
    if (threads <= 0)
    {
        cout << "Invalid thread count. Using default of 8 threads." << endl;
        threads = 8;
    }
    
    return threads;
}

//FILE OUTPUT FUNCTION -------------------------------------------------------------------------------------------------------------------------------------------------------

void writeMatricesToFile(double **A, double **B, double **C, int size, const string& implementation)
{
    string filename = "matrix_multiplication_" + implementation + "_" + to_string(size) + "x" + to_string(size) + ".txt";
    ofstream file(filename);
    
    if (!file.is_open())
    {
        cout << "Error: Could not create output file " << filename << endl;
        return;
    }
    
    file << "Matrix Multiplication Results" << endl;
    file << "Implementation: " << implementation << endl;
    file << "Matrix Size: " << size << "x" << size << endl;
    file << "Equation: A × B = C" << endl;
    file << "------------------------------\n" << endl;
    
    file << fixed << setprecision(2);
    
    file << "Matrix A (" << size << "x" << size << "):" << endl;
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            file << setw(8) << A[i][j];
            if (j < size - 1) file << " ";
        }
        file << endl;
    }
    file << endl;
    
    file << "Matrix B (" << size << "x" << size << "):" << endl;
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            file << setw(8) << B[i][j];
            if (j < size - 1) file << " ";
        }
        file << endl;
    }
    file << endl;
    
    file << "Matrix C = A × B (" << size << "x" << size << "):" << endl;
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            file << setw(8) << C[i][j];
            if (j < size - 1) file << " ";
        }
        file << endl;
    }
    
    file.close();
    cout << "Matrices written to file: " << filename << endl;
}

//SERIAL IMPLEMENTATION SECTION -------------------------------------------------------------------------------------------------------------------------------------------------------

//Function to initialise matrix with random values
void initialiseMatrix(double **matrix, int size)
{
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            matrix[i][j] = (double)(rand() % 100) / 10.0; 
        }
    }
}

//Serial matrix multiplication implementation
void matrixMultiplySerial(double **A, double **B, double **C, int size)
{
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            C[i][j] = 0.0;
            for (int k = 0; k < size; k++)
            {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

//Function to run the serial implementation
void runSerial(double **A, double **B, double **C)
{
    cout << "\nSerial Implementation" << endl;
    cout << "Matrix size: " << N << "x" << N << endl;
    
    long long durations[10];
    
    for (int run = 0; run < 10; run++)
    {
        initialiseMatrix(A, N);
        initialiseMatrix(B, N);
        
        auto start = high_resolution_clock::now();
        matrixMultiplySerial(A, B, C, N);
        auto stop = high_resolution_clock::now();
        
        auto duration = duration_cast<microseconds>(stop - start);
        durations[run] = duration.count();
    }
    
    for (int i = 0; i < 10; i++)
    {
        cout << "Run " << (i + 1) << " - Time taken: " << formatWithCommas(durations[i]) << " microseconds" << endl;
    }
    
    long long total_time = 0;
    for (int i = 0; i < 10; i++)
    {
        total_time += durations[i];
    }
    double average = (double)total_time / 10.0;
    cout << "Average time over 10 runs: " << formatWithCommas((long long)average) << " microseconds" << endl;
    
    writeMatricesToFile(A, B, C, N, "Serial");
}

//PTHREAD IMPLEMENTATION SECTION -------------------------------------------------------------------------------------------------------------------------------------------------------

//Pthread worker function for matrix initialisation
void* initialiseMatrixPthread(void* args)
{
    randomTask *task = (randomTask*)args;
    
    for (int i = task->start_row; i < task->end_row; i++)
    {
        for (int j = 0; j < task->matrix_size; j++)
        {
            task->matrix[i][j] = (double)(rand() % 100) / 10.0; 
        }
    }
    return NULL;
}

//Pthread worker function for matrix multiplication
void* matrixMultiplyPthread(void* args)
{
    multiplyTask *task = (multiplyTask*)args;
    
    for (int i = task->start_row; i < task->end_row; i++)
    {
        for (int j = 0; j < task->matrix_size; j++)
        {
            task->C[i][j] = 0.0;
            for (int k = 0; k < task->matrix_size; k++)
            {
                task->C[i][j] += task->A[i][k] * task->B[k][j];
            }
        }
    }
    return NULL;
}

//Function to run pthread implementation
void runPthread(double **A, double **B, double **C)
{
    int num_threads = getThreadCount();
    
    cout << "\nPthread Implementation" << endl;
    cout << "Matrix size: " << N << "x" << N << endl;
    cout << "Threads: " << num_threads << endl;
    
    pthread_t *initThreads = new pthread_t[num_threads];
    pthread_t *multiplyThreads = new pthread_t[num_threads];
    randomTask *initTasks = new randomTask[num_threads];
    multiplyTask *multiplyTasks = new multiplyTask[num_threads];
    long long durations[10];
    
    for (int run = 0; run < 10; run++)
    {
        int rows_per_thread = N / num_threads;
        int remaining_rows = N % num_threads;
        
        for (int t = 0; t < num_threads; t++)
        {
            initTasks[t].matrix = A;
            initTasks[t].matrix_size = N;
            initTasks[t].start_row = t * rows_per_thread;
            initTasks[t].end_row = (t + 1) * rows_per_thread;
            
            if (t == num_threads - 1)
            {
                initTasks[t].end_row += remaining_rows;
            }
            
            pthread_create(&initThreads[t], NULL, initialiseMatrixPthread, (void*)&initTasks[t]);
        }
        
        for (int t = 0; t < num_threads; t++)
        {
            pthread_join(initThreads[t], NULL);
        }
        
        for (int t = 0; t < num_threads; t++)
        {
            initTasks[t].matrix = B;
            initTasks[t].matrix_size = N;
            initTasks[t].start_row = t * rows_per_thread;
            initTasks[t].end_row = (t + 1) * rows_per_thread;
            
            if (t == num_threads - 1)
            {
                initTasks[t].end_row += remaining_rows;
            }
            
            pthread_create(&initThreads[t], NULL, initialiseMatrixPthread, (void*)&initTasks[t]);
        }
        
        for (int t = 0; t < num_threads; t++)
        {
            pthread_join(initThreads[t], NULL);
        }
        
        auto start = high_resolution_clock::now();
        
        for (int t = 0; t < num_threads; t++)
        {
            multiplyTasks[t].A = A;
            multiplyTasks[t].B = B;
            multiplyTasks[t].C = C;
            multiplyTasks[t].matrix_size = N;
            multiplyTasks[t].start_row = t * rows_per_thread;
            multiplyTasks[t].end_row = (t + 1) * rows_per_thread;
            
            if (t == num_threads - 1)
            {
                multiplyTasks[t].end_row += remaining_rows;
            }
            
            pthread_create(&multiplyThreads[t], NULL, matrixMultiplyPthread, (void*)&multiplyTasks[t]);
        }
        
        for (int t = 0; t < num_threads; t++)
        {
            pthread_join(multiplyThreads[t], NULL);
        }
        
        auto stop = high_resolution_clock::now();
        
        auto duration = duration_cast<microseconds>(stop - start);
        durations[run] = duration.count();
    }
    
    for (int i = 0; i < 10; i++)
    {
        cout << "Run " << (i + 1) << " - Time taken: " << formatWithCommas(durations[i]) << " microseconds" << endl;
    }
    
    long long total_time = 0;
    for (int i = 0; i < 10; i++)
    {
        total_time += durations[i];
    }
    double average = (double)total_time / 10.0;
    cout << "Average time over 10 runs: " << formatWithCommas((long long)average) << " microseconds" << endl;
    
    writeMatricesToFile(A, B, C, N, "Pthread");
    
    delete[] initThreads;
    delete[] multiplyThreads;
    delete[] initTasks;
    delete[] multiplyTasks;
}

//OPENMP IMPLEMENTATION SECTION -------------------------------------------------------------------------------------------------------------------------------------------------------

//OpenMP matrix initialisation implementation
void initialiseMatrixOpenMP(double **matrix, int size, int num_threads)
{
    #pragma omp parallel num_threads(num_threads) default(none) shared(matrix, size)
    {
        #pragma omp for
        for (int i = 0; i < size; i++)
        {
            for (int j = 0; j < size; j++)
            {
                matrix[i][j] = (double)(rand() % 100) / 10.0; 
            }
        }
    }
}

//OpenMP matrix multiplication implementation
void matrixMultiplyOpenMP(double **A, double **B, double **C, int size, int num_threads)
{
    #pragma omp parallel num_threads(num_threads) default(none) shared(A, B, C, size)
    {
        #pragma omp for
        for (int i = 0; i < size; i++)
        {
            for (int j = 0; j < size; j++)
            {
                C[i][j] = 0.0;
                for (int k = 0; k < size; k++)
                {
                    C[i][j] += A[i][k] * B[k][j];
                }
            }
        }
    }
}

//Function to run OpenMP implementation
void runOpenMP(double **A, double **B, double **C)
{
    int num_threads = getThreadCount();
    
    cout << "\nOpenMP Implementation" << endl;
    cout << "Matrix size: " << N << "x" << N << endl;
    cout << "Threads: " << num_threads << endl;
    
    long long durations[10];
    
    for (int run = 0; run < 10; run++)
    {
        initialiseMatrixOpenMP(A, N, num_threads);
        initialiseMatrixOpenMP(B, N, num_threads);
        
        auto start = high_resolution_clock::now();
        matrixMultiplyOpenMP(A, B, C, N, num_threads);
        auto stop = high_resolution_clock::now();
        
        auto duration = duration_cast<microseconds>(stop - start);
        durations[run] = duration.count();
    }
    
    for (int i = 0; i < 10; i++)
    {
        cout << "Run " << (i + 1) << " - Time taken: " << formatWithCommas(durations[i]) << " microseconds" << endl;
    }
    
    long long total_time = 0;
    for (int i = 0; i < 10; i++)
    {
        total_time += durations[i];
    }
    double average = (double)total_time / 10.0;
    cout << "Average time over 10 runs: " << formatWithCommas((long long)average) << " microseconds" << endl;
    
    writeMatricesToFile(A, B, C, N, "OpenMP");
}

//MAIN FUNCTION -------------------------------------------------------------------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    srand(time(0));
    
    if (argc > 1)
    {
        N = atoi(argv[1]);
        if (N <= 0)
        {
            cout << "Invalid matrix size from command line. Getting size from user input." << endl;
            N = getMatrixSize();
        }
        else
        {
            cout << "Using matrix size from command line: " << N << "x" << N << endl;
        }
    }
    else
    {
        N = getMatrixSize();
    }
    
    cout << "\nAllocating " << N << "x" << N << " matrices..." << endl;
    
    double **A = allocateMatrix(N);
    double **B = allocateMatrix(N);
    double **C = allocateMatrix(N);
    
    cout << "Matrices allocated. Ready to run functions." << endl;
    
    int choice;
    do
    {
        choice = getMenuChoice();
        
        switch (choice)
        {
            case 1:
                runSerial(A, B, C);
                break;
            case 2:
                runPthread(A, B, C);
                break;
            case 3:
                runOpenMP(A, B, C);
                break;
            case 0:
                cout << "Exiting..." << endl;
                break;
            default:
                cout << "Invalid choice. Please try again." << endl;
                break;
        }
    } while (choice != 0);
    
    freeMatrix(A, N);
    freeMatrix(B, N);
    freeMatrix(C, N);
    
    return 0;
}