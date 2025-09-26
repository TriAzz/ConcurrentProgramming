# QuickSort Performance Comparison

This task involved parallelising the QuickSort algorithm using multiple distributed programming implementations. The goal was to compare the performance of a standard serial implementation against versions using MPI, and a hybrid model of MPI with OpenCL. Note though, that these tests and results are based on running MPI and MPI + OpenCL on a single machine, and NOT in a true distributed manner like what would be intended.

## Comparing Parallel Implementations to Serial

### Baseline Tests

Before analysing the distributed and hybrid models, baseline performance was established using the serial and OpenMP implementations. These tests highlight the inherent challenges of parallelising QuickSort.

#### Serial Implementation

| Array Size | Average Time (µs) |
|------------|-------------------|
| 16 | 0 |
| 96 | 2 |
| 1,024 | 36 |
| 10,000 | 451 |
| 100,000 | 5,289 |
| 1,000,000 | 50,902 |
| 10,000,000 | 1,058,571 |
| 100,000,000 | 60,793,771 |

#### OpenMP Implementation

| Array Size | 2 Threads | 6 Threads | 10 Threads | 16 Threads | 30 Threads | 50 Threads | 100 Threads |
|------------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|
| 16 | 37 | 57 | 66 | 91 | 174 | 247 | 504 |
| 96 | 71 | 145 | 84 | 118 | 172 | 247 | 509 |
| 1,024 | 109 | 165 | 105 | 122 | 241 | 346 | 672 |
| 10,000 | 516 | 505 | 551 | 534 | 581 | 761 | 927 |
| 100,000 | 5,159 | 4,707 | 4,871 | 5,101 | 4,788 | 5,452 | 5,616 |
| 1,000,000 | 30,861 | 15,761 | 14,146 | 14,707 | 14,784 | 14,315 | 14,216 |
| 10,000,000 | 605,566 | 277,724 | 213,305 | 171,962 | 171,406 | 167,679 | 168,964 |
| 100,000,000 | 34,751,626 | 15,658,891 | 11,208,179 | 8,570,229 | 8,331,753 | 8,207,077 | 8,142,146 |

This test showcased similar results to those found in module 2 task 2, that for QuickSort, parallelism introduces significant overhead when not using a threshold. We will use both of these (mainly the serial) as a baseline to test the MPI and MPI + OpenCL implementations.

### Analysis of Raw MPI

#### Code Explanation

##### MPI Implementation

The raw MPI implementation redesigns QuickSort from a shared memory recursive algorithm which is how OpenMP was implemented, into a divide and conquer model. Instead of recursively splitting the array within a single process, MPI splits the entire array across multiple processes. This means the array is effectively carved up, and then each process in tandem works on a smaller array size. Finally, these smaller sorted arrays can be easily merged together.

```cpp
// In main()
// ... MPI Initialisation ...

int local_size = size / world_size;
int* local_array = new int[local_size];

// 1. Scatter data from root process to all processes
MPI_Scatter(array, local_size, MPI_INT, local_array, local_size, MPI_INT, 0, MPI_COMM_WORLD);

// 2. Each process sorts its local chunk of the array sequentially
quickSort(local_array, 0, local_size - 1);

// 3. Gather all sorted chunks back to the root process
MPI_Gather(local_array, local_size, MPI_INT, array, local_size, MPI_INT, 0, MPI_COMM_WORLD);

// 4. Root process performs a final merge of all sorted chunks
if (rank == 0) {
    for (int i = 1; i < world_size; i++) {
        merge(array, 0, (i * local_size) - 1, (i * local_size) + local_size - 1);
    }
}
```

**How it works:**
1.  **Distribute Data (`MPI_Scatter`)**: The root process (rank 0) divides the main array into equal-sized chunks and sends one chunk to each MPI process (including itself).
2.  **Local Sort**: Each process independently runs the standard *sequential* `quickSort` function on its own local chunk of data. This step is performed in parallel across all processes.
3.  **Collect Data (`MPI_Gather`)**: After sorting, each process sends its sorted chunk back to the root process, which assembles them into a single large array.
4.  **Final Merge**: The root process now has an array composed of several sorted sub-arrays. It performs a series of merge operations to combine these sub-arrays into one final, fully sorted array. This final step is sequential.

##### Hybrid MPI + OpenCL Implementation

This model uses the same high-level distribution strategy as the raw MPI version but replaces the local sequential sort with a GPU accelerated sort using OpenCL.

```cpp
// OpenCL Kernel to partition a sub-array on the GPU
__kernel void parallel_partition(__global int* data, ...) {
    // ... logic to compare elements and move them relative to a pivot ...
    // This partitioning is done in parallel by many GPU threads
}

// In main() after MPI_Scatter
// ...
// 2. Each process offloads its local chunk to the GPU for sorting
quickSortGPU(local_array, local_size, context, queue, kernel);
// ...
// 3. Gather sorted chunks back to root
MPI_Gather(...)
```

**How it works:**
1.  **Distribution (`MPI_Scatter`)**: This step is identical to the raw MPI implementation; the array is split and sent to all processes.
2.  **GPU Offloading**: Instead of sorting on the CPU, each MPI process copies its local sub-array from the host (CPU memory) to the device (GPU memory).
3.  **Parallel Partitioning**: An OpenCL kernel is launched on the GPU. The kernel performs a version of the partition logic in parallel.
4.  **Data Return and Gather**: The sorted sub-array is copied back from the GPU to the host. The `MPI_Gather` and final merge steps on the root process are the same as in the raw MPI implementation.

#### MPI Implementation (CPU: Intel(R) Core(TM) i5-14400F)

| Array Size | 2 Processes | 4 Processes | 8 Processes | 16 Processes |
|------------|------------------|------------------|------------------|------------------|
| 16 | 49 | 15 | 122 | 1,417 |
| 96 | 6 | 8 | 12 | 1,014 |
| 1,024 | 42 | 77 | 51 | 72 |
| 10,000 | 439 | 374 | 493 | 773 |
| 100,000 | 5,323 | 3,634 | 4,026 | 6,270 |
| 1,000,000 | 58,050 | 37,106 | 34,509 | 47,163 |
| 10,000,000 | 1,429,639 | 594,878 | 381,967 | 411,168 |
| 100,000,000 | 96,414,632 | 55,172,115 | 10,228,297 | 6,455,606 |

The MPI results follow a similar pattern to OpenMP, where the overhead of process management and data communication (scatter/gather operations) makes it inefficient for smaller arrays. However, its performance on the largest arrays is notable. For the 100,000,000-element array, the 16-process MPI implementation is the fastest of all CPU-based methods, outperforming the best OpenMP result. This suggests that the "divide and conquer" data decomposition strategy of this MPI model is highly effective for QuickSort at a massive scale, even on a single machine, as it parallelises the workload effectively once the initial overhead is overcome.

### Analysis of Hybrid MPI + OpenCL

This implementation was designed to offload the sorting of each sub array created by MPI to the GPU rather than the CPU.

#### Hybrid MPI + OpenCL Implementation (GPU: NVIDIA GeForce RTX 4060)

| Array Size | P:1 / GPU | P:2 / GPU | P:4 / GPU |
|------------|-----------------|-----------------|-----------------|
| 16 | 368 | 726 | 1,034 |
| 96 | 328 | 631 | 988 |
| 1,024 | 398 | 613 | 1,005 |
| 10,000 | 1,460 | 1,606 | 1,278 |
| 100,000 | 15,534 | 12,305 | 12,534 |
| 1,000,000 | 125,898 | 118,323 | 122,562 |
| 10,000,000 | 1,412,264 | 1,171,377 | 1,263,039 |
| 100,000,000 | 15,581,626 | 12,805,672 | 13,348,256 |

The results from the Hybrid MPI + OpenCL implementation decisively show that QuickSort is not an algorithm well-suited for GPU acceleration. In every test case, this implementation was significantly slower than its CPU only counterparts (OpenMP and MPI) and often slower than the serial version.

This poor performance is due to a fundamental mismatch between the QuickSort algorithm and GPU architecture:
1.  **The Tasks aren't Complex Enough to Benefit from the GPU:** QuickSort is dominated by data comparisons and swaps, not complex mathematical calculations. The massive overhead of transferring data to and from the GPU is not justified by the simple work being performed.
2.  **Tasks aren't Static:** The memory access patterns in QuickSort are data dependent and irregular, which is highly inefficient for GPUs that thrive on predictable, static memory access.
3.  **Constant Branching:** The algorithm's recursive nature and constant branching (if-else statements) disrupt the parallel execution flow of the GPU's processing units, causing significant performance degradation.

This stands in stark contrast to the matrix multiplication task, where the GPU's architecture was a perfect match for the problem. The complex nature of the parallelised task combined with how static the task was meant the GPU thrived. But here, the GPU is a terrible fit for this task.

## Conclusion

The optimal implementation for QuickSort is highly dependent on the scale of the problem and the hardware available.

1.  **For Small to Medium Arrays (up to ~100,000 elements): The Serial Implementation OR OpenMP implementation with a threshold:** I dind't run more tests with the threshold, but from the Module 2 task we can see that trying to parralelise small arrays is simply inefficient. It is much better to let the smaller arrays (or small sub arrays) simply run sequentially.

2.  **For Large Arrays on a Single CPU:** Both **OpenMP** and **MPI** provide significant speedups. MPI shows slightly better scalability at the highest process count for the largest array size, making it the preferred choice for massive, single-node sorting tasks. OpenMP with a threshold though would still be the preferred option.

3.  **Do NOT use a GPU.** The Hybrid MPI + OpenCL model clearly showcases that offloading QuickSort to a GPU is counterproductive. The algorithm's structure is fundamentally incompatible with the strengths of GPU architecture. If you wanted to utilise a GPU for extremely fast sorting, the best strategy would simply be to abandon quicksort altogether in favour of a different, more GPU-friendly sorting algorithm (like Radix Sort or Bitonic Sort).