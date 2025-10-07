#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <chrono>
#include <algorithm>
#include "mpi.h"
#include <CL/cl.h> 

//Struct to hold parsed data. Must match OpenCL struct.
struct TrafficData {
    cl_int year, month, day, hour, light_id, cars;
};

//Formats number with commas
std::string formatWithCommas(long long number) {
    std::string str = std::to_string(number);
    std::string result = "";
    int count = 0;
    for (int i = str.length() - 1; i >= 0; i--) {
        if (count == 3) {
            result = "," + result;
            count = 0;
        }
        result = str[i] + result;
        count++;
    }
    return result;
}

//Function to load an OpenCL kernel source from a file
std::string loadKernelFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open kernel file: " << filename << std::endl;
        exit(1);
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

//Main function
int main(int argc, char** argv) {
    //MPI Initialization
    MPI_Init(&argc, &argv);
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    //User inputs
    int N = 5;
    std::vector<std::string> local_lines;
    auto start_time = MPI_Wtime();

    //Master process (rank 0) reads and distributes data
    if (world_rank == 0) {
        std::ifstream in("data.txt");
        if (!in) {
            std::cerr << "Error opening data.txt" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        std::vector<std::string> all_lines;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty()) all_lines.push_back(line);
        }
        in.close();

        //Distribute chunks of lines to all processes
        int total_lines = all_lines.size();
        int lines_per_process = total_lines / world_size;
        int remainder = total_lines % world_size;
        int current_pos = 0;
        for (int i = 0; i < world_size; ++i) {
            int chunk_size = lines_per_process + (i < remainder ? 1 : 0);
            if (i == 0) {
                local_lines.assign(all_lines.begin(), all_lines.begin() + chunk_size);
            } else {
                std::stringstream ss;
                for (int j = 0; j < chunk_size; ++j) ss << all_lines[current_pos + j] << '\n';
                std::string chunk_str = ss.str();
                MPI_Send(chunk_str.c_str(), chunk_str.length() + 1, MPI_CHAR, i, 0, MPI_COMM_WORLD);
            }
            current_pos += chunk_size;
        }
    } else {
        //Worker processes receive their chunk of lines
        MPI_Status status;
        MPI_Probe(0, 0, MPI_COMM_WORLD, &status);
        int count;
        MPI_Get_count(&status, MPI_CHAR, &count);
        std::vector<char> recv_buffer(count);
        MPI_Recv(recv_buffer.data(), count, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        std::string received_str(recv_buffer.begin(), recv_buffer.end());
        received_str.erase(std::find(received_str.begin(), received_str.end(), '\0'), received_str.end());
        std::stringstream ss(received_str);
        std::string line;
        std::vector<std::string> invalid_lines;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.find_first_not_of(" \t\r\n") != std::string::npos) {
                // Robustness: validate line format (should have at least 2 spaces for proper data format)
                if (std::count(line.begin(), line.end(), ' ') >= 2) {
                    local_lines.push_back(line);
                } else {
                    invalid_lines.push_back(line);
                }
            }
        }
        
        // Log invalid lines to file if any found
        if (!invalid_lines.empty()) {
            std::ofstream invalid_file("invalidlines.txt", std::ios::app);
            if (invalid_file) {
                invalid_file << "Process " << world_rank << " found " << invalid_lines.size() << " invalid lines:" << std::endl;
                for (const auto& invalid_line : invalid_lines) {
                    invalid_file << "  Invalid: " << invalid_line << std::endl;
                }
                invalid_file.close();
            }
        }
    }

    //All processes set up OpenCL to parse their local lines
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    cl_int err;

    //OpenCL setup
    clGetPlatformIDs(1, &platform, NULL);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    queue = clCreateCommandQueue(context, device, 0, &err);

    //Load and build kernel
    std::string kernelSource = loadKernelFromFile("TrafficSimulatorMPI-OpenCL.cl");
    const char* kernelSourceCStr = kernelSource.c_str();
    program = clCreateProgramWithSource(context, 1, &kernelSourceCStr, NULL, &err);
    clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    kernel = clCreateKernel(program, "parse_traffic_data", &err);

    std::map<std::string, std::map<int, int>> local_hour_to_light;
    if (!local_lines.empty()) {
        //Prepare data for the GPU
        std::string all_lines_str;
        std::vector<cl_int> line_starts;
        line_starts.push_back(0);
        for (const auto& line : local_lines) {
            all_lines_str += line;
            line_starts.push_back(all_lines_str.length());
        }

        //Create GPU buffers
        cl_mem in_char_buf = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, all_lines_str.length() * sizeof(char), (void*)all_lines_str.c_str(), &err);
        cl_mem in_starts_buf = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, line_starts.size() * sizeof(cl_int), line_starts.data(), &err);
        cl_mem out_struct_buf = clCreateBuffer(context, CL_MEM_WRITE_ONLY, local_lines.size() * sizeof(TrafficData), NULL, &err);

        //Set kernel arguments
        clSetKernelArg(kernel, 0, sizeof(cl_mem), &in_char_buf);
        clSetKernelArg(kernel, 1, sizeof(cl_mem), &in_starts_buf);
        clSetKernelArg(kernel, 2, sizeof(cl_mem), &out_struct_buf);

        //Execute kernel on the GPU
        size_t global_work_size = local_lines.size();
        clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);
        clFinish(queue);

        //Read results back from the GPU
        std::vector<TrafficData> results(local_lines.size());
        clEnqueueReadBuffer(queue, out_struct_buf, CL_TRUE, 0, results.size() * sizeof(TrafficData), results.data(), 0, NULL, NULL);

        //Clean up memory objects for this chunk
        clReleaseMemObject(in_char_buf);
        clReleaseMemObject(in_starts_buf);
        clReleaseMemObject(out_struct_buf);
        
        //Aggregate the parsed results on the CPU for this local chunk
        int invalid_data_count = 0;
        std::vector<std::string> invalid_entries;
        for (size_t i = 0; i < results.size(); i++) {
            const auto& data = results[i];
            // Robustness: validate parsed data fields
            if (data.year > 0 && data.month > 0 && data.month <= 12 && 
                data.day > 0 && data.day <= 31 && data.hour >= 0 && data.hour < 24 && 
                data.light_id > 0 && data.cars >= 0) {
                
                std::stringstream ss;
                ss << data.year << "-" << (data.month < 10 ? "0" : "") << data.month << "-" << (data.day < 10 ? "0" : "") << data.day << " " << (data.hour < 10 ? "0" : "") << data.hour;
                std::string hour_key = ss.str();
                local_hour_to_light[hour_key][data.light_id] += data.cars;
            } else {
                invalid_data_count++;
                // Log the original line and parsed data for debugging
                std::stringstream invalid_entry;
                invalid_entry << "Line " << (i + 1) << ": \"" << local_lines[i] << "\" -> Parsed: year=" << data.year 
                             << ", month=" << data.month << ", day=" << data.day << ", hour=" << data.hour 
                             << ", light_id=" << data.light_id << ", cars=" << data.cars;
                invalid_entries.push_back(invalid_entry.str());
            }
        }
        
        // Log invalid parsed data if any found
        if (invalid_data_count > 0) {
            std::ofstream invalid_file("invalidlines.txt", std::ios::app);
            if (invalid_file) {
                invalid_file << "Process " << world_rank << " found " << invalid_data_count << " invalid parsed data entries after OpenCL processing:" << std::endl;
                for (const auto& entry : invalid_entries) {
                    invalid_file << "  " << entry << std::endl;
                }
                invalid_file.close();
            }
        }
    }

    //Gather results from all processes to the master
    if (world_rank != 0) {
        //Workers serialize and send their local map to the master
        std::stringstream ss;
        for (const auto& hour_entry : local_hour_to_light) {
            for (const auto& light_entry : hour_entry.second) {
                ss << hour_entry.first << " " << light_entry.first << " " << light_entry.second << "\n";
            }
        }
        std::string result_str = ss.str();
        MPI_Send(result_str.c_str(), result_str.length() + 1, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
    } else {
        //Master aggregates results from all workers
        std::map<std::string, std::map<int, int>> final_hour_to_light = local_hour_to_light;
        for (int i = 1; i < world_size; ++i) {
            MPI_Status status;
            MPI_Probe(i, 1, MPI_COMM_WORLD, &status);
            int count;
            MPI_Get_count(&status, MPI_CHAR, &count);
            std::vector<char> recv_buffer(count);
            MPI_Recv(recv_buffer.data(), count, MPI_CHAR, i, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::stringstream ss(std::string(recv_buffer.begin(), recv_buffer.end()));
            std::string hour_key, date, time;
            int light_id, cars;
            while (ss >> date >> time >> light_id >> cars) {
                hour_key = date + " " + time;
                final_hour_to_light[hour_key][light_id] += cars;
            }
        }
        
        //Stop timing
        auto end_time = MPI_Wtime();
        auto elapsed_time = (end_time - start_time) * 1e6; // in microseconds

        //Output final results
        std::ofstream out("TrafficSimulatorMPI-OpenCLResults.txt");
        if (!out) std::cerr << "Error opening output file." << std::endl;
        for (auto const& [hour, lights] : final_hour_to_light) {
            std::vector<std::pair<int, int>> sorted_lights;
            for(auto const& [light_id, car_count] : lights) {
                sorted_lights.push_back({car_count, light_id});
            }
            std::sort(sorted_lights.rbegin(), sorted_lights.rend());
            out << "Top " << N << " congested lights for hour " << hour << ":" << std::endl;
            for (int i = 0; i < std::min((int)sorted_lights.size(), N); ++i) {
                out << "  Light " << sorted_lights[i].second << ": " << sorted_lights[i].first << " cars" << std::endl;
            }
            out << std::endl;
        }
        out << "Total processing time with " << world_size << " MPI processes: " << formatWithCommas(elapsed_time) << " microseconds" << std::endl;
        out.close();
        std::cout << "Results written to TrafficSimulatorMPI-OpenCLResults.txt" << std::endl;
    }

    //Final OpenCL Cleanup
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    //Finalize MPI
    MPI_Finalize();
    return 0;
}