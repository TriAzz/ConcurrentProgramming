# Traffic Control Simulator

## Generate Data
The first file is `GenerateData.cpp`. This is not one of the main bits of code but it is essential for allowing you to choose variables that affect the size of the data. Specifically, the user is able to input the amount of traffic lights and the amount of hours and based on that, it will generate data for the amount of cars that pass through each traffic light every 5 minutes.

## Data Format
The generated data is written as plain text with each line representing a traffic light during a 5 minute window. 
Each line features the following information:
- Date (e.g., 2025-01-01)
- Time (e.g., 00:00:00)
- Traffic light ID (integer)
- Car count (integer)
With this format, each of the essential bits of information is easily seen and shown. Data and time are important as the consumers need to find the most congested traffic lights for each hour. Traffic light ID also ensures that consumers can tell which lights each line refer to in order to combine the results for that hour. Finally, car count is the actual value that is being counted as the goal of the program is to find the traffic lights where, for each hour window, has the highest amount of cars.

## Sequential and OpenMP Simulators
The core of the assignment is implemented in two C++ programs:
- **Sequential Simulator**: Processes the data in a single thread, taking data from the `data.txt` file and storing it. Then, for each hour, adds up the total amount of cars for each light and compares each one with the current top N congested lights.
- **OpenMP Simulator**: Using OpenMP, producers read and add data to a queue, while consumers take results from the queue and sort them based on most congested lights. The buffer size and thread configuration are flexible so that you can test at varying levels.

## Results
A combined code which ran both sequential and OpenMP methods, with the OpenMP being tested with multiple buffer sizes and thread count, had it's results written to a markdown which specifically was designed to ease comparison.

### Sequential Results
75,787 microseconds

### OpenMP Results Table
| Thread Count | Buffer Size 2 | Buffer Size 10 | Buffer Size 50 | Buffer Size 100 | Buffer Size 1000 |
|---|---|---|---|---|---|
| 2 | 479,712 | 274,294 | 233,423 | 227,219 | 210,172 |
| 5 | 551,668 | 326,623 | 273,416 | 260,001 | 240,223 |
| 10 | 595,989 | 414,072 | 355,167 | 345,785 | 367,350 |
| 50 | 656,394 | 405,568 | 308,634 | 288,654 | 270,572 |
| 100 | 643,303 | 426,540 | 318,532 | 304,118 | 261,800 |

### OpenMP Producer-Consumer Ratio Table
| Producer Threads | Consumer Threads | Avg Time (microseconds) |
|---|---|---|
| 1 | 4 | 601,407 |
| 2 | 3 | 339,978 |
| 2 | 2 | 243,385 |
| 3 | 2 | 254,615 |
| 4 | 1 | 471,652 |

## Discussion of Results
The main goal of this was to compare a sequential implemnetation of this code vs a parallel implemnetation in an effort to help demonstrate why parallelisation isn't always a net positive. 

### Overall Performance: Sequential vs. Parallel
The sequential implementation established a performance baseline, completing the task in an average of about **75,000 microseconds**. In contrast, the most optimal parallel configuration tested, that being 2 of each type of threads with a buffer size of 1000, took **220,000 microseconds** which is more than 2 times as long. As this is the best paralell achieved, this showcases that for this task parallelisation in this manner is simply not viable.

The are two reasons for this. The first, though the not as important reason, is that the base task is simply not very time consuming to do. The processing for each line of data is computationally inexpensive, primarily involving reading from a file and simple integer addition. This means that parallelising doesn't result in as much benefit compared to parellelising a longer more complex task. In addition, the overhead cost of the parallelisation is quite alot due to getting overhead from a lot of streams. Those being thread creation and management as usual, but also syncronising the queue results in threads not being able to make full use of the amount. 

### Analysis of OpenMP Configurations
While we already know that OpenMP was far slower, I compared various buffer sizes and thread counts, and well as thread ratios.

**1. Impact of Thread Count and Buffer Size:**
As seen in the "OpenMP Results Table," two clear trends emerge:
*   **Increasing the thread count leads to worse performance.** For all of the tested buffer sizes, the processing time increases as more threads are added. This is due to increased waiting time for the shared buffer. With more threads trying to access the single queue simultaneously, they essentially work sequentially which ruins the benefits of parallelisation.
*   **Increasing the buffer size leads to better performance (up to the size tested).** For any given thread count, a larger buffer leads to a faster execution time. A larger buffer allows producer threads to add more items to the queue before it becomes full. This reduces that waiting time that I mentioned above as a reason for slow performance.

**2. Impact of Producer-Consumer Ratio:**
I also tested different producer consumer thread ratios since my previous testing was all done with a 1:1 ratio but I found that 1:1 is actually the best ratio. Therefore, changing up thread ratios wouldn't have a real positive impact of the performance of OpenMP.

In conclusion, the inherent sequential nature and low computational complexity of the task, combined with the significant overhead that results from the queuing implementation means that for this program, parallelisation in this manner doesn't result in a benefit and is actually much worse.