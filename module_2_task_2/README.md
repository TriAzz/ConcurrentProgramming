# QuickSort Performance Analysis: Sequential vs OpenMP Parallel

## Code Explanation

### Partition Function

The partition function is the core of QuickSort, implementing the Lomuto partition scheme:

```cpp
int partition(int array[], int low, int high)
{
    int pivot = array[high];  // Choose last element as pivot
    int i = (low - 1);        // Index for smaller elements

    for (int j = low; j <= high - 1; j++)
    {
        if (array[j] <= pivot)  // If current element <= pivot
        {
            i++;               // Move smaller element index
            swap(array[i], array[j]);  // Swap elements
        }
    }
    swap(array[i + 1], array[high]);  // Place pivot in correct position
    return (i + 1);  // Return pivot's final position
}
```

**How it works:**
1. **Select pivot**: Last element becomes the pivot value
2. **Partition process**: Iterate through subarray, moving smaller elements left
3. **Swap operations**: Elements ≤ pivot go to left, larger go to right
4. **Pivot placement**: Final swap places pivot in its correct sorted position
5. **Return index**: Pivot's position used to split array for recursive calls

### Sequential QuickSort Implementation

The sequential version implements a standard recursive QuickSort algorithm using the Lomuto partition scheme:

```cpp
void quickSort(int array[], int low, int high)
{
    if (low < high)
    {
        int pivot = partition(array, low, high);
        quickSort(array, low, pivot - 1);      // Left subarray
        quickSort(array, pivot + 1, high);     // Right subarray
    }
}
```

### OpenMP Parallel QuickSort Implementation

The OpenMP version uses task-based parallelism with a threshold optimization:

```cpp
void quickSort(int array[], int low, int high)
{
    const int Threshold = 100;

    if (low < high)
    {
        int pivot = partition(array, low, high);

        if (high - low > Threshold)
        {
            #pragma omp task
            quickSort(array, low, pivot - 1);

            #pragma omp task
            quickSort(array, pivot + 1, high);

            #pragma omp taskwait
        }
        else
        {
            quickSort(array, low, pivot - 1);
            quickSort(array, pivot + 1, high);
        }
    }
}
```

## Performance: Sequential vs Parallel

### Base Performance Comparison

When comparing base Sequential vs Parallel (that is, comparing when all calls of the QuickSort function are either parallel or sequential), we observe the following performance results:

| Implementation | 10 | 100 | 1,000 | 10,000 | 100,000 | 1,000,000 | 10,000,000 |
|----------------|----|-----|-------|--------|---------|-----------|-------------|
| Sequential | 0 | 3 | 51 | 667 | 9,111 | 117,087 | 4,491,695 |
| Parallel (2 threads) | 73 | 149 | 567 | 4,043 | 42,892 | 497,987 | 4,890,954 |
| Parallel (6 threads) | 64 | 163 | 788 | 6,074 | 64,848 | 855,330 | 7,908,070 |
| Parallel (10 threads) | 88 | 285 | 1,013 | 7,530 | 81,825 | 1,076,317 | 8,037,309 |
| Parallel (16 threads) | 89 | 317 | 1,175 | 8,946 | 96,334 | 1,211,561 | 8,874,383 |
| Parallel (30 threads) | 155 | 522 | 1,663 | 11,004 | 109,074 | 1,156,723 | 10,869,158 |
| Parallel (50 threads) | 239 | 700 | 1,957 | 10,650 | 104,000 | 1,140,786 | 10,197,298 |
| Parallel (100 threads) | 511 | 1,061 | 2,824 | 11,683 | 102,621 | 1,244,115 | 11,382,303 |

This clearly shows us that parallelising a QuickSort algorithm results in terrible time loss due to overhead cost. This makes sense, since QuickSort results in many calls, and thus many tasks need to be created and assigned. However, there is another solution we can do. Rather than doing completely parallelisation, we can instead create a **threshold** for when parallelisation occurs. Essentially, what we will want to do, is make it so that if a subarray is smaller than a certain size, we don't create a parellel task for it and instead just do them sequentially. This means that, once we get to a point where an array is small enough that the time saved parallelising it is actually less then the time gained by overhead, we can simply not parallel that task.

### Performance of Sequential vs Parallel with 1000 Threshold

When comparing Sequential vs Parallel with a 1000 threshold (that is, subarrays with less than 1000 elements aren't parellelised), we observe the following performance results:

| Implementation | 10 | 100 | 1,000 | 10,000 | 100,000 | 1,000,000 | 10,000,000 |
|----------------|----|-----|-------|--------|---------|-----------|-------------|
| Sequential | 0 | 3 | 51 | 667 | 9,111 | 117,087 | 4,491,695 |
| Parallel (2 threads) | 61 | 74 | 155 | 660 | 7,268 | 81,435 | 3,001,758 |
| Parallel (6 threads) | 68 | 148 | 111 | 427 | 3,427 | 41,212 | 1,338,357 |
| Parallel (10 threads) | 55 | 91 | 115 | 347 | 2,600 | 31,359 | 887,761 |
| Parallel (16 threads) | 138 | 98 | 132 | 407 | 2,397 | 28,396 | 703,452 |
| Parallel (30 threads) | 192 | 172 | 174 | 449 | 2,455 | 23,800 | 592,937 |
| Parallel (50 threads) | 221 | 263 | 254 | 507 | 2,862 | 23,360 | 608,006 |
| Parallel (100 threads) | 470 | 510 | 504 | 677 | 3,493 | 24,540 | 607,312 |

These results paint a much better picture of parallelisation. We can see with a threshold, sequential does a much better job as we are able to make it so it only parallelises tasks when it provides actual benefit. For this example, parallel is worse for array sizes of 10 - 1,000 which makes complete sense. This is because they aren't being parallelised at all, but the OpenMP code to set up the tasks and parallisation still needs to be run. However for 10,000+ size arrays, there is a massive timesave that is achieved by parallelising now. 

## Performance: Parallel at Thresholds

### Table of Each Thresholds Performance

#### Threshold = 0 

| Array Size | 2 threads | 6 threads | 10 threads | 16 threads | 30 threads | 50 threads | 100 threads |
|------------|-----------|-----------|------------|------------|------------|------------|-------------|
| 10 | 73 | 64 | 88 | 89 | 155 | 239 | 511 |
| 100 | 149 | 163 | 285 | 317 | 522 | 700 | 1,061 |
| 1,000 | 567 | 788 | 1,013 | 1,175 | 1,663 | 1,957 | 2,824 |
| 10,000 | 4,043 | 6,074 | 7,530 | 8,946 | 11,004 | 10,650 | 11,683 |
| 100,000 | 42,892 | 64,848 | 81,825 | 96,334 | 109,074 | 104,000 | 102,621 |
| 1,000,000 | 497,987 | 855,330 | 1,076,317 | 1,211,561 | 1,156,723 | 1,140,786 | 1,244,115 |
| 10,000,000 | 4,890,954 | 7,908,070 | 8,037,309 | 8,874,383 | 10,869,158 | 10,197,298 | 11,382,303 |

#### Threshold = 100 

| Array Size | 2 threads | 6 threads | 10 threads | 16 threads | 30 threads | 50 threads | 100 threads |
|------------|-----------|-----------|------------|------------|------------|------------|-------------|
| 10 | 75 | 59 | 67 | 109 | 162 | 229 | 530 |
| 100 | 80 | 118 | 67 | 102 | 189 | 243 | 455 |
| 1,000 | 189 | 147 | 196 | 305 | 358 | 429 | 736 |
| 10,000 | 605 | 574 | 687 | 865 | 1,125 | 1,461 | 2,220 |
| 100,000 | 6,091 | 4,364 | 4,300 | 4,789 | 5,529 | 6,264 | 8,017 |
| 1,000,000 | 105,192 | 48,679 | 40,284 | 39,789 | 43,015 | 43,935 | 46,260 |
| 10,000,000 | 4,411,329 | 6,474,729 | 6,449,705 | 7,666,677 | 8,483,253 | 8,292,543 | 8,796,584 |

#### Threshold = 1,000 

| Array Size | 2 threads | 6 threads | 10 threads | 16 threads | 30 threads | 50 threads | 100 threads |
|------------|-----------|-----------|------------|------------|------------|------------|-------------|
| 10 | 61 | 68 | 55 | 138 | 192 | 221 | 470 |
| 100 | 74 | 148 | 91 | 98 | 172 | 263 | 510 |
| 1,000 | 155 | 111 | 115 | 132 | 174 | 254 | 504 |
| 10,000 | 660 | 427 | 347 | 407 | 449 | 507 | 677 |
| 100,000 | 7,268 | 3,427 | 2,600 | 2,397 | 2,455 | 2,862 | 3,493 |
| 1,000,000 | 81,435 | 41,212 | 31,359 | 28,396 | 23,800 | 23,360 | 24,540 |
| 10,000,000 | 3,001,758 | 1,338,357 | 887,761 | 703,452 | 592,937 | 608,006 | 607,312 |

#### Threshold = 10,000 

| Array Size | 2 threads | 6 threads | 10 threads | 16 threads | 30 threads | 50 threads | 100 threads |
|------------|-----------|-----------|------------|------------|------------|------------|-------------|
| 10 | 76 | 63 | 85 | 105 | 212 | 278 | 498 |
| 100 | 67 | 174 | 80 | 104 | 189 | 279 | 532 |
| 1,000 | 172 | 131 | 120 | 136 | 184 | 282 | 575 |
| 10,000 | 833 | 881 | 815 | 900 | 903 | 985 | 1,156 |
| 100,000 | 6,468 | 3,151 | 2,738 | 2,515 | 2,529 | 2,540 | 2,772 |
| 1,000,000 | 88,250 | 42,713 | 29,456 | 24,200 | 22,208 | 21,756 | 21,627 |
| 10,000,000 | 2,976,637 | 1,218,570 | 1,399,470 | 725,081 | 620,901 | 612,883 | 609,893 |

#### Threshold = 100,000

| Array Size | 2 threads | 6 threads | 10 threads | 16 threads | 30 threads | 50 threads | 100 threads |
|------------|------------|------------|------------|------------|------------|------------|------------|
| 10 | 72 | 65 | 71 | 93 | 169 | 250 | 483 |
| 100 | 75 | 143 | 74 | 87 | 155 | 228 | 498 |
| 1,000 | 135 | 114 | 114 | 128 | 166 | 239 | 499 |
| 10,000 | 843 | 810 | 831 | 833 | 895 | 931 | 1,118 |
| 100,000 | 9,392 | 9,277 | 9,136 | 9,502 | 9,211 | 9,377 | 9,656 |
| 1,000,000 | 76,969 | 40,014 | 30,475 | 27,384 | 26,789 | 26,196 | 27,888 |
| 10,000,000 | 2,902,152 | 1,197,543 | 1,557,684 | 2,550,635 | 2,450,232 | 2,322,918 | 861,707 |

### Threshold Comparison

I tested the OpenMP parallel code with various thresholds (0, 100, 1,000, 10,000 and 100,000). My findings were that out of the tested options, 1,000 provided the best results. Having a threshold that is too small leads to the exact same problem that occurs when you have no threshold, where overhead costs of the small arrays take up so much time that they invalidate the time saved by parallelising. However, if the threshold is too high, the opposite problem occurs where you don't save as much time compared to sequential since a large part of your code is still being done sequentially.

For QuickSort, the largest subarrays of course, take up the most time. As a result, even if the threshold is quite high, say 100,000, you actually still save quite a bit of time.

#### Performance Comparison: 100,000 Threshold vs Sequential (1,000,000 Array Size)

Focusing specifically on the 1,000,000 element array size with a 100,000 threshold, we can see the performance benefits of parallelization even with a fairly high threshold:

| Implementation | Time | Speedup vs Sequential |
|----------------|-----------|----------------------|
| Sequential | 117,087 | 1.00x |
| Parallel (2 threads) | 76,969 | 1.52x |
| Parallel (6 threads) | 40,014 | 2.93x |
| Parallel (10 threads) | 30,475 | 3.84x |
| Parallel (16 threads) | 27,384 | 4.28x |
| Parallel (30 threads) | 26,789 | 4.37x |
| Parallel (50 threads) | 26,196 | 4.47x |
| Parallel (100 threads) | 27,888 | 4.20x |

This showcases to us that the large subarrays take up a large majority of the time and as such, parallelising even just thos and keep alot of the program sequential still ends up with a large speed increase.

## Conclusion

This demonstrates that naive parallelization of QuickSort leads to significant performance issues due to overhead costs, especially for smaller arrays. However, threshold optimization provides an effective solution by processing small subarrays sequentially while parallelizing larger ones. The optimal threshold I found was 1,000, although I only tested powers of 10 so it is possible that there is a more specific optimal threshold. When using optimal thresholds, speedup was extremely significant.
