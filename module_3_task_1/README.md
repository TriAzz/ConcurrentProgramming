# MPI Matrix Multiplication (with OpenMP and OpenCL)

This task involved redesigning the Matrix Multiplication Program that we have made in the past to work with MPI in 3 different implementations. Those implementations being just raw MPI, a hybrid of MPI with OpenMP, and finally a hybrid of MPI with OpenCL.

## Comparing MPI with Serial, Pthreads, and OpenMP

### Previous Tests

While the task was mostly about creating the 3 MPI imeplementations and comparing them, I wanted to compare them to the already existing implementations I had, and as such, I reran those implementations to get some results to use as a comparison for the MPI implementations.

#### Serial Implementation

| Matrix Size | Average Time |
|-------------|--------------|
| 128x128 | 837 |
| 256x256 | 8,058 |
| 512x512 | 106,515 |
| 768x768 | 399,755 |
| 1024x1024 | 2,758,112 |
| 1536x1536 | 11,091,820 |
| 2048x2048 | 43,798,067 |

#### Pthread Implementation

| Matrix Size | 1 Threads | 2 Threads | 4 Threads | 8 Threads | 16 Threads |
|-------------|-----------|-----------|-----------|-----------|-----------|
| 128x128 | 3,460 | 2,871 | 1,366 | 1,128 | 1,568 |
| 256x256 | 17,146 | 8,392 | 9,992 | 6,718 | 9,333 |
| 512x512 | 151,378 | 91,118 | 86,175 | 76,142 | 77,973 |
| 768x768 | 552,713 | 359,099 | 300,416 | 297,464 | 289,866 |
| 1024x1024 | 1,995,151 | 1,561,690 | 944,269 | 954,168 | 922,087 |
| 1536x1536 | 11,131,229 | 8,962,614 | 4,504,576 | 4,857,836 | 4,049,732 |
| 2048x2048 | 38,678,122 | 23,326,549 | 15,034,093 | 14,714,414 | 13,199,025 |

#### OpenMP Implementation

| Matrix Size | 1 Threads | 2 Threads | 4 Threads | 8 Threads | 16 Threads |
|-------------|-----------|-----------|-----------|-----------|-----------|
| 128x128 | 1,816 | 1,357 | 1,078 | 1,009 | 1,113 |
| 256x256 | 15,876 | 8,712 | 9,034 | 6,371 | 6,830 |
| 512x512 | 150,162 | 97,333 | 84,673 | 80,908 | 79,515 |
| 768x768 | 580,441 | 359,608 | 298,363 | 316,966 | 287,061 |
| 1024x1024 | 2,228,906 | 832,598 | 426,969 | 231,755 | 227,161 |
| 1536x1536 | 6,553,605 | 3,525,342 | 1,713,871 | 1,193,521 | 1,330,558 |
| 2048x2048 | 20,922,003 | 10,069,782 | 5,125,904 | 3,848,815 | 3,813,575 |

The tests done for the previous task established that out of these implementations, **OpenMP is the most efficient shared-memory parallelization strategy**, providing the best performance and scalability compared to Pthreads and the serial implementation. This made the OpenMP version the primary benchmark to beat.

### Analysis of Raw MPI

I created a program that utilises just raw MPI and this was my results:

#### MPI Implementation (CPU: Intel(R) Core(TM) i5-14400F  )

| Matrix Size | 1 Processes | 2 Processes | 4 Processes | 8 Processes | 16 Processes |
|-------------|---------------|---------------|---------------|---------------|---------------|
| 128x128 | 5,730 | 2,867 | 1,887 | 1,184 | 3,791 |
| 256x256 | 45,468 | 22,731 | 18,945 | 11,691 | 11,917 |
| 512x512 | 485,428 | 250,488 | 179,066 | 103,824 | 105,931 |
| 768x768 | 1,860,637 | 983,967 | 827,280 | 495,378 | 399,226 |
| 1024x1024 | 5,282,645 | 3,508,994 | 3,041,924 | 1,471,768 | 1,221,848 |
| 1536x1536 | 25,184,736 | 14,358,850 | 10,060,584 | 5,338,100 | 4,276,498 |
| 2048x2048 | 160,269,153 | 88,832,146 | 41,129,263 | 30,955,938 | 24,024,253 |

The analysis of the raw MPI implementation revealed a significant performance degradation compared to OpenMP. When being ran on a single machine, and thus not taking real advantage of MPI's distribution ability, the raw MPI implementation was consistently slower than the OpenMP implementation. This outcome was expected to me, as MPI is intended to be a distributed programming tool, and involves communication between separate nodes over a network. When run on a single machine, the program still incurs the high overhead costs associated with its design since data must be scattered to different processes and gathered back. Data is copied between the memory spaces of each simulated process, which is far less efficient than threads directly accessing shared memory.

While the MPI model did show speedup over the serial version as more processes were added, it could not overcome its inherent overhead to compete with a native shared-memory model like OpenMP in this single-node environment. As such, it is clear that MPI being used in this manner, on only one CPU, is inheriently problematic. Although it would be able to scale much better than OpenMP provided we had multiple computers to make use of.

### Comparing Hybrid MPI + OpenMP

Next, I created a program that utilised MPI to create processes, and each process was able to handle multiple threads by utilising OpenMP. Essentially, each process is running it's own version of the OpenMP implementation in a way. These were the results:

#### Hybrid MPI + OpenMP Implementation (CPU: Intel(R) Core(TM) i5-14400F  )

| Matrix Size | P:1 / T:2 | P:1 / T:4 | P:1 / T:8 | P:2 / T:2 | P:2 / T:4 | P:2 / T:8 | P:4 / T:2 | P:4 / T:4 | P:4 / T:8 |
|-------------|-------------|-------------|-------------|-------------|-------------|-------------|-------------|-------------|-------------|
| 128x128 | 2,879 | 1,846 | 1,141 | 1,592 | 1,398 | 1,251 | 1,557 | 1,283 | 1,033 |
| 256x256 | 23,879 | 17,156 | 10,009 | 16,715 | 11,287 | 9,753 | 11,769 | 9,287 | 7,726 |
| 512x512 | 249,196 | 164,231 | 90,200 | 167,927 | 102,340 | 76,250 | 104,331 | 70,986 | 65,248 |
| 768x768 | 920,177 | 604,653 | 315,448 | 653,413 | 433,195 | 259,216 | 498,349 | 305,387 | 264,075 |
| 1024x1024 | 2,411,272 | 2,563,114 | 1,690,472 | 2,680,741 | 2,444,694 | 1,609,699 | 2,701,578 | 2,001,331 | 1,629,967 |
| 1536x1536 | 10,968,466 | 7,904,584 | 8,510,015 | 9,580,530 | 9,299,287 | 8,702,129 | 10,124,091 | 9,546,629 | 8,879,493 |
| 2048x2048 | 84,023,981 | 69,309,186 | 66,828,279 | 72,468,313 | 53,907,601 | 65,123,853 | 71,202,324 | 61,719,235 | 62,232,034 |

This implementation produced the worst performance results of all parallel strategies, running significantly slower than even the raw MPI version. By incurring overhead costs both by setting up the MPI processes AND the OpenMP threads, the amount of cost far exceeded any benefit that the implementation could provide. The model creates multiple MPI processes, and within each process, it launches a new set of OpenMP threads. This results in the operating system trying to manage far more threads (e.g., 4 processes x 8 threads = 32 total threads) than there are available physical CPU cores. This leads to excessive **context switching**, where the CPU wastes most of its time swapping threads in and out of execution instead of performing useful computation. The combined overhead of MPI's message passing and OpenMP's thread management in this oversubscribed state is crippling.

While I did not test it myself, I do believe that this implemention would be quite effective if I actually had multiple computers. This is because we have already seen that OpenMP on it's own is a massive improvement. By having an MPI + OpenMP implementation where each computer only had a few OpenMP threads, you could have a situation where one computer focused on the MPI and the others focused on the actual multiplication with OpenMP. As such, it is essentially like each computer has our OpenMP implementation but for a smaller section of the Matrix.

### Comparing Hybrid MPI + OpenCL

The final test was with OpenCL, which was the only implementation that utilised my GPU **(NVIDIA GeForce RTX 4060)**, rather than my CPU **(Intel(R) Core(TM) i5-14400F)**.

#### Hybrid MPI + OpenCL Implementation (GPU: NVIDIA GeForce RTX 4060)

| Matrix Size | P:1 / GPU | P:2 / GPU | P:4 / GPU |
|-------------|-------------|-------------|-------------|
| 128x128 | 578 | 706 | 1,172 |
| 256x256 | 940 | 1,189 | 1,662 |
| 512x512 | 4,662 | 5,239 | 5,488 |
| 768x768 | 11,702 | 14,657 | 15,606 |
| 1024x1024 | 23,933 | 30,400 | 32,917 |
| 1536x1536 | 71,091 | 96,799 | 98,659 |
| 2048x2048 | 270,559 | 238,440 | 223,310 |


It was not particularly suprising to me that OpenCL implementation absolutetly smashed every other implementation when being performed on a large matrix size. This is in most part simply due to the increased power of my GPU compared to my CPU. However, the most interesting find for me, was that the OpenCL implementation was so efficient that it even outperformed the serial implementation on the smallest matrix size. This showcases that the GPU is actually so powerful that even on a small matrix size where the task isn't particularly time consuming, the GPU is still able to save more time than the overhead costs, which none of the implementations even came close to doing on the smallest matrix size.

## Conclusion

These tests, along with some personal understanding, leads to the conclusion that the best implementation depends on the context of both the task, and the available resources. 

1.  **For a Single CPU: OpenMP** is the best choice. It provides excellent performance with minimal programming overhead and avoids the unnecessary communication costs of MPI.

2.  **For Distributed CPU Clusters: Hybrid MPI + OpenMP** model is the model I believe would be the best. While it actually performed the worst for me, it's intended to have each MPI process run on a *separate machine*, using OpenMP to fully utilise the cores on that specific node. With this setup, I believe it is most likely for this implementation to be better than all of the other CPU implementations

3.  **For Maximum Performance on Suitable Problems: Hybrid MPI + OpenCL/GPU** model is the definitive winner. It correctly leverages the strengths of each component. MPI for managing work across nodes, even if only being simulated on one machine, and OpenCL for utlising the GPU for the computation heavy task that is getting parallelised.