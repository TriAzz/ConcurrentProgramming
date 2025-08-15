# Documentation for Parallel Matrix Multiplication

## Roadmap for Parallelising Matrix Multiplication Program

### Decomposing the Problem

The matrix multiplication implementation consists of two main phases:

**Phase 1: Matrix Initialisation** - Each of the matrices that are going to be multiplied together need to be populated with random values. This is achieved by simply going through each of the columns and rows of the matrix via an indented for loop. This means that it goes through each row, and for each row, it goes through the columns of that row. This is extremely easy to parallelise since this function doesn't use any variables or data that needs to worry about being shared/changed other than the matrix itself. This means that each row of the rows can simply be given to a different thread by parallelising the outer for loop.

**Phase 2: Matrix Multiplication Core Algorithm** - This is the actual multiplication of the matrices. This is achieved in a very similar manner to the initialisation, albeit with a third loop since for each cell which is found by the first two loops, it needs to iterate over the row and column to get the full matrix multiplication for that cell. This can be parallelised in the same manner as the initialisation, by giving each thread a different row to work with and dividing based on that. This is done again, by parallelising only the outer for loop.

### Parallel vs Sequential Task Classification

**Parallel Tasks:** Matrix A initialisation, Matrix B initialisation, and the matrix multiplication computation can all execute in parallel using row-based data decomposition across available threads.

**Sequential Tasks:** Memory allocation, result matrix initialisation with zeros, timing measurements, and final result verification must remain sequential to maintain data integrity and accurate performance metrics.

**Parallelisation Strategy:** The implementation employs row-based data partitioning where the N×N matrices are divided into approximately equal segments among threads using `rows_per_thread = N / num_threads` with proper handling of remainder rows. Both pthread and OpenMP approaches follow this strategy: pthread uses explicit thread creation with task structures containing row boundaries, while OpenMP uses compiler directives to automatically distribute loop iterations. This approach ensures optimal memory locality since threads access contiguous memory regions and minimises false sharing between processor caches.

## Performance Results Analysis

### Standard Test (1,000x1,000 Matrix, 10 Threads)

| Run | Serial (μs) | Pthread (μs) | OpenMP (μs) |
|-----|-------------|--------------|-------------|
| 1   | 5,704,885   | 834,798      | 737,504     |
| 2   | 8,515,123   | 800,915      | 676,854     |
| 3   | 11,588,780  | 941,789      | 648,601     |
| 4   | 12,154,821  | 884,283      | 602,650     |
| 5   | 12,221,995  | 889,262      | 654,734     |
| 6   | 12,099,394  | 945,500      | 713,856     |
| 7   | 7,551,788   | 1,084,815    | 676,337     |
| 8   | 8,647,231   | 3,006,773    | 606,913     |
| 9   | 10,647,789  | 3,186,753    | 672,086     |
| 10  | 7,164,759   | 3,065,710    | 644,079     |

| Metric | Serial | Pthread | OpenMP |
|--------|--------|---------|--------|
| **Average Performance (μs)** | 9,629,656 | 1,564,059 | 663,361 |
| **Speedup vs Serial** | 1.00x | 6.16x | 14.51x |
| **Speedup vs Pthread** | - | 1.00x | 2.36x |

#### Conclusion
When using a conservative number of threads to accomplish a long task, there is clear significant speedup by implementing a parallel approach. This makes sense, as while the serial implementation sequentially goes through each row 1 by 1, the parallel implementations are able to do multiple rows simultaneously, which would naturally speed up the process.

### Small Matrix Test (10x10 Matrix, 10 Threads)

| Run | Serial (μs) | Pthread (μs) | OpenMP (μs) |
|-----|-------------|--------------|-------------|
| 1   | 2           | 672          | 45          |
| 2   | 2           | 584          | 50          |
| 3   | 2           | 776          | 49          |
| 4   | 2           | 780          | 50          |
| 5   | 2           | 748          | 48          |
| 6   | 2           | 719          | 48          |
| 7   | 2           | 938          | 49          |
| 8   | 2           | 743          | 49          |
| 9   | 2           | 1,022        | 50          |
| 10  | 2           | 700          | 42          |

| Metric | Serial | Pthread | OpenMP |
|--------|--------|---------|--------|
| **Average Performance (μs)** | 2 | 768 | 48 |
| **Performance vs Serial** | 1.00x | 384x slower | 24x slower |
| **Speedup vs Pthread** | - | 1.00x | 16x faster |

#### Conclusion
When parallelising tasks that take an extremely short amount of time, the time required to actually run the code that performs the parallelising may end up actually taking longer on its own than just sequentially running through the program. With the times above, it isn't that Pthread or OpenMP necessarily spent more time doing the multiplication itself, but rather simply setting up the threads takes longer than just doing the multiplication.

### More Threads Test (1,000x1,000 Matrix, 1,000/10,000 Threads)

#### High Thread Count (1,000 Threads)

| Run | Pthread (μs) | OpenMP (μs) |
|-----|--------------|-------------|
| 1   | 652,124      | 497,520     |
| 2   | 717,460      | 509,550     |
| 3   | 684,756      | 516,347     |
| 4   | 713,714      | 511,559     |
| 5   | 679,259      | 497,986     |
| 6   | 647,938      | 507,551     |
| 7   | 635,732      | 504,401     |
| 8   | 651,027      | 507,686     |
| 9   | 642,339      | 518,374     |
| 10  | 647,069      | 509,403     |

| Metric | Pthread | OpenMP |
|--------|---------|--------|
| **Average Performance (μs)** | 667,141 | 508,037 |
| **Speedup vs Pthread** | 1.00x | 1.31x |

#### Extreme Thread Count (10,000 Threads)

| Run | Pthread (μs) | OpenMP (μs) |
|-----|--------------|-------------|
| 1   | 5,512,027    | 534,567     |
| 2   | 5,496,756    | 544,912     |
| 3   | 9,190,941    | 557,324     |
| 4   | 10,663,304   | 541,572     |
| 5   | 7,626,469    | 565,197     |
| 6   | 13,757,212   | 550,161     |
| 7   | 8,571,647    | 540,345     |
| 8   | 15,567,035   | 539,976     |
| 9   | 16,117,000   | 539,314     |
| 10  | 15,793,230   | 540,780     |

| Metric | Pthread | OpenMP |
|--------|---------|--------|
| **Average Performance (μs)** | 10,829,562 | 545,414 |
| **Performance vs 1,000 threads** | 16.2x slower | 1.07x slower |

#### Conclusion
This test is somewhat similar to the previous test, showing that an overabundance of threads can actually cause slower times. This is because, when you increase the amount of threads you need to initialise and sort out, you'll eventually get to the point where the time taken to prepare the threads becomes more than the time saved by parallelising the main task.

## Overall Summary

This performance evaluation demonstrates that the effectiveness of parallelising a task depends on both the size of the task and the amount of threads provided to the task. For larger tasks, paralleling provides a massive boost to speed no matter which method you go with, and OpenMP proved to be superior to Pthread due to its better handling of the multiple threads and more efficient thread usage.

However, the results also showed that parallelising doesn't always improve the speed of a task. This is because parallelising itself takes up some time, and while this time is relatively quick with a reasonable amount of threads, if the task is short then the time taken to actually parallelise the task ends up overriding the time saved by parallelising. This was demonstrated both by having a small task (10x10 matrix) and an exorbitantly large amount of threads.
