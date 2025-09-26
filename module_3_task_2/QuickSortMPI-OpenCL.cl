//OpenCL Kernel to partition a sub array around a pivot.
__kernel void parallel_partition(
    __global int* data,
    const int low,
    const int high,
    const int pivot,
    __global int* less_count,
    __global int* greater_count)
{
    int i = get_global_id(0) + low;

    if (i > high) return;

    if (data[i] < pivot) {
        int dest = low + atomic_inc(less_count);
        int temp = data[i];
        data[i] = data[dest];
        data[dest] = temp;
    } else if (data[i] > pivot) {
        int dest = high - atomic_inc(greater_count);
        int temp = data[i];
        data[i] = data[dest];
        data[dest] = temp;
    }
}