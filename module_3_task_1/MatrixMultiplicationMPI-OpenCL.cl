__kernel void matrix_mul(
    const __global double* A,
    const __global double* B,
    __global double* C,
    const int N,
    const int rows_per_proc)
{
    int i = get_global_id(0); // Row index (0 to rows_per_proc-1)
    int j = get_global_id(1); // Column index (0 to N-1)

    if (i >= rows_per_proc || j >= N) return;

    double sum = 0.0;
    for (int k = 0; k < N; ++k) {
        sum += A[i * N + k] * B[k * N + j];
    }
    C[i * N + j] = sum;
}