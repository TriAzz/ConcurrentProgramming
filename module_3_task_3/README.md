# Traffic Control Simulator

## Solution Design: Data Structures and Thread Safety

This project evaluates different parallel programming models for a traffic control simulation. The core data structures are designed with thread safety in mind to handle concurrent operations.

*   **`queue<TrafficData>` (Bounded Buffer):** Utilised in the OpenMP model, this structure facilitates communication between producer and consumer threads. Access is managed by a mutex and synchronised with condition variables, ensuring thread-safe, blocking operations.
*   **`map<string, map<int, int>>` (Results Map):** This structure aggregates car counts for each traffic light per hour. Updates are protected by a mutex to maintain thread safety. Access remains non-blocking except during update operations.
*   **`vector<string>` (Input Data):** This holds all lines from the input file. As it is read-only after initialisation, no synchronisation mechanisms are required.
*   **`atomic<int>` (Counters):** These are employed for tracking the current line being processed and the number of active producer threads. Their atomic nature ensures thread-safe and non-blocking increments and decrements.

## Generate Data

The `GenerateData.cpp` utility is a crucial preliminary step, allowing for the creation of datasets of varying sizes. Users can specify the number of traffic lights and the duration in hours to generate simulated traffic data. This data consists of the number of cars passing through each traffic light in 5 minute intervals.

## Data Format

The generated data is stored in a plain text file, with each line representing a single 5 minute window for a specific traffic light. The format is as follows:

-   Date (e.g., 2025-01-01)
-   Time (e.g., 00:00:00)
-   Traffic light ID (integer)
-   Car count (integer)

This clear and simple format allows for easy parsing. The date and time are essential for the consumer components to aggregate data into hourly congestion reports. The traffic light ID is used to associate car counts with the correct light, and the car count itself is the primary metric for determining congestion levels.

## Sequential, OpenMP, and MPI+OpenCL simulators

This project contains the 2 previous implementations tested in Module 2, as well as the new OpenCL implementation.

*   **Sequential Simulator:** Processes the data in a single thread, taking data from the data.txt file and storing it. Then, for each hour, adds up the total amount of cars for each light and compares each one with the current top N congested lights.
*   **OpenMP Simulator:** Using OpenMP, producers read and add data to a queue, while consumers take results from the queue and sort them based on most congested lights. The buffer size and thread configuration are flexible so that you can test at varying levels.
*   **MPI+OpenCL Simulator:** Using both MPI and OpenCL, MPI is used to distribute the workload across multiple processes, which can run on different nodes. Within each MPI process, OpenCL is utilised to offload the parallel computation of car count aggregation to a GPU or other accelerator, taking advantage of its massively parallel architecture.

## Results

A shell file was created to run all 3 implementations across a wide range of entry amounts. The results are shown below:

### Sequential Results

| Total Entries | Average Time (µs) |
| :------------ | ----------------: |
| 180           |               402 |
| 4,320         |             4,362 |
| 14,400        |            15,309 |
| 57,600        |            63,188 |
| 129,600       |           132,704 |
| 230,400       |           251,350 |
| 360,000       |           403,160 |
| 518,400       |           598,637 |
| 806,400       |           898,414 |
| 1,152,000     |         1,335,904 |
| 1,944,000     |         2,077,330 |

### OpenMP Results Table

| Total Entries | 1P/1C Threads | 2P/2C Threads | 4P/4C Threads | 8P/8C Threads |
| :------------ | ------------: | ------------: | ------------: | ------------: |
| 180           |           782 |         1,091 |         1,291 |         1,649 |
| 4,320         |         9,031 |        12,908 |        15,210 |        16,013 |
| 14,400        |        26,957 |        41,256 |        47,434 |        51,547 |
| 57,600        |       100,687 |       165,746 |       194,595 |       210,599 |
| 129,600       |       197,718 |       365,001 |       389,301 |       431,250 |
| 230,400       |       345,009 |       650,466 |       706,826 |       719,195 |
| 360,000       |       508,747 |       971,682 |     1,116,401 |     1,119,432 |
| 518,400       |       781,890 |     1,398,250 |     1,568,299 |     1,672,270 |
| 806,400       |     1,243,007 |     2,211,804 |     2,468,377 |     2,652,650 |
| 1,152,000     |     1,738,753 |     3,120,860 |     3,472,557 |     3,541,217 |
| 1,944,000     |     2,600,277 |     5,175,754 |     5,886,402 |     6,020,257 |

### MPI+OpenCL Results Table

| Total Entries | 1 Processor | 2 Processors | 4 Processors | 8 Processors |
| :------------ | ----------: | -----------: | -----------: | -----------: |
| 180           |     123,130 |      163,564 |      236,065 |      422,413 |
| 4,320         |     112,862 |      157,333 |      236,707 |      419,367 |
| 14,400        |     124,584 |      168,348 |      246,012 |      454,089 |
| 57,600        |     169,664 |      190,662 |      259,725 |      415,889 |
| 129,600       |     230,467 |      222,732 |      278,429 |      439,309 |
| 230,400       |     316,629 |      284,961 |      339,227 |      474,446 |
| 360,000       |     429,955 |      369,180 |      377,043 |      526,437 |
| 518,400       |     606,995 |      470,649 |      452,800 |      556,467 |
| 806,400       |     892,820 |      630,582 |      581,395 |      718,899 |
| 1,152,000     |   1,196,561 |      888,700 |      792,582 |      837,864 |
| 1,944,000     |   1,895,617 |    1,326,458 |    1,055,688 |    1,032,900 |

## Discussion of Results

The main goal of this was to compare an MPI+OpenCL hybrid implementation against the previous 2 implementations


### Overall Performance: 

The sequential implementation provides a crucial baseline. Its performance scales linearly with the size of the input data, which is expected for a single-threaded application.

The OpenMP implementation is still much slower than the sequential implementation even at the much larger entry sizes tested this time around. I still believe there is a hypothetic point where OpenMP would become faster but it is simply too high to reasonably test to.

MPI+OpenCL however, is different. It starts off easily the worst. However, as more and more entries are added, the gap between the speed of the sequential version and the MPI+OpenCL version slowly closes, until eventually MPI+OpenCL actually becomes faster than it. This shows that, the more work that needs to be done, the less of an impact the overhead cost of setting up MPI and OpenCL is since the overhead impact is more static, and the speed benefit of parellelising is more and more prominent.

### Deep Dive into MPI+OpenCL Performance

The true strength of the MPI+OpenCL approach becomes evident as the problem size increases.

*   **Overcoming Overhead with Scale:** For small datasets, the constant overhead of setting up the MPI/OpenCL environment dominates the execution time, making it the slowest option. However, as the number of data entries grows, this initial cost becomes less and less significant over a larger workload.
*   **Scalability with Processors:** The performance of the MPI+OpenCL implementation generally improves with the addition of more processors, especially for larger datasets. For instance, with 1,944,000 entries, the execution time decreases steadily as the processor count increases from 1 to 8. This demonstrates that the workload is being effectively parallelised and distributed.
*   **The Sweet Spot of Parallelism:** There appears to be an optimal number of processors for certain data sizes. For example, with 806,400 entries, the 4 processor configuration is the fastest. This suggests that beyond a certain point, the communication overhead between MPI processes begins to diminish the returns of adding more processing units.

### Conclusion: Choosing the Right Tool for the Job

The introduction of the MPI+OpenCL implementation showcases that this task is not neccesarily one that parellelisation is bad for, but rather that there are certain forms and variations of parallelisation that are better or worse for certain tasks then others. Once you get a high enough amount of entires, MPI+OpenCL becomes the significantly best option to choose thanks to the overhead cost being fairly constant, whereas the amount of time saved by parellelising and using the GPU increases.