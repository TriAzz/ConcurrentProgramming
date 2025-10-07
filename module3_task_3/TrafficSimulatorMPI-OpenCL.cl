//Struct to hold parsed data from a single line
typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int light_id;
    int cars;
} TrafficData;

//Helper function to convert a character to an integer
int char_to_int(char c) {
    return c - '0';
}

//OpenCL kernel to parse traffic data lines in parallel
__kernel void parse_traffic_data(
    __global const char* all_lines,
    __global const int* line_starts,
    __global TrafficData* results)
{
    //Get the unique ID for this thread, which corresponds to the line number
    int gid = get_global_id(0);

    //Determine the start and end index for the line this thread is responsible for
    int start_index = line_starts[gid];
    int end_index = line_starts[gid + 1];

    //Get a pointer to the start of the line and its length
    __global const char* line = &all_lines[start_index];
    int len = end_index - start_index;

    //Initialize variables for parsing
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, light_id = 0, cars = 0;
    int current_number = 0;
    int part = 0; //Used to track which part of the timestamp/data we are parsing

    //Manual string parsing loop for the line
    for (int i = 0; i < len; ++i) {
        char c = line[i];

        //If the character is a digit, append it to the current number
        if (c >= '0' && c <= '9') {
            current_number = current_number * 10 + char_to_int(c);
        } else {
            //If a delimiter is found, assign the parsed number to the correct variable
            if (part == 0) year = current_number;
            else if (part == 1) month = current_number;
            else if (part == 2) day = current_number;
            else if (part == 3) hour = current_number;
            else if (part == 4) minute = current_number;
            else if (part == 5) second = current_number;
            else if (part == 6) light_id = current_number;

            //Reset current number for the next part
            current_number = 0;

            //Move to the next part if a valid delimiter is found
            if (c == '-' || c == ' ' || c == ':') {
                 part++;
            }
        }
    }
    //Assign the final number (car count)
    cars = current_number;

    //Store the final parsed data into the output results buffer
    results[gid].year = year;
    results[gid].month = month;
    results[gid].day = day;
    results[gid].hour = hour;
    results[gid].light_id = light_id;
    results[gid].cars = cars;
}