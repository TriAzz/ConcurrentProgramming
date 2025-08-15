#include <iostream>
#include <cstdlib>
#include <time.h>
#include <chrono>
#include <string>
#include <fstream>
#include <iomanip>

using namespace std::chrono;
using namespace std;

//INITIALISATION SECTION ----------------------------------------------------------------------------------------------------------------------------------------------------------

//Global variables for matrix dimensions
int N;

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
    cout << "\nWelcome to Serial Matrix Multiplication" << endl;
    
    do {
        cout << "Enter matrix size: ";
        cin >> size;
        
        if (size <= 0)
        {
            cout << "Invalid matrix size. Please enter a positive number." << endl;
        }
    } while (size <= 0);
    
    return size;
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

//MAIN FUNCTION -------------------------------------------------------------------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    srand(time(0));
    
    // Get matrix size from user input or command line argument
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
    
    cout << "Matrices allocated. Ready to run benchmark." << endl;
    
    runSerial(A, B, C);
    
    freeMatrix(A, N);
    freeMatrix(B, N);
    freeMatrix(C, N);
    
    return 0;
}
