#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>
#include <algorithm>
#include <memory>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <functional>
#include <windows.h>
#include <microhttpd.h>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <omp.h>
#include <CL/cl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

using namespace std::chrono;
using namespace std;
using json = nlohmann::json;

//CONFIGURATION SECTION ----------------------------------------------------

const int SERVER_PORT = 8080;
const int MAX_CONNECTIONS = 1000;

//FILTER TYPES SECTION ----------------------------------------------------

//Enumeration of all supported image filters
enum FilterType {
    GRAYSCALE,
    SEPIA,
    INVERT,
    SATURATION,
    BRIGHTNESS,
    CONTRAST,
    BLUR,
    PIXELATE,
    EDGE_DETECTION,
    EMBOSS,
    SHARPEN,
    MEDIAN_BLUR,
    MORPHOLOGICAL_OPEN,
    BILATERAL_FILTER
};

//PROCESSING METHODS SECTION ----------------------------------------------------

//Enumeration of processing methods available to the user
enum ProcessingMethod {
    CPU_OPENMP,
    GPU_OPENCL,
    RECOMMENDED
};

//BATCH PROCESSING DATA STRUCTURES SECTION ----------------------------------------------------

//Structure to define a filter with its intensity range for batch processing
struct FilterRange {
    FilterType filter;
    float minIntensity;
    float maxIntensity;
    float increment;
    bool useSingleValue = false; // When true, only use minIntensity value
    
    vector<float> getIntensityValues() const {
        vector<float> values;
        
        if (useSingleValue || increment <= 0) {
            // Single value mode or invalid increment - only use min value
            values.push_back(minIntensity);
            return values;
        }
        
        // Range mode - generate all values from min to max
        for (float intensity = minIntensity; intensity <= maxIntensity; intensity += increment) {
            values.push_back(intensity);
        }
        return values;
    }
};

struct FilterCombination {
    vector<FilterRange> colorFilters;
    vector<FilterRange> effectFilters;
    
    size_t getTotalCombinations() const {
        // For pairwise processing: colorFilters × colorIntensities × effectFilters × effectIntensities
        size_t totalCombinations = 0;
        
        for (const auto& colorFilter : colorFilters) {
            size_t colorIntensities = colorFilter.getIntensityValues().size();
            for (const auto& effectFilter : effectFilters) {
                size_t effectIntensities = effectFilter.getIntensityValues().size();
                totalCombinations += colorIntensities * effectIntensities;
            }
        }
        
        return totalCombinations;
    }
};

struct BatchResult {
    string processedImage;
    string filePath; // For disk storage mode
    vector<pair<string, float>> appliedFilters; // filter name and intensity
    double processingTime;
    int combinationIndex;
};

// Forward declaration for disk processing fallback
pair<vector<BatchResult>, pair<int, int>> processBatchToDisk_Individual(const cv::Mat& originalImage, const FilterCombination& combinations, ProcessingMethod method, const string& outputPath);

//UTILITY FUNCTIONS SECTION ----------------------------------------------------

//Function to decode base64 string
vector<uchar> base64Decode(const string& encoded) {
    BIO *bio, *b64;
    int decodeLen = encoded.length() * 3 / 4;
    vector<uchar> buffer(decodeLen);
    
    bio = BIO_new_mem_buf(encoded.data(), -1);
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    int length = BIO_read(bio, buffer.data(), encoded.length());
    buffer.resize(length);
    
    BIO_free_all(bio);
    return buffer;
}

//Function to encode image to base64
string base64Encode(const vector<uchar>& buffer) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, buffer.data(), buffer.size());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    
    string encoded(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    
    return encoded;
}

//Function to extract base64 data from data URL
string extractBase64FromDataURL(const string& dataUrl) {
    size_t commaPos = dataUrl.find(',');
    if (commaPos != string::npos) {
        return dataUrl.substr(commaPos + 1);
    }
    return dataUrl;
}

//Function to get filter name from FilterType
string getFilterName(FilterType filter) {
    switch(filter) {
        case GRAYSCALE: return "Grayscale";
        case SEPIA: return "Sepia";
        case INVERT: return "Invert";
        case SATURATION: return "Saturation";
        case BRIGHTNESS: return "Brightness";
        case CONTRAST: return "Contrast";
        case BLUR: return "Blur";
        case PIXELATE: return "Pixelate";
        case EDGE_DETECTION: return "Edge Detection";
        case EMBOSS: return "Emboss";
        case SHARPEN: return "Sharpen";
        case MEDIAN_BLUR: return "Median Blur";
        case MORPHOLOGICAL_OPEN: return "Morphological Open";
        case BILATERAL_FILTER: return "Bilateral Filter";
        default: return "Unknown";
    }
}

//Function to get filter type from string
FilterType getFilterType(const string& filterName) {
    if (filterName == "Grayscale") return GRAYSCALE;
    if (filterName == "Sepia") return SEPIA;
    if (filterName == "Invert") return INVERT;
    if (filterName == "Saturation") return SATURATION;
    if (filterName == "Brightness") return BRIGHTNESS;
    if (filterName == "Contrast") return CONTRAST;
    if (filterName == "Blur") return BLUR;
    if (filterName == "Pixelate") return PIXELATE;
    if (filterName == "Edge Detection") return EDGE_DETECTION;
    if (filterName == "Emboss") return EMBOSS;
    if (filterName == "Sharpen") return SHARPEN;
    if (filterName == "Median Blur") return MEDIAN_BLUR;
    if (filterName == "Morphological Open") return MORPHOLOGICAL_OPEN;
    if (filterName == "Bilateral Filter") return BILATERAL_FILTER;
    return GRAYSCALE; 
}

//Function to load OpenCL kernel source from file
string loadKernelFromFile(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Warning: Could not open kernel file: " << filename << endl;
        return "";
    }
    return string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
}

//Function to get kernel name from filter type
string getKernelName(FilterType filter) {
    switch (filter) {
        case GRAYSCALE: return "grayscale_filter";
        case SEPIA: return "sepia_filter";
        case INVERT: return "invert_filter";
        case SATURATION: return "saturation_filter";
        case BRIGHTNESS: return "brightness_filter";
        case CONTRAST: return "contrast_filter";
        case BLUR: return "blur_filter";
        case PIXELATE: return "pixelate_filter";
        case EDGE_DETECTION: return "edge_detection_filter";
        case EMBOSS: return "emboss_filter";
        case SHARPEN: return "sharpen_filter";
        case MEDIAN_BLUR: return "median_blur_filter";
        case MORPHOLOGICAL_OPEN: return "morphological_open_filter";
        case BILATERAL_FILTER: return "bilateral_filter";
        default: return "grayscale_filter";
    }
}

//Function to get processing method from string
ProcessingMethod getProcessingMethod(const string& methodName) {
    if (methodName == "GPU (OpenCL)" || methodName == "gpu") return GPU_OPENCL;
    if (methodName == "Recommended") return RECOMMENDED;
    return CPU_OPENMP; // Default to CPU
}

//Function to get filter complexity score (1-5, higher = more complex)
int getFilterComplexity(FilterType filter) {
    switch(filter) {
        // Simple operations (1-2)
        case GRAYSCALE:
        case SEPIA:
        case INVERT:
        case BRIGHTNESS:
        case CONTRAST:
        case SATURATION:
            return 1;
            
        case PIXELATE:
        case BLUR:
            return 2;
            
        // Medium complexity (3)
        case EDGE_DETECTION:
        case EMBOSS:
        case SHARPEN:
            return 3;
            
        // High complexity (4-5)
        case MEDIAN_BLUR:
        case MORPHOLOGICAL_OPEN:
            return 4;
            
        case BILATERAL_FILTER:
            return 5;
            
        default:
            return 2;
    }
}

//Function to select recommended processing method
ProcessingMethod selectRecommendedMethod(const cv::Mat& image, FilterType filter) {
    size_t pixels = (size_t)image.rows * image.cols;
    size_t imageSize = pixels * 3; // RGB bytes
    int complexity = getFilterComplexity(filter);
    
    // Size thresholds (in MB)
    const size_t SMALL_IMAGE = 50 * 1024 * 1024;   // 50MB
    const size_t LARGE_IMAGE = 300 * 1024 * 1024;  // 300MB
    
    // Always use CPU for very small images
    if (imageSize < SMALL_IMAGE) {
        return CPU_OPENMP;
    }
    
    // Always use GPU for very large, complex operations
    if (imageSize > LARGE_IMAGE && complexity >= 4) {
        return GPU_OPENCL;
    }
    
    // Filter-specific recommendations
    switch(filter) {
        // GPU-favored (highly parallel)
        case EMBOSS:
        case SHARPEN:
        case BILATERAL_FILTER:
        case EDGE_DETECTION:
            return (imageSize > 150 * 1024 * 1024) ? GPU_OPENCL : CPU_OPENMP;
            
        // CPU-favored (optimized implementations)
        case MORPHOLOGICAL_OPEN:
        case BLUR:
            return CPU_OPENMP;
            
        // Size-dependent
        case MEDIAN_BLUR:
            return (imageSize > 200 * 1024 * 1024) ? GPU_OPENCL : CPU_OPENMP;
            
        // Always fast on CPU
        case GRAYSCALE:
        case SEPIA:
        case INVERT:
        case SATURATION:
        case BRIGHTNESS:
        case CONTRAST:
        case PIXELATE:
        default:
            return CPU_OPENMP;
    }
}

//OPENCL SETUP SECTION ----------------------------------------------------

struct OpenCLContext {
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    bool initialized;
    bool kernelsLoaded;
    
    OpenCLContext() : initialized(false), kernelsLoaded(false) {}
    
    ~OpenCLContext() {
        cleanup();
    }
    
    void cleanup() {
        if (initialized) {
            if (program) clReleaseProgram(program);
            if (queue) clReleaseCommandQueue(queue);
            if (context) clReleaseContext(context);
            initialized = false;
            kernelsLoaded = false;
        }
    }
};

OpenCLContext gOpenCLContext;

// Global counter for tracking GPU fallbacks to CPU during batch processing
thread_local int gGpuFallbackCount = 0;

//Function to load and compile OpenCL kernels
bool loadOpenCLKernels() {
    if (gOpenCLContext.kernelsLoaded) return true;
    
    // Load OpenCL kernels (simplified logging)
    string kernelSource = loadKernelFromFile("image_filters.cl");
    if (kernelSource.empty()) {
        cerr << "Warning: Failed to load OpenCL kernels from image_filters.cl, GPU processing unavailable" << endl;
        cerr << "Make sure image_filters.cl is in the current working directory" << endl;
        return false;
    }
    
    cl_int err;
    const char* kernelSourceCStr = kernelSource.c_str();
    
    gOpenCLContext.program = clCreateProgramWithSource(gOpenCLContext.context, 1, &kernelSourceCStr, NULL, &err);
    if (err != CL_SUCCESS) {
        cerr << "Error: Failed to create OpenCL program (error code: " << err << ")" << endl;
        return false;
    }
    
    // Compile OpenCL kernels (simplified logging)
    err = clBuildProgram(gOpenCLContext.program, 1, &gOpenCLContext.device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        cerr << "Error: Failed to build OpenCL program (error code: " << err << ")" << endl;
        // Get build log for debugging
        size_t logSize;
        clGetProgramBuildInfo(gOpenCLContext.program, gOpenCLContext.device, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
        vector<char> log(logSize);
        clGetProgramBuildInfo(gOpenCLContext.program, gOpenCLContext.device, CL_PROGRAM_BUILD_LOG, logSize, log.data(), NULL);
        cerr << "Build log:\n" << log.data() << endl;
        return false;
    }
    
    gOpenCLContext.kernelsLoaded = true;
    cout << "OpenCL kernels: COMPILED" << endl;
    return true;
}

//Function to initialize OpenCL
bool initializeOpenCL() {
    cl_int err;
    
    //Get platform
    err = clGetPlatformIDs(1, &gOpenCLContext.platform, NULL);
    if (err != CL_SUCCESS) {
        cerr << "Error: Failed to get OpenCL platform" << endl;
        return false;
    }
    
    //Get device (try GPU first, fallback to CPU)
    err = clGetDeviceIDs(gOpenCLContext.platform, CL_DEVICE_TYPE_GPU, 1, &gOpenCLContext.device, NULL);
    if (err != CL_SUCCESS) {
        err = clGetDeviceIDs(gOpenCLContext.platform, CL_DEVICE_TYPE_CPU, 1, &gOpenCLContext.device, NULL);
        if (err != CL_SUCCESS) {
            cerr << "Error: Failed to get OpenCL device" << endl;
            return false;
        }
    }
    
    //Create context
    gOpenCLContext.context = clCreateContext(NULL, 1, &gOpenCLContext.device, NULL, NULL, &err);
    if (err != CL_SUCCESS) {
        cerr << "Error: Failed to create OpenCL context" << endl;
        return false;
    }
    
    //Create command queue (using deprecated function for compatibility)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    gOpenCLContext.queue = clCreateCommandQueue(gOpenCLContext.context, gOpenCLContext.device, 0, &err);
    #pragma GCC diagnostic pop
    if (err != CL_SUCCESS) {
        cerr << "Error: Failed to create OpenCL command queue" << endl;
        return false;
    }
    
    gOpenCLContext.initialized = true;
    
    // Print GPU device information
    char deviceName[1024];
    cl_ulong globalMemSize;
    cl_ulong maxMemAllocSize;
    
    clGetDeviceInfo(gOpenCLContext.device, CL_DEVICE_NAME, sizeof(deviceName), deviceName, NULL);
    clGetDeviceInfo(gOpenCLContext.device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(globalMemSize), &globalMemSize, NULL);
    clGetDeviceInfo(gOpenCLContext.device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(maxMemAllocSize), &maxMemAllocSize, NULL);
    
    cout << "GPU Device: " << deviceName << endl;
    cout << "GPU Memory: " << (globalMemSize / 1024 / 1024) << " MB total, " 
         << (maxMemAllocSize / 1024 / 1024) << " MB max allocation" << endl;
    cout << "OpenCL initialization: SUCCESS" << endl;
    
    // Load and compile kernels
    if (!loadOpenCLKernels()) {
        cout << "Warning: Failed to load OpenCL kernels, GPU processing will fallback to CPU" << endl;
    }
    
    return true;
}

//IMAGE PROCESSING FUNCTIONS SECTION ----------------------------------------------------

//Function to apply grayscale filter using OpenMP
cv::Mat applyGrayscaleCPU(const cv::Mat& image) {
    cv::Mat result;
    cv::cvtColor(image, result, cv::COLOR_BGR2GRAY);
    cv::cvtColor(result, result, cv::COLOR_GRAY2BGR); // Convert back to 3-channel for consistency
    return result;
}

//Function to apply sepia filter using OpenMP
cv::Mat applySepiaCPU(const cv::Mat& image) {
    cv::Mat result = image.clone();
    
    #pragma omp parallel for
    for (int y = 0; y < result.rows; ++y) {
        for (int x = 0; x < result.cols; ++x) {
            cv::Vec3b& pixel = result.at<cv::Vec3b>(y, x);
            uchar b = pixel[0], g = pixel[1], r = pixel[2];
            
            pixel[0] = cv::saturate_cast<uchar>(0.272 * r + 0.534 * g + 0.131 * b); // Blue
            pixel[1] = cv::saturate_cast<uchar>(0.349 * r + 0.686 * g + 0.168 * b); // Green
            pixel[2] = cv::saturate_cast<uchar>(0.393 * r + 0.769 * g + 0.189 * b); // Red
        }
    }
    
    return result;
}

//Function to apply invert filter using OpenMP
cv::Mat applyInvertCPU(const cv::Mat& image) {
    cv::Mat result;
    cv::bitwise_not(image, result);
    return result;
}

//Function to apply saturation filter using OpenMP
cv::Mat applySaturationCPU(const cv::Mat& image, float intensity) {
    cv::Mat result;
    cv::Mat hsv;
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
    
    vector<cv::Mat> channels;
    cv::split(hsv, channels);
    
    // Adjust saturation channel (index 1)
    channels[1] *= (intensity / 100.0f);
    
    cv::merge(channels, hsv);
    cv::cvtColor(hsv, result, cv::COLOR_HSV2BGR);
    
    return result;
}

//Function to apply brightness filter using OpenMP
cv::Mat applyBrightnessCPU(const cv::Mat& image, float intensity) {
    cv::Mat result;
    float alpha = intensity / 100.0f;
    image.convertTo(result, -1, alpha, 0);
    return result;
}

//Function to apply contrast filter using OpenMP
cv::Mat applyContrastCPU(const cv::Mat& image, float intensity) {
    cv::Mat result;
    float alpha = intensity / 100.0f;
    image.convertTo(result, -1, alpha, 0);
    return result;
}

//Function to apply blur filter using OpenMP
cv::Mat applyBlurCPU(const cv::Mat& image, float intensity) {
    cv::Mat result;
    
    //Match GPU calculation: intensity is 0-100%, convert to radius (max 20 pixels)
    int blur_radius;
    if (intensity == 0.0f) {
        blur_radius = 0; //No blur at 0% intensity
    } else {
        blur_radius = max(1, min(20, (int)((intensity / 100.0f) * 20.0f)));
    }
    
    if (blur_radius == 0) {
        return image.clone();
    }
    
    //Convert radius to kernel size (must be odd)
    int kernelSize = blur_radius * 2 + 1;
    cv::GaussianBlur(image, result, cv::Size(kernelSize, kernelSize), 0);
    return result;
}

//Function to apply pixelate filter using OpenMP
cv::Mat applyPixelateCPU(const cv::Mat& image, float intensity) {
    cv::Mat result;
    
    //Match GPU calculation: intensity is 0-100%, calculate block size
    int blockSize;
    if (intensity == 0.0f) {
        blockSize = 1; //No pixelation at 0% intensity
    } else {
        //Higher intensity creates larger blocks for increased pixelation
        blockSize = max(1, (int)((intensity / 100.0f) * min((float)image.cols, (float)image.rows) / 10.0f));
    }
    
    if (blockSize == 1) {
        return image.clone();
    }
    
    // Create pixelated effect by sampling block centers
    result = image.clone();
    
    #pragma omp parallel for
    for (int y = 0; y < image.rows; y++) {
        for (int x = 0; x < image.cols; x++) {
            //Find the block center for this pixel (matching GPU logic)
            int blockX = (x / blockSize) * blockSize + blockSize / 2;
            int blockY = (y / blockSize) * blockSize + blockSize / 2;
            
            //Clamp to image boundaries
            blockX = min(blockX, image.cols - 1);
            blockY = min(blockY, image.rows - 1);
            
            //Copy the block center pixel to current position
            cv::Vec3b blockPixel = image.at<cv::Vec3b>(blockY, blockX);
            result.at<cv::Vec3b>(y, x) = blockPixel;
        }
    }
    
    return result;
}

//Function to apply edge detection (Sobel) using OpenMP
cv::Mat applyEdgeDetectionCPU(const cv::Mat& image, float intensity) {
    cv::Mat gray, result;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    
    cv::Mat grad_x, grad_y;
    cv::Sobel(gray, grad_x, CV_16S, 1, 0, 3);
    cv::Sobel(gray, grad_y, CV_16S, 0, 1, 3);
    
    cv::Mat abs_grad_x, abs_grad_y;
    cv::convertScaleAbs(grad_x, abs_grad_x);
    cv::convertScaleAbs(grad_y, abs_grad_y);
    
    cv::Mat edges;
    cv::addWeighted(abs_grad_x, 0.5, abs_grad_y, 0.5, 0, edges);
    
    // Apply intensity
    edges *= (intensity / 100.0f);
    
    cv::cvtColor(edges, result, cv::COLOR_GRAY2BGR);
    return result;
}

//Function to apply emboss filter using OpenMP
cv::Mat applyEmbossCPU(const cv::Mat& image, float intensity) {
    cv::Mat result = cv::Mat::zeros(image.size(), image.type());
    
    // Emboss kernel
    float kernel[3][3] = {
        {-2, -1, 0},
        {-1, 1, 1},
        {0, 1, 2}
    };
    
    #pragma omp parallel for
    for (int y = 1; y < image.rows - 1; y++) {
        for (int x = 1; x < image.cols - 1; x++) {
            for (int c = 0; c < 3; c++) {
                float sum = 0;
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        sum += image.at<cv::Vec3b>(y + ky, x + kx)[c] * kernel[ky + 1][kx + 1];
                    }
                }
                sum = sum * (intensity / 100.0f) + 128; // Add bias for emboss effect
                result.at<cv::Vec3b>(y, x)[c] = cv::saturate_cast<uchar>(sum);
            }
        }
    }
    
    return result;
}

//Function to apply sharpen filter using OpenMP
cv::Mat applySharpenCPU(const cv::Mat& image, float intensity) {
    cv::Mat result = cv::Mat::zeros(image.size(), image.type());
    
    // Sharpen kernel
    float strength = intensity / 100.0f;
    float kernel[3][3] = {
        {0, -strength, 0},
        {-strength, 1 + 4 * strength, -strength},
        {0, -strength, 0}
    };
    
    #pragma omp parallel for
    for (int y = 1; y < image.rows - 1; y++) {
        for (int x = 1; x < image.cols - 1; x++) {
            for (int c = 0; c < 3; c++) {
                float sum = 0;
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        sum += image.at<cv::Vec3b>(y + ky, x + kx)[c] * kernel[ky + 1][kx + 1];
                    }
                }
                result.at<cv::Vec3b>(y, x)[c] = cv::saturate_cast<uchar>(sum);
            }
        }
    }
    
    return result;
}

//Function to apply median blur using OpenMP
cv::Mat applyMedianBlurCPU(const cv::Mat& image, float intensity) {
    cv::Mat result;
    int kernelSize = max(3, (int)(intensity / 10.0f));
    if (kernelSize % 2 == 0) kernelSize++; // Ensure odd
    
    cv::medianBlur(image, result, kernelSize);
    return result;
}

//Function to apply morphological opening using OpenMP
cv::Mat applyMorphologicalOpenCPU(const cv::Mat& image, float intensity) {
    cv::Mat result;
    int kernelSize = max(3, (int)(intensity / 20.0f));
    
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(kernelSize, kernelSize));
    cv::morphologyEx(image, result, cv::MORPH_OPEN, kernel);
    
    return result;
}

//Function to apply bilateral filter using OpenMP
cv::Mat applyBilateralFilterCPU(const cv::Mat& image, float intensity) {
    cv::Mat result;
    int d = max(5, (int)(intensity / 10.0f));
    double sigmaColor = intensity * 2;
    double sigmaSpace = intensity * 2;
    
    cv::bilateralFilter(image, result, d, sigmaColor, sigmaSpace);
    return result;
}

//Function to process image using CPU (OpenMP)
cv::Mat processImageCPU(const cv::Mat& image, FilterType filter, float intensity) {
    switch (filter) {
        case GRAYSCALE:
            return applyGrayscaleCPU(image);
        case SEPIA:
            return applySepiaCPU(image);
        case INVERT:
            return applyInvertCPU(image);
        case SATURATION:
            return applySaturationCPU(image, intensity);
        case BRIGHTNESS:
            return applyBrightnessCPU(image, intensity);
        case CONTRAST:
            return applyContrastCPU(image, intensity);
        case BLUR:
            return applyBlurCPU(image, intensity);
        case PIXELATE:
            return applyPixelateCPU(image, intensity);
        case EDGE_DETECTION:
            return applyEdgeDetectionCPU(image, intensity);
        case EMBOSS:
            return applyEmbossCPU(image, intensity);
        case SHARPEN:
            return applySharpenCPU(image, intensity);
        case MEDIAN_BLUR:
            return applyMedianBlurCPU(image, intensity);
        case MORPHOLOGICAL_OPEN:
            return applyMorphologicalOpenCPU(image, intensity);
        case BILATERAL_FILTER:
            return applyBilateralFilterCPU(image, intensity);
        default:
            return image.clone();
    }
}

//Function to process image using GPU (OpenCL)
cv::Mat processImageGPU(const cv::Mat& image, FilterType filter, float intensity) {
    if (!gOpenCLContext.initialized) {
        cout << "OpenCL not initialized, falling back to CPU" << endl;
        gGpuFallbackCount++;
        return processImageCPU(image, filter, intensity);
    }
    
    if (!gOpenCLContext.kernelsLoaded) {
        cout << "OpenCL kernels not loaded, falling back to CPU" << endl;
        gGpuFallbackCount++;
        return processImageCPU(image, filter, intensity);
    }
    
    auto total_start = high_resolution_clock::now();
    cl_int err;
    
    // Convert BGR to RGB for OpenCL processing
    auto convert_start = high_resolution_clock::now();
    cv::Mat rgbImage;
    cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
    auto convert_end = high_resolution_clock::now();
    
    int width = rgbImage.cols;
    int height = rgbImage.rows;
    // Use safe arithmetic to avoid overflow
    size_t imageSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 3 * sizeof(uchar);
    
    // Check if image is too large for GPU (need 2x image size for input + output buffers)
    cl_ulong maxMemAlloc;
    clGetDeviceInfo(gOpenCLContext.device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(maxMemAlloc), &maxMemAlloc, NULL);
    
    if (imageSize * 2 > maxMemAlloc) {
        double neededMB = (imageSize * 2.0) / (1024.0 * 1024.0);
        double maxMB = static_cast<double>(maxMemAlloc) / (1024.0 * 1024.0);
        cout << "Image too large for GPU (" << std::fixed << std::setprecision(1) << neededMB << " MB needed, " 
             << std::fixed << std::setprecision(1) << maxMB << " MB max), falling back to CPU" << endl;
        gGpuFallbackCount++;
        return processImageCPU(image, filter, intensity);
    }
    
    // Create OpenCL buffers and transfer to GPU
    auto transfer_to_gpu_start = high_resolution_clock::now();
    
    // Removed verbose GPU buffer creation logs for clean output
    
    cl_mem inputBuffer = clCreateBuffer(gOpenCLContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
                                       imageSize, rgbImage.data, &err);
    if (err != CL_SUCCESS) {
        cerr << "Error: Failed to create input buffer (error code: " << err << ")" << endl;
        cerr << "Required memory: " << std::fixed << std::setprecision(1) << (imageSize / (1024.0 * 1024.0)) << " MB" << endl;
        
        // More detailed error analysis
        switch(err) {
            case CL_OUT_OF_HOST_MEMORY:
                cerr << "  -> Host (system) memory exhausted" << endl;
                break;
            case CL_OUT_OF_RESOURCES:
                cerr << "  -> GPU resources exhausted" << endl;
                break;
            case CL_MEM_OBJECT_ALLOCATION_FAILURE:
                cerr << "  -> GPU memory allocation failed" << endl;
                break;
            case CL_INVALID_BUFFER_SIZE:
                cerr << "  -> Buffer size invalid (too large)" << endl;
                break;
            default:
                cerr << "  -> Unknown OpenCL error: " << err << endl;
        }
        
        gGpuFallbackCount++;
        return processImageCPU(image, filter, intensity);
    }
    
    cl_mem outputBuffer = clCreateBuffer(gOpenCLContext.context, CL_MEM_WRITE_ONLY, imageSize, NULL, &err);
    if (err != CL_SUCCESS) {
        cerr << "Error: Failed to create output buffer (error code: " << err << ")" << endl;
        switch(err) {
            case CL_OUT_OF_HOST_MEMORY:
                cerr << "  -> Host memory exhausted" << endl;
                break;
            case CL_OUT_OF_RESOURCES:
                cerr << "  -> GPU resources exhausted" << endl;
                break;
            case CL_MEM_OBJECT_ALLOCATION_FAILURE:
                cerr << "  -> GPU memory allocation failed" << endl;
                break;
        }
        clReleaseMemObject(inputBuffer);
        gGpuFallbackCount++;
        return processImageCPU(image, filter, intensity);
    }
    auto transfer_to_gpu_end = high_resolution_clock::now();
    
    // Create kernel
    string kernelName = getKernelName(filter);
    cl_kernel kernel = clCreateKernel(gOpenCLContext.program, kernelName.c_str(), &err);
    if (err != CL_SUCCESS) {
        cerr << "Error: Failed to create kernel: " << kernelName << endl;
        clReleaseMemObject(inputBuffer);
        clReleaseMemObject(outputBuffer);
        gGpuFallbackCount++;
        return processImageCPU(image, filter, intensity);
    }
    
    // Set kernel arguments with error checking (verbose logging removed for clean output)
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &inputBuffer);
    if (err != CL_SUCCESS) {
        cerr << "Error setting kernel arg 0 (input buffer): " << err << endl;
        clReleaseKernel(kernel);
        clReleaseMemObject(inputBuffer);
        clReleaseMemObject(outputBuffer);
        gGpuFallbackCount++;
        return processImageCPU(image, filter, intensity);
    }
    
    err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &outputBuffer);
    if (err != CL_SUCCESS) {
        cerr << "Error setting kernel arg 1 (output buffer): " << err << endl;
        clReleaseKernel(kernel);
        clReleaseMemObject(inputBuffer);
        clReleaseMemObject(outputBuffer);
        gGpuFallbackCount++;
        return processImageCPU(image, filter, intensity);
    }
    
    err = clSetKernelArg(kernel, 2, sizeof(int), &width);
    if (err != CL_SUCCESS) {
        cerr << "Error setting kernel arg 2 (width): " << err << endl;
        clReleaseKernel(kernel);
        clReleaseMemObject(inputBuffer);
        clReleaseMemObject(outputBuffer);
        gGpuFallbackCount++;
        return processImageCPU(image, filter, intensity);
    }
    
    err = clSetKernelArg(kernel, 3, sizeof(int), &height);
    if (err != CL_SUCCESS) {
        cerr << "Error setting kernel arg 3 (height): " << err << endl;
        clReleaseKernel(kernel);
        clReleaseMemObject(inputBuffer);
        clReleaseMemObject(outputBuffer);
        gGpuFallbackCount++;
        return processImageCPU(image, filter, intensity);
    }
    
    // Set intensity parameter for filters that need it
    if (filter == SATURATION || filter == BRIGHTNESS || filter == CONTRAST || 
        filter == BLUR || filter == PIXELATE || filter == EDGE_DETECTION ||
        filter == EMBOSS || filter == SHARPEN || filter == MEDIAN_BLUR ||
        filter == MORPHOLOGICAL_OPEN || filter == BILATERAL_FILTER) {
        err = clSetKernelArg(kernel, 4, sizeof(float), &intensity);
        if (err != CL_SUCCESS) {
            cerr << "Error setting kernel arg 4 (intensity): " << err << endl;
            clReleaseKernel(kernel);
            clReleaseMemObject(inputBuffer);
            clReleaseMemObject(outputBuffer);
            gGpuFallbackCount++;
            return processImageCPU(image, filter, intensity);
        }
        // Intensity parameter set (verbose logging removed for clean output)
    }
    
    // Removed verbose kernel argument and memory check logs for clean output
    
    // Execute kernel with safe work group size
    auto compute_start = high_resolution_clock::now();
    size_t globalWorkSize[2] = { (size_t)width, (size_t)height };
    
    // Use NULL for local work size to let OpenCL optimize automatically
    // This prevents work group size issues that cause memory access violations
    size_t* localPtr = NULL;
    
    // Removed verbose kernel execution logs for clean output
    
    err = clEnqueueNDRangeKernel(gOpenCLContext.queue, kernel, 2, NULL, globalWorkSize, localPtr, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        cerr << "Error: Failed to execute kernel (error code: " << err << ")" << endl;
        cerr << "Kernel: " << getKernelName(filter) << endl;
        cerr << "Global work size: " << globalWorkSize[0] << "x" << globalWorkSize[1] << endl;
        cerr << "Local work size: AUTO (OpenCL optimized)" << endl;
        
        // Detailed error analysis
        switch(err) {
            case CL_OUT_OF_RESOURCES:
                cerr << "  -> GPU compute resources exhausted (reduce image size or work group)" << endl;
                break;
            case CL_OUT_OF_HOST_MEMORY:
                cerr << "  -> Host memory exhausted" << endl;
                break;
            case CL_INVALID_WORK_GROUP_SIZE:
                cerr << "  -> Work group size invalid for this kernel" << endl;
                break;
            case CL_INVALID_WORK_DIMENSION:
                cerr << "  -> Work dimension invalid" << endl;
                break;
            case CL_INVALID_GLOBAL_WORK_SIZE:
                cerr << "  -> Global work size invalid" << endl;
                break;
            default:
                cerr << "  -> Unknown kernel execution error: " << err << endl;
        }
        
        clReleaseKernel(kernel);
        clReleaseMemObject(inputBuffer);
        clReleaseMemObject(outputBuffer);
        gGpuFallbackCount++;
        return processImageCPU(image, filter, intensity);
    }
    
    // Wait for kernel completion
    clFinish(gOpenCLContext.queue);
    auto compute_end = high_resolution_clock::now();

    // Read result back from GPU
    auto transfer_from_gpu_start = high_resolution_clock::now();
    cv::Mat resultRGB(height, width, CV_8UC3);
    err = clEnqueueReadBuffer(gOpenCLContext.queue, outputBuffer, CL_TRUE, 0, imageSize, resultRGB.data, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        cerr << "Error: Failed to read result buffer (error code: " << err << ")" << endl;
        clReleaseKernel(kernel);
        clReleaseMemObject(inputBuffer);
        clReleaseMemObject(outputBuffer);
        gGpuFallbackCount++;
        return processImageCPU(image, filter, intensity);
    }
    
    auto transfer_from_gpu_end = high_resolution_clock::now();
    
    // Convert back to BGR for OpenCV
    auto convert_back_start = high_resolution_clock::now();
    cv::Mat result;
    cv::cvtColor(resultRGB, result, cv::COLOR_RGB2BGR);
    auto convert_back_end = high_resolution_clock::now();
    
    auto total_end = high_resolution_clock::now();
    
    // Removed verbose GPU timing breakdown for clean output
    
    // Cleanup with aggressive memory management
    clReleaseKernel(kernel);
    clReleaseMemObject(inputBuffer);
    clReleaseMemObject(outputBuffer);
    
    // Force GPU to complete ALL operations and flush memory
    clFinish(gOpenCLContext.queue);
    
    // Additional memory cleanup - force driver to reclaim memory
    clFlush(gOpenCLContext.queue);
    
    // GPU memory cleanup completed (verbose logging removed for clean output)
    
    return result;
}

//Function to process a single filter chain on CPU (for batch processing)
BatchResult processCPUFilterChain(const cv::Mat& originalImage,
                                 const vector<pair<FilterType, float>>& colorFilters,
                                 const vector<pair<FilterType, float>>& effectFilters,
                                 int combinationIndex) {
    auto chain_start = high_resolution_clock::now();
    
    cv::Mat processedImage = originalImage.clone();
    vector<pair<string, float>> appliedFilters;
    
    // Apply color filters sequentially
    for (const auto& filterPair : colorFilters) {
        FilterType filter = filterPair.first;
        float intensity = filterPair.second;
        
        processedImage = processImageCPU(processedImage, filter, intensity);
        
        // Record applied filter
        string filterName = "Unknown";
        switch(filter) {
            case GRAYSCALE: filterName = "Grayscale"; break;
            case SEPIA: filterName = "Sepia"; break;
            case INVERT: filterName = "Invert"; break;
            case SATURATION: filterName = "Saturation"; break;
            case BRIGHTNESS: filterName = "Brightness"; break;
            case CONTRAST: filterName = "Contrast"; break;
            case BLUR: filterName = "Blur"; break;
            case PIXELATE: filterName = "Pixelate"; break;
            case EDGE_DETECTION: filterName = "Edge Detection"; break;
            case EMBOSS: filterName = "Emboss"; break;
            case SHARPEN: filterName = "Sharpen"; break;
            case MEDIAN_BLUR: filterName = "Median Blur"; break;
            case MORPHOLOGICAL_OPEN: filterName = "Morphological Open"; break;
            case BILATERAL_FILTER: filterName = "Bilateral Filter"; break;
        }
        appliedFilters.push_back({filterName, intensity});
    }
    
    // Apply effect filters sequentially
    for (const auto& filterPair : effectFilters) {
        FilterType filter = filterPair.first;
        float intensity = filterPair.second;
        
        processedImage = processImageCPU(processedImage, filter, intensity);
        
        // Record applied filter
        string filterName = "Unknown";
        switch(filter) {
            case GRAYSCALE: filterName = "Grayscale"; break;
            case SEPIA: filterName = "Sepia"; break;
            case INVERT: filterName = "Invert"; break;
            case SATURATION: filterName = "Saturation"; break;
            case BRIGHTNESS: filterName = "Brightness"; break;
            case CONTRAST: filterName = "Contrast"; break;
            case BLUR: filterName = "Blur"; break;
            case PIXELATE: filterName = "Pixelate"; break;
            case EDGE_DETECTION: filterName = "Edge Detection"; break;
            case EMBOSS: filterName = "Emboss"; break;
            case SHARPEN: filterName = "Sharpen"; break;
            case MEDIAN_BLUR: filterName = "Median Blur"; break;
            case MORPHOLOGICAL_OPEN: filterName = "Morphological Open"; break;
            case BILATERAL_FILTER: filterName = "Bilateral Filter"; break;
        }
        appliedFilters.push_back({filterName, intensity});
    }
    
    // Encode processed image
    vector<uchar> outputBuffer;
    cv::imencode(".jpg", processedImage, outputBuffer);
    string processedBase64 = "data:image/jpeg;base64," + base64Encode(outputBuffer);
    
    auto chain_end = high_resolution_clock::now();
    double processingTime = duration_cast<microseconds>(chain_end - chain_start).count() / 1000.0;
    
    BatchResult result;
    result.processedImage = processedBase64;
    result.appliedFilters = appliedFilters;
    result.processingTime = processingTime;
    result.combinationIndex = combinationIndex;
    
    return result;
}

//Function to process multiple filter combinations to disk with pairwise combinations
//Each image gets exactly ONE color filter AND ONE effect filter (not cumulative)
pair<vector<BatchResult>, pair<int, int>> processBatchToDisk(const cv::Mat& originalImage, const FilterCombination& combinations, ProcessingMethod method, const string& outputPath) {
    vector<BatchResult> results;
    int gpuSuccessCount = 0;
    int gpuFailureCount = 0;
    const int DISK_BATCH_SIZE = 20; // Download and save results in batches of 20
    
    // Reset the global GPU fallback counter for this batch
    gGpuFallbackCount = 0;
    
    cout << "\n" << string(70, '=') << endl;
    cout << "STARTING BATCH PROCESSING TO DISK" << endl;
    cout << "Total combinations: " << combinations.getTotalCombinations() << endl;
    cout << "Disk batch size: " << DISK_BATCH_SIZE << endl;
    cout << "Output path: " << outputPath << endl;
    cout << string(70, '=') << endl;
    
    // Create output directory if it doesn't exist
    #ifdef _WIN32
        CreateDirectoryA(outputPath.c_str(), NULL);
    #else
        mkdir(outputPath.c_str(), 0755);
    #endif
    
    // Check processing method and GPU availability
    if (method == CPU_OPENMP) {
        // CPU mode selected - use CPU processing directly
        cout << "CPU processing method selected - using CPU-only disk storage" << endl;
        return processBatchToDisk_Individual(originalImage, combinations, method, outputPath);
    } else if ((method == GPU_OPENCL || method == RECOMMENDED) && gOpenCLContext.initialized && gOpenCLContext.kernelsLoaded) {
        // Use GPU-resident batch processing with periodic disk saves
        cout << "\nSTARTING GPU BATCH PROCESSING (DISK)" << endl;
        cout << "Processing method: GPU-resident batch processing" << endl;
        
        // Convert image once for GPU processing
        cv::Mat rgbImage;
        cv::cvtColor(originalImage, rgbImage, cv::COLOR_BGR2RGB);
        
        int width = rgbImage.cols;
        int height = rgbImage.rows;
        size_t imageSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 3 * sizeof(uchar);
        
        cout << "Image size: " << width << "x" << height << " (" << (imageSize/(1024*1024)) << " MB)" << endl;
        
        // Check GPU memory requirements
        cl_ulong maxMemAlloc;
        clGetDeviceInfo(gOpenCLContext.device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(maxMemAlloc), &maxMemAlloc, NULL);
        
        if (imageSize * 3 > maxMemAlloc) { 
            cout << "Image too large for GPU batch processing, falling back to individual operations" << endl;
            // Fall back to original individual processing method
            return processBatchToDisk_Individual(originalImage, combinations, method, outputPath);
        }
        cl_int err;
        
        // Create persistent GPU buffers for the entire batch
        cl_mem sourceBuffer = clCreateBuffer(gOpenCLContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
                                            imageSize, rgbImage.data, &err);
        if (err != CL_SUCCESS) {
            cout << "Failed to create source buffer, falling back to individual operations" << endl;
            return processBatchToDisk_Individual(originalImage, combinations, method, outputPath);
        }
        
        cl_mem workingBuffer = clCreateBuffer(gOpenCLContext.context, CL_MEM_READ_WRITE, imageSize, NULL, &err);
        if (err != CL_SUCCESS) {
            clReleaseMemObject(sourceBuffer);
            cout << "Failed to create working buffer, falling back to individual operations" << endl;
            return processBatchToDisk_Individual(originalImage, combinations, method, outputPath);
        }
        
        cl_mem outputBuffer = clCreateBuffer(gOpenCLContext.context, CL_MEM_WRITE_ONLY, imageSize, NULL, &err);
        if (err != CL_SUCCESS) {
            clReleaseMemObject(sourceBuffer);
            clReleaseMemObject(workingBuffer);
            cout << "Failed to create output buffer, falling back to individual operations" << endl;
            return processBatchToDisk_Individual(originalImage, combinations, method, outputPath);
        }
        
        // GPU buffers allocated successfully for disk batch processing (removed log for consistency)
        
        // Generate all intensity combinations for each filter
        vector<vector<float>> colorIntensities;
        for (const auto& filter : combinations.colorFilters) {
            vector<float> intensities = filter.getIntensityValues();
            colorIntensities.push_back(intensities);
        }
        
        vector<vector<float>> effectIntensities;
        for (const auto& filter : combinations.effectFilters) {
            vector<float> intensities = filter.getIntensityValues();
            effectIntensities.push_back(intensities);
        }
        
        auto startTime = chrono::high_resolution_clock::now();
        int combinationIndex = 0;
        vector<cv::Mat> batchResults;
        vector<vector<pair<string, float>>> batchFilterInfo;
        
        // Generate pairwise combinations: each color filter × each effect filter
        for (size_t colorIdx = 0; colorIdx < colorIntensities.size(); colorIdx++) {
            for (float colorIntensity : colorIntensities[colorIdx]) {
                FilterType colorFilter = combinations.colorFilters[colorIdx].filter;
                
                for (size_t effectIdx = 0; effectIdx < effectIntensities.size(); effectIdx++) {
                    for (float effectIntensity : effectIntensities[effectIdx]) {
                        FilterType effectFilter = combinations.effectFilters[effectIdx].filter;
                        
                        // Process combination using GPU-resident pipeline
                        vector<pair<FilterType, float>> colorCombo = {{colorFilter, colorIntensity}};
                        vector<pair<FilterType, float>> effectCombo = {{effectFilter, effectIntensity}};
                        
                        // Copy source to working buffer
                        err = clEnqueueCopyBuffer(gOpenCLContext.queue, sourceBuffer, workingBuffer, 0, 0, imageSize, 0, NULL, NULL);
                        if (err != CL_SUCCESS) {
                            cerr << "Error copying source buffer for combination " << combinationIndex << endl;
                            gpuFailureCount++;
                            continue;
                        }
                        
                        cl_mem currentInput = workingBuffer;
                        cl_mem currentOutput = outputBuffer;
                        vector<pair<string, float>> appliedFilters;
                        
                        // Apply color filter
                        string kernelName = getKernelName(colorFilter);
                        cl_kernel kernel = clCreateKernel(gOpenCLContext.program, kernelName.c_str(), &err);
                        if (err == CL_SUCCESS) {
                            clSetKernelArg(kernel, 0, sizeof(cl_mem), &currentInput);
                            clSetKernelArg(kernel, 1, sizeof(cl_mem), &currentOutput);
                            clSetKernelArg(kernel, 2, sizeof(int), &width);
                            clSetKernelArg(kernel, 3, sizeof(int), &height);
                            
                            if (colorFilter == SATURATION || colorFilter == BRIGHTNESS || colorFilter == CONTRAST || 
                                colorFilter == BLUR || colorFilter == PIXELATE || colorFilter == EDGE_DETECTION ||
                                colorFilter == EMBOSS || colorFilter == SHARPEN || colorFilter == MEDIAN_BLUR ||
                                colorFilter == MORPHOLOGICAL_OPEN || colorFilter == BILATERAL_FILTER) {
                                clSetKernelArg(kernel, 4, sizeof(float), &colorIntensity);
                            }
                            
                            size_t globalWorkSize[2] = { (size_t)width, (size_t)height };
                            err = clEnqueueNDRangeKernel(gOpenCLContext.queue, kernel, 2, NULL, globalWorkSize, NULL, 0, NULL, NULL);
                            clReleaseKernel(kernel);
                            
                            if (err == CL_SUCCESS) {
                                swap(currentInput, currentOutput);
                                appliedFilters.push_back({getFilterName(colorFilter), colorIntensity});
                            }
                        }
                        
                        // Apply effect filter
                        kernelName = getKernelName(effectFilter);
                        kernel = clCreateKernel(gOpenCLContext.program, kernelName.c_str(), &err);
                        if (err == CL_SUCCESS) {
                            clSetKernelArg(kernel, 0, sizeof(cl_mem), &currentInput);
                            clSetKernelArg(kernel, 1, sizeof(cl_mem), &currentOutput);
                            clSetKernelArg(kernel, 2, sizeof(int), &width);
                            clSetKernelArg(kernel, 3, sizeof(int), &height);
                            
                            if (effectFilter == SATURATION || effectFilter == BRIGHTNESS || effectFilter == CONTRAST || 
                                effectFilter == BLUR || effectFilter == PIXELATE || effectFilter == EDGE_DETECTION ||
                                effectFilter == EMBOSS || effectFilter == SHARPEN || effectFilter == MEDIAN_BLUR ||
                                effectFilter == MORPHOLOGICAL_OPEN || effectFilter == BILATERAL_FILTER) {
                                clSetKernelArg(kernel, 4, sizeof(float), &effectIntensity);
                            }
                            
                            size_t globalWorkSize[2] = { (size_t)width, (size_t)height };
                            err = clEnqueueNDRangeKernel(gOpenCLContext.queue, kernel, 2, NULL, globalWorkSize, NULL, 0, NULL, NULL);
                            clReleaseKernel(kernel);
                            
                            if (err == CL_SUCCESS) {
                                swap(currentInput, currentOutput);
                                appliedFilters.push_back({getFilterName(effectFilter), effectIntensity});
                                gpuSuccessCount++;
                            } else {
                                gpuFailureCount++;
                            }
                        } else {
                            gpuFailureCount++;
                        }
                        
                        // Download result from GPU
                        vector<uchar> resultData(imageSize);
                        err = clEnqueueReadBuffer(gOpenCLContext.queue, currentInput, CL_TRUE, 0, imageSize, 
                                                resultData.data(), 0, NULL, NULL);
                        
                        if (err == CL_SUCCESS) {
                            // Convert back to BGR format for OpenCV
                            cv::Mat resultMat(height, width, CV_8UC3, resultData.data());
                            cv::Mat bgrResult;
                            cv::cvtColor(resultMat, bgrResult, cv::COLOR_RGB2BGR);
                            
                            batchResults.push_back(bgrResult.clone());
                            batchFilterInfo.push_back(appliedFilters);
                        } else {
                            gpuFailureCount++;
                            cerr << "Failed to download result for combination " << combinationIndex << endl;
                        }
                        
                        combinationIndex++;
                        
                        // Save batch to disk when batch size reached or when we're done
                        bool isLastCombination = (colorIdx == colorIntensities.size() - 1) && 
                                               (colorIntensity == colorIntensities[colorIdx].back()) &&
                                               (effectIdx == effectIntensities.size() - 1) && 
                                               (effectIntensity == effectIntensities[effectIdx].back());
                        
                        if (batchResults.size() >= DISK_BATCH_SIZE || isLastCombination) {
                            // Save batch (removed verbose logging for consistency)
                            for (size_t i = 0; i < batchResults.size(); i++) {
                                string filename = outputPath + "/batch_" + to_string(combinationIndex - batchResults.size() + i + 1);
                                for (const auto& filter : batchFilterInfo[i]) {
                                    filename += "_" + filter.first + to_string((int)(filter.second * 100));
                                }
                                filename += ".jpg";
                                
                                cv::imwrite(filename, batchResults[i]);
                                
                                // Create result entry (filename only, no base64 data for disk mode)
                                BatchResult result;
                                result.processedImage = ""; // No base64 data for disk mode
                                result.filePath = filename;
                                // Calculate timing: use elapsed time since start, divided by images processed so far
                                auto currentTime = chrono::high_resolution_clock::now();
                                double elapsedTime = chrono::duration<double, milli>(currentTime - startTime).count();
                                result.processingTime = elapsedTime / combinationIndex; // Average time per combination processed so far 
                                result.combinationIndex = combinationIndex - batchResults.size() + i;
                                result.appliedFilters.clear();
                                for (const auto& filter : batchFilterInfo[i]) {
                                    result.appliedFilters.push_back({filter.first, filter.second});
                                }
                                results.push_back(result);
                            }
                            
                            // Batch saved successfully (removed verbose logging for consistency)
                            batchResults.clear();
                            batchFilterInfo.clear();
                        }
                    }
                }
            }
        }
        
        // Cleanup GPU resources
        clReleaseMemObject(sourceBuffer);
        clReleaseMemObject(workingBuffer);
        clReleaseMemObject(outputBuffer);
        
        auto endTime = chrono::high_resolution_clock::now();
        double totalTime = chrono::duration<double, milli>(endTime - startTime).count();
        
        cout << "\nGPU BATCH PROCESSING (DISK) COMPLETED!" << endl;
        cout << "Total combinations processed: " << results.size() << endl;
        cout << "Total processing time: " << totalTime << "ms" << endl;
        cout << "Average time per combination: " << (results.empty() ? 0.0 : totalTime / results.size()) << "ms" << endl;
        cout << "Results stored: " << results.size() << " files in " << outputPath << "/ directory" << endl;
        cout << "GPU operations successful: " << gpuSuccessCount << endl;
        cout << "GPU operations failed: " << gpuFailureCount << endl;
        cout << string(70, '-') << endl;
        
        return make_pair(results, make_pair(gpuSuccessCount, gpuFailureCount));
        
    } else {
        // Use individual operations (either GPU not available or different method selected)
        if (method == CPU_OPENMP) {
            cout << "CPU method selected - using individual CPU operations" << endl;
        } else {
            cout << "GPU not available for optimized batch processing, using individual operations" << endl;
        }
        return processBatchToDisk_Individual(originalImage, combinations, method, outputPath);
    }
}

// Fallback function for individual GPU operations (original implementation)
pair<vector<BatchResult>, pair<int, int>> processBatchToDisk_Individual(const cv::Mat& originalImage, const FilterCombination& combinations, ProcessingMethod method, const string& outputPath) {
    vector<BatchResult> results;
    int gpuSuccessCount = 0;
    int gpuFailureCount = 0;
    
    if (method == CPU_OPENMP) {
        cout << "\nSTARTING CPU BATCH PROCESSING (DISK)" << endl;
        cout << "Processing method: CPU-only operations" << endl;
    } else {
        cout << "\nSTARTING GPU BATCH PROCESSING (DISK - FALLBACK)" << endl;
        cout << "Processing method: Individual GPU operations (fallback mode)" << endl;
    }
    cout << "Image size: " << originalImage.cols << "x" << originalImage.rows << " (" 
         << (originalImage.total() * originalImage.elemSize() / (1024*1024)) << " MB)" << endl;
    
    // Generate all intensity combinations for each filter
    vector<vector<float>> colorIntensities;
    for (const auto& filter : combinations.colorFilters) {
        colorIntensities.push_back(filter.getIntensityValues());
    }
    
    vector<vector<float>> effectIntensities;
    for (const auto& filter : combinations.effectFilters) {
        effectIntensities.push_back(filter.getIntensityValues());
    }
    
    auto startTime = chrono::high_resolution_clock::now();
    int combinationIndex = 0;
    
    // Generate pairwise combinations using individual processing
    for (size_t colorIdx = 0; colorIdx < combinations.colorFilters.size(); colorIdx++) {
        FilterType colorFilter = combinations.colorFilters[colorIdx].filter;
        
        for (float colorIntensity : colorIntensities[colorIdx]) {
            for (size_t effectIdx = 0; effectIdx < combinations.effectFilters.size(); effectIdx++) {
                FilterType effectFilter = combinations.effectFilters[effectIdx].filter;
                
                for (float effectIntensity : effectIntensities[effectIdx]) {
                    auto procStartTime = chrono::high_resolution_clock::now();
                    
                    // Start with fresh original image for each combination
                    cv::Mat processedImage = originalImage.clone();
                    vector<pair<string, float>> appliedFilters;
                    
                    // Apply ONE color filter - ALWAYS use CPU for individual mode
                    processedImage = processImageCPU(processedImage, colorFilter, colorIntensity);
                    appliedFilters.push_back(make_pair(getFilterName(colorFilter), colorIntensity));
                    
                    // Apply ONE effect filter to the result - ALWAYS use CPU for individual mode
                    processedImage = processImageCPU(processedImage, effectFilter, effectIntensity);
                    appliedFilters.push_back(make_pair(getFilterName(effectFilter), effectIntensity));
                    
                    // Generate filename with applied filters
                    string filename = outputPath + "/batch_" + to_string(combinationIndex + 1);
                    for (const auto& filter : appliedFilters) {
                        filename += "_" + filter.first + to_string((int)(filter.second * 100));
                    }
                    filename += ".jpg";
                    
                    // Save processed image to disk
                    cv::imwrite(filename, processedImage);
                    
                    auto procEndTime = chrono::high_resolution_clock::now();
                    double processingTime = chrono::duration<double, milli>(procEndTime - procStartTime).count();
                    
                    // Create result entry (filename only, no base64 data for disk mode)
                    BatchResult result;
                    result.processedImage = ""; // No base64 data for disk mode
                    result.filePath = filename;
                    result.processingTime = processingTime;
                    result.combinationIndex = combinationIndex;
                    result.appliedFilters.clear();
                    for (const auto& filter : appliedFilters) {
                        result.appliedFilters.push_back(make_pair(filter.first, filter.second));
                    }
                    results.push_back(result);
                    
                    combinationIndex++;
                }
            }
        }
    }
    
    auto endTime = chrono::high_resolution_clock::now();
    double totalTime = chrono::duration<double, milli>(endTime - startTime).count();
    
    if (method == CPU_OPENMP) {
        cout << "\nCPU BATCH PROCESSING (DISK) COMPLETED!" << endl;
    } else {
        cout << "\nGPU BATCH PROCESSING (DISK - FALLBACK) COMPLETED!" << endl;
    }
    cout << "Total combinations processed: " << results.size() << endl;
    cout << "Total processing time: " << totalTime << "ms" << endl;
    cout << "Average time per combination: " << (results.empty() ? 0.0 : totalTime / results.size()) << "ms" << endl;
    cout << "Results stored: " << results.size() << " files in " << "image_output/ directory" << endl;
    if (method != CPU_OPENMP) {
        cout << "GPU operations successful: " << gpuSuccessCount << endl;
        cout << "GPU operations failed: " << gpuFailureCount << endl;
    }
    cout << string(70, '-') << endl;
    
    return make_pair(results, make_pair(gpuSuccessCount, gpuFailureCount));
}

//Function to process multiple filter combinations on CPU
vector<BatchResult> processBatchCPU(const cv::Mat& originalImage, const FilterCombination& combinations, ProcessingMethod method) {
    vector<BatchResult> results;
    
    cout << "\n" << string(70, '=') << endl;
    cout << "STARTING CPU BATCH PROCESSING (MEMORY)" << endl;
    cout << "Total combinations: " << combinations.getTotalCombinations() << endl;
    cout << "Image size: " << originalImage.cols << "x" << originalImage.rows << " (" 
         << (originalImage.total() * originalImage.elemSize() / (1024*1024)) << " MB)" << endl;
    cout << string(70, '=') << endl;
    
    // Generate all combinations using CPU processing (PAIRWISE: 1 color + 1 effect per image)
    vector<vector<float>> colorIntensities;
    for (const auto& filter : combinations.colorFilters) {
        colorIntensities.push_back(filter.getIntensityValues());
    }
    
    vector<vector<float>> effectIntensities;
    for (const auto& filter : combinations.effectFilters) {
        effectIntensities.push_back(filter.getIntensityValues());
    }
    
    int combinationIndex = 0;
    auto batch_start = high_resolution_clock::now();
    
    // Generate pairwise combinations: each color filter × each effect filter
    // Each image gets exactly ONE color filter AND ONE effect filter
    for (size_t colorIdx = 0; colorIdx < colorIntensities.size(); colorIdx++) {
        for (float colorIntensity : colorIntensities[colorIdx]) {
            FilterType colorFilter = combinations.colorFilters[colorIdx].filter;
            
            for (size_t effectIdx = 0; effectIdx < effectIntensities.size(); effectIdx++) {
                for (float effectIntensity : effectIntensities[effectIdx]) {
                    FilterType effectFilter = combinations.effectFilters[effectIdx].filter;
                    
                    // Create single-filter combinations for pairwise processing
                    vector<pair<FilterType, float>> colorCombo = {{colorFilter, colorIntensity}};
                    vector<pair<FilterType, float>> effectCombo = {{effectFilter, effectIntensity}};
                    
                    auto result = processCPUFilterChain(originalImage, colorCombo, effectCombo, combinationIndex++);
                    results.push_back(result);
                }
            }
        }
    }
    
    auto batch_end = high_resolution_clock::now();
    double totalTime = duration_cast<microseconds>(batch_end - batch_start).count() / 1000.0;
    
    cout << "\nCPU BATCH PROCESSING (MEMORY) COMPLETED!" << endl;
    cout << "Total combinations processed: " << results.size() << endl;
    cout << "Total processing time: " << totalTime << "ms" << endl;
    cout << "Average time per combination: " << (totalTime / results.size()) << "ms" << endl;
    cout << "Results stored: In memory (base64 encoded)" << endl;
    cout << string(70, '-') << endl;
    
    return results;
}

//Function to process a single filter chain on GPU (for batch processing)
BatchResult processGPUFilterChain(cl_mem sourceBuffer, cl_mem workingBuffer, cl_mem outputBuffer,
                                 const vector<pair<FilterType, float>>& colorFilters,
                                 const vector<pair<FilterType, float>>& effectFilters,
                                 int width, int height, int combinationIndex) {
    auto chain_start = high_resolution_clock::now();
    cl_int err;
    size_t imageSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 3 * sizeof(uchar);
    
    // Copy source to working buffer
    err = clEnqueueCopyBuffer(gOpenCLContext.queue, sourceBuffer, workingBuffer, 0, 0, imageSize, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        cerr << "Error copying source buffer" << endl;
    }
    
    cl_mem currentInput = workingBuffer;
    cl_mem currentOutput = outputBuffer;
    
    vector<pair<string, float>> appliedFilters;
    
    // Apply color filters sequentially
    for (const auto& filterPair : colorFilters) {
        FilterType filter = filterPair.first;
        float intensity = filterPair.second;
        
        string kernelName = getKernelName(filter);
        cl_kernel kernel = clCreateKernel(gOpenCLContext.program, kernelName.c_str(), &err);
        if (err != CL_SUCCESS) continue;
        
        clSetKernelArg(kernel, 0, sizeof(cl_mem), &currentInput);
        clSetKernelArg(kernel, 1, sizeof(cl_mem), &currentOutput);
        clSetKernelArg(kernel, 2, sizeof(int), &width);
        clSetKernelArg(kernel, 3, sizeof(int), &height);
        
        if (filter == SATURATION || filter == BRIGHTNESS || filter == CONTRAST || 
            filter == BLUR || filter == PIXELATE || filter == EDGE_DETECTION ||
            filter == EMBOSS || filter == SHARPEN || filter == MEDIAN_BLUR ||
            filter == MORPHOLOGICAL_OPEN || filter == BILATERAL_FILTER) {
            clSetKernelArg(kernel, 4, sizeof(float), &intensity);
        }
        
        size_t globalWorkSize[2] = { (size_t)width, (size_t)height };
        err = clEnqueueNDRangeKernel(gOpenCLContext.queue, kernel, 2, NULL, globalWorkSize, NULL, 0, NULL, NULL);
        
        clReleaseKernel(kernel);
        
        // Swap buffers for next filter
        swap(currentInput, currentOutput);
        
        // Record applied filter
        string filterName = "Unknown";
        switch(filter) {
            case GRAYSCALE: filterName = "Grayscale"; break;
            case SEPIA: filterName = "Sepia"; break;
            case INVERT: filterName = "Invert"; break;
            case SATURATION: filterName = "Saturation"; break;
            case BRIGHTNESS: filterName = "Brightness"; break;
            case CONTRAST: filterName = "Contrast"; break;
            case BLUR: filterName = "Blur"; break;
            case PIXELATE: filterName = "Pixelate"; break;
            case EDGE_DETECTION: filterName = "Edge Detection"; break;
            case EMBOSS: filterName = "Emboss"; break;
            case SHARPEN: filterName = "Sharpen"; break;
            case MEDIAN_BLUR: filterName = "Median Blur"; break;
            case MORPHOLOGICAL_OPEN: filterName = "Morphological Open"; break;
            case BILATERAL_FILTER: filterName = "Bilateral Filter"; break;
        }
        appliedFilters.push_back({filterName, intensity});
    }
    
    // Apply effect filters sequentially
    for (const auto& filterPair : effectFilters) {
        FilterType filter = filterPair.first;
        float intensity = filterPair.second;
        
        string kernelName = getKernelName(filter);
        cl_kernel kernel = clCreateKernel(gOpenCLContext.program, kernelName.c_str(), &err);
        if (err != CL_SUCCESS) continue;
        
        clSetKernelArg(kernel, 0, sizeof(cl_mem), &currentInput);
        clSetKernelArg(kernel, 1, sizeof(cl_mem), &currentOutput);
        clSetKernelArg(kernel, 2, sizeof(int), &width);
        clSetKernelArg(kernel, 3, sizeof(int), &height);
        
        if (filter == SATURATION || filter == BRIGHTNESS || filter == CONTRAST || 
            filter == BLUR || filter == PIXELATE || filter == EDGE_DETECTION ||
            filter == EMBOSS || filter == SHARPEN || filter == MEDIAN_BLUR ||
            filter == MORPHOLOGICAL_OPEN || filter == BILATERAL_FILTER) {
            clSetKernelArg(kernel, 4, sizeof(float), &intensity);
        }
        
        size_t globalWorkSize[2] = { (size_t)width, (size_t)height };
        err = clEnqueueNDRangeKernel(gOpenCLContext.queue, kernel, 2, NULL, globalWorkSize, NULL, 0, NULL, NULL);
        
        clReleaseKernel(kernel);
        
        // Swap buffers for next filter
        swap(currentInput, currentOutput);
        
        // Record applied filter
        string filterName = "Unknown";
        switch(filter) {
            case GRAYSCALE: filterName = "Grayscale"; break;
            case SEPIA: filterName = "Sepia"; break;
            case INVERT: filterName = "Invert"; break;
            case SATURATION: filterName = "Saturation"; break;
            case BRIGHTNESS: filterName = "Brightness"; break;
            case CONTRAST: filterName = "Contrast"; break;
            case BLUR: filterName = "Blur"; break;
            case PIXELATE: filterName = "Pixelate"; break;
            case EDGE_DETECTION: filterName = "Edge Detection"; break;
            case EMBOSS: filterName = "Emboss"; break;
            case SHARPEN: filterName = "Sharpen"; break;
            case MEDIAN_BLUR: filterName = "Median Blur"; break;
            case MORPHOLOGICAL_OPEN: filterName = "Morphological Open"; break;
            case BILATERAL_FILTER: filterName = "Bilateral Filter"; break;
        }
        appliedFilters.push_back({filterName, intensity});
    }
    
    // Wait for all operations to complete
    clFinish(gOpenCLContext.queue);
    
    // Read final result (currentInput has the final result due to buffer swapping)
    cv::Mat resultRGB(height, width, CV_8UC3);
    err = clEnqueueReadBuffer(gOpenCLContext.queue, currentInput, CL_TRUE, 0, imageSize, resultRGB.data, 0, NULL, NULL);
    
    // Convert back to BGR and encode
    cv::Mat resultBGR;
    cv::cvtColor(resultRGB, resultBGR, cv::COLOR_RGB2BGR);
    
    vector<uchar> outputBuffer;
    cv::imencode(".jpg", resultBGR, outputBuffer);
    string processedBase64 = "data:image/jpeg;base64," + base64Encode(outputBuffer);
    
    auto chain_end = high_resolution_clock::now();
    double processingTime = duration_cast<microseconds>(chain_end - chain_start).count() / 1000.0;
    
    BatchResult result;
    result.processedImage = processedBase64;
    result.appliedFilters = appliedFilters;
    result.processingTime = processingTime;
    result.combinationIndex = combinationIndex;
    
    return result;
}

//Function to process multiple filter combinations efficiently on GPU
vector<BatchResult> processBatchGPU(const cv::Mat& originalImage, const FilterCombination& combinations, ProcessingMethod method) {
    vector<BatchResult> results;
    
    if (!gOpenCLContext.initialized || !gOpenCLContext.kernelsLoaded) {
        cout << "GPU not available for batch processing, falling back to CPU" << endl;
        return processBatchCPU(originalImage, combinations, method);
    }
    
    auto batch_start = high_resolution_clock::now();
    
    // Convert image once for GPU processing
    cv::Mat rgbImage;
    cv::cvtColor(originalImage, rgbImage, cv::COLOR_BGR2RGB);
    
    int width = rgbImage.cols;
    int height = rgbImage.rows;
    size_t imageSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 3 * sizeof(uchar);
    
    // Check GPU memory requirements
    cl_ulong maxMemAlloc;
    clGetDeviceInfo(gOpenCLContext.device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(maxMemAlloc), &maxMemAlloc, NULL);
    
    if (imageSize * 3 > maxMemAlloc) { // Need 3 buffers: input, working, output
        cout << "Image too large for GPU batch processing, falling back to CPU" << endl;
        return processBatchCPU(originalImage, combinations, method);
    }
    
    cout << "\n" << string(70, '=') << endl;
    cout << "STARTING GPU BATCH PROCESSING (MEMORY)" << endl;
    cout << "Total combinations: " << combinations.getTotalCombinations() << endl;
    cout << "Image size: " << width << "x" << height << " (" << (imageSize/(1024*1024)) << " MB)" << endl;
    cout << string(70, '=') << endl;
    
    cl_int err;
    
    // Create persistent GPU buffers for the entire batch
    cl_mem sourceBuffer = clCreateBuffer(gOpenCLContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
                                        imageSize, rgbImage.data, &err);
    if (err != CL_SUCCESS) {
        cout << "Failed to create source buffer, falling back to CPU" << endl;
        return processBatchCPU(originalImage, combinations, method);
    }
    
    cl_mem workingBuffer = clCreateBuffer(gOpenCLContext.context, CL_MEM_READ_WRITE, imageSize, NULL, &err);
    if (err != CL_SUCCESS) {
        clReleaseMemObject(sourceBuffer);
        cout << "Failed to create working buffer, falling back to CPU" << endl;
        return processBatchCPU(originalImage, combinations, method);
    }
    
    cl_mem outputBuffer = clCreateBuffer(gOpenCLContext.context, CL_MEM_WRITE_ONLY, imageSize, NULL, &err);
    if (err != CL_SUCCESS) {
        clReleaseMemObject(sourceBuffer);
        clReleaseMemObject(workingBuffer);
        cout << "Failed to create output buffer, falling back to CPU" << endl;
        return processBatchCPU(originalImage, combinations, method);
    }
    
    // GPU buffers allocated successfully (removed log for consistency)
    
    // Generate all filter combinations (PAIRWISE: 1 color + 1 effect per image)
    vector<vector<float>> colorIntensities;
    for (const auto& filter : combinations.colorFilters) {
        colorIntensities.push_back(filter.getIntensityValues());
    }
    
    vector<vector<float>> effectIntensities;
    for (const auto& filter : combinations.effectFilters) {
        effectIntensities.push_back(filter.getIntensityValues());
    }
    
    int combinationIndex = 0;
    
    // Generate pairwise combinations: each color filter × each effect filter
    // Each image gets exactly ONE color filter AND ONE effect filter
    for (size_t colorIdx = 0; colorIdx < colorIntensities.size(); colorIdx++) {
        for (float colorIntensity : colorIntensities[colorIdx]) {
            FilterType colorFilter = combinations.colorFilters[colorIdx].filter;
            
            for (size_t effectIdx = 0; effectIdx < effectIntensities.size(); effectIdx++) {
                for (float effectIntensity : effectIntensities[effectIdx]) {
                    FilterType effectFilter = combinations.effectFilters[effectIdx].filter;
                    
                    // Create single-filter combinations for pairwise processing
                    vector<pair<FilterType, float>> colorCombo = {{colorFilter, colorIntensity}};
                    vector<pair<FilterType, float>> effectCombo = {{effectFilter, effectIntensity}};
                    
                    auto result = processGPUFilterChain(sourceBuffer, workingBuffer, outputBuffer, 
                                                      colorCombo, effectCombo, width, height, combinationIndex++);
                    results.push_back(result);
                }
            }
        }
    }
    
    // Cleanup GPU resources
    clReleaseMemObject(sourceBuffer);
    clReleaseMemObject(workingBuffer);
    clReleaseMemObject(outputBuffer);
    
    auto batch_end = high_resolution_clock::now();
    double totalTime = duration_cast<microseconds>(batch_end - batch_start).count() / 1000.0;
    
    cout << "\nGPU BATCH PROCESSING (MEMORY) COMPLETED!" << endl;
    cout << "Total combinations processed: " << results.size() << endl;
    cout << "Total processing time: " << totalTime << "ms" << endl;
    cout << "Average time per combination: " << (totalTime / results.size()) << "ms" << endl;
    cout << "Results stored: In memory (base64 encoded)" << endl;
    cout << string(70, '-') << endl;
    
    return results;
}

//MAIN PROCESSING FUNCTION SECTION ----------------------------------------------------

//Forward declarations for batch processing
vector<BatchResult> processBatchCPU(const cv::Mat& originalImage, const FilterCombination& combinations, ProcessingMethod method);
BatchResult processCPUFilterChain(const cv::Mat& originalImage, const vector<pair<FilterType, float>>& colorFilters, const vector<pair<FilterType, float>>& effectFilters, int combinationIndex);

//Function to process image with specified filter and method
pair<cv::Mat, double> processImage(const cv::Mat& image, FilterType filter, float intensity, ProcessingMethod method) {
    auto start = high_resolution_clock::now();
    
    // Get filter name for logging
    string filterName = "Unknown";
    switch(filter) {
        case GRAYSCALE: filterName = "Grayscale"; break;
        case SEPIA: filterName = "Sepia"; break;
        case INVERT: filterName = "Invert"; break;
        case SATURATION: filterName = "Saturation"; break;
        case BLUR: filterName = "Blur"; break;
        case PIXELATE: filterName = "Pixelate"; break;
        case BRIGHTNESS: filterName = "Brightness"; break;
        case CONTRAST: filterName = "Contrast"; break;
        case EDGE_DETECTION: filterName = "Edge Detection"; break;
        case EMBOSS: filterName = "Emboss"; break;
        case SHARPEN: filterName = "Sharpen"; break;
        case MEDIAN_BLUR: filterName = "Median Blur"; break;
        case MORPHOLOGICAL_OPEN: filterName = "Morphological Open"; break;
        case BILATERAL_FILTER: filterName = "Bilateral Filter"; break;
    }
    
    std::cout << "Processing " << filterName << " with " 
              << (method == GPU_OPENCL ? "GPU (OpenCL)" : "CPU (OpenMP)") << "..." << std::endl;
    
    cv::Mat result;
    if (method == GPU_OPENCL && gOpenCLContext.initialized) {
        result = processImageGPU(image, filter, intensity);
    } else {
        if (method == GPU_OPENCL) {
            std::cout << "WARNING: GPU requested but not available, falling back to CPU" << std::endl;
        }
        result = processImageCPU(image, filter, intensity);
    }
    
    auto end = high_resolution_clock::now();
    double processingTime = duration_cast<microseconds>(end - start).count() / 1000.0; // Convert to milliseconds
    
    return make_pair(result, processingTime);
}

//HTTP SERVER SECTION ----------------------------------------------------

//Structure to hold request data
struct RequestData {
    string body;
    size_t size;
};

//Function to handle HTTP requests
static MHD_Result handleRequest(void *cls, struct MHD_Connection *connection,
                        const char *url, const char *method,
                        const char *version, const char *upload_data,
                        size_t *upload_data_size, void **con_cls) {
    
    // Handle CORS preflight requests
    if (strcmp(method, "OPTIONS") == 0) {
        const char* empty_response = "";
        struct MHD_Response *response = MHD_create_response_from_buffer(0, (void*)empty_response, MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "POST, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
        MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Only handle POST requests to /process or /batch
    if (strcmp(method, "POST") != 0 || (strcmp(url, "/process") != 0 && strcmp(url, "/batch") != 0)) {
        const char *page = "404 Not Found";
        struct MHD_Response *response = MHD_create_response_from_buffer(strlen(page), (void*)page, MHD_RESPMEM_PERSISTENT);
        MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Collect POST data
    static RequestData req_data;
    if (*con_cls == NULL) {
        req_data.body.clear();
        req_data.size = 0;
        *con_cls = &req_data;
        return MHD_YES;
    }
    
    if (*upload_data_size != 0) {
        req_data.body.append(upload_data, *upload_data_size);
        req_data.size += *upload_data_size;
        *upload_data_size = 0;
        return MHD_YES;
    }
    
    try {
        // Check if this is a batch processing request
        if (strcmp(url, "/batch") == 0) {
            // Handle batch processing (/batch endpoint)
            json request = json::parse(req_data.body);
            
            string imageData = request["imageData"];
            string methodStr = request["method"];
            json selectedFilters = request["selectedFilters"];
            json filterRanges = request["filterRanges"];
            
            // Log incoming batch request details
            std::cout << "\n" << std::string(70, '=') << std::endl;
            std::cout << "NEW BATCH PROCESSING REQUEST" << std::endl;
            std::cout << "Processing Method: " << methodStr << std::endl;
            std::cout << "Selected filters: " << selectedFilters.size() << std::endl;
            std::cout << std::string(70, '=') << std::endl;
            
            // Extract base64 image data
            string base64Data = extractBase64FromDataURL(imageData);
            vector<uchar> imageBuffer = base64Decode(base64Data);
            
            // Decode image
            cv::Mat image = cv::imdecode(imageBuffer, cv::IMREAD_COLOR);
            if (image.empty()) {
                throw runtime_error("Failed to decode image");
            }
            
            // Calculate memory size
            size_t totalPixels = static_cast<size_t>(image.cols) * static_cast<size_t>(image.rows);
            double imageSizeMB = (totalPixels * 3.0) / (1024.0 * 1024.0);
            std::cout << "IMAGE INFO: " << image.cols << "x" << image.rows 
                      << " (" << std::fixed << std::setprecision(1) << imageSizeMB << " MB)" << std::endl;
            
            // Build filter combinations from request
            FilterCombination combinations;
            
            // Parse color filters
            if (selectedFilters.contains("colorFilters")) {
                for (const auto& filterName : selectedFilters["colorFilters"]) {
                    FilterRange range;
                    range.filter = getFilterType(filterName.get<string>());
                    
                    string filterKey = filterName.get<string>();
                    if (filterRanges.contains(filterKey)) {
                        range.minIntensity = filterRanges[filterKey]["min"];
                        range.maxIntensity = filterRanges[filterKey]["max"];
                        range.increment = filterRanges[filterKey]["increment"];
                        // Check for single value mode
                        if (filterRanges[filterKey].contains("useSingleValue")) {
                            range.useSingleValue = filterRanges[filterKey]["useSingleValue"];
                        }
                    } else {
                        range.minIntensity = range.maxIntensity = 100.0f;
                        range.increment = 1.0f;
                    }
                    
                    combinations.colorFilters.push_back(range);
                }
            }
            
            // Parse effect filters
            if (selectedFilters.contains("effectFilters")) {
                for (const auto& filterName : selectedFilters["effectFilters"]) {
                    FilterRange range;
                    range.filter = getFilterType(filterName.get<string>());
                    
                    string filterKey = filterName.get<string>();
                    if (filterRanges.contains(filterKey)) {
                        range.minIntensity = filterRanges[filterKey]["min"];
                        range.maxIntensity = filterRanges[filterKey]["max"];
                        range.increment = filterRanges[filterKey]["increment"];
                        // Check for single value mode
                        if (filterRanges[filterKey].contains("useSingleValue")) {
                            range.useSingleValue = filterRanges[filterKey]["useSingleValue"];
                        }
                    } else {
                        range.minIntensity = range.maxIntensity = 100.0f;
                        range.increment = 1.0f;
                    }
                    
                    combinations.effectFilters.push_back(range);
                }
            }
            
            std::cout << "Total combinations to process: " << combinations.getTotalCombinations() << std::endl;
            
            // Check if we need to force disk storage
            bool forceDiskStorage = combinations.getTotalCombinations() >= 1500;
            bool useDiskStorage = forceDiskStorage;
            string outputPath = "";
            
            // Check for output path and storage mode in request
            if (request.contains("outputPath") && !request["outputPath"].is_null()) {
                outputPath = request["outputPath"];
                useDiskStorage = true;
            }
            if (request.contains("saveToDisk") && request["saveToDisk"].is_boolean()) {
                if (request["saveToDisk"] && !forceDiskStorage) {
                    useDiskStorage = request["saveToDisk"];
                }
            }
            
            std::cout << "Storage mode: " << (useDiskStorage ? "Disk" : "Memory") << std::endl;
            if (useDiskStorage) {
                std::cout << "Output path: " << outputPath << std::endl;
            }
            
            // Process batch
            ProcessingMethod method = getProcessingMethod(methodStr);
            vector<BatchResult> results;
            
            // Initialize GPU statistics variables
            int gpuSuccesses = 0;
            int gpuFailures = 0;
            
            if (useDiskStorage) {
                // Process in batches and save to disk
                auto batchResult = processBatchToDisk(image, combinations, method, outputPath);
                results = batchResult.first;
                gpuSuccesses = batchResult.second.first;
                gpuFailures = batchResult.second.second;
            } else if (method == GPU_OPENCL || method == RECOMMENDED) {
                results = processBatchGPU(image, combinations, method);
            } else {
                results = processBatchCPU(image, combinations, method);
            }
            
            // Create response
            json response;
            response["results"] = json::array();
            response["savedToDisk"] = useDiskStorage;
            if (useDiskStorage) {
                response["outputPath"] = outputPath;
            }
            
            for (const auto& result : results) {
                json resultJson;
                if (useDiskStorage) {
                    // For disk storage, processedImage contains filename
                    resultJson["filePath"] = result.processedImage;
                    resultJson["processedImage"] = ""; // Empty for disk storage
                } else {
                    // For memory storage, processedImage contains base64 data
                    resultJson["processedImage"] = result.processedImage;
                    resultJson["filePath"] = "";
                }
                resultJson["processingTime"] = result.processingTime;
                resultJson["combinationIndex"] = result.combinationIndex;
                resultJson["appliedFilters"] = json::array();
                
                for (const auto& filter : result.appliedFilters) {
                    json filterJson;
                    filterJson["name"] = filter.first;
                    filterJson["intensity"] = filter.second;
                    resultJson["appliedFilters"].push_back(filterJson);
                }
                
                response["results"].push_back(resultJson);
            }
            
            double totalTime = 0.0;
            for (const auto& result : results) {
                totalTime += result.processingTime;
            }
            response["totalProcessingTime"] = totalTime;
            response["averageProcessingTime"] = results.empty() ? 0.0 : totalTime / results.size();
            response["totalCombinations"] = results.size();
            
            // Add GPU usage statistics for disk storage mode
            if (useDiskStorage) {
                response["gpuSuccessCount"] = gpuSuccesses;
                response["gpuFailureCount"] = gpuFailures;
            }
            
            string responseStr = response.dump();
            
            struct MHD_Response *http_response = MHD_create_response_from_buffer(
                responseStr.length(), (void*)responseStr.c_str(), MHD_RESPMEM_MUST_COPY);
            
            // Add CORS headers
            MHD_add_response_header(http_response, "Access-Control-Allow-Origin", "*");
            MHD_add_response_header(http_response, "Content-Type", "application/json");
            
            MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, http_response);
            MHD_destroy_response(http_response);
            
            std::cout << "\nBATCH REQUEST COMPLETED!" << std::endl;
            std::cout << "Method: " << methodStr << std::endl;
            std::cout << "Total Combinations: " << results.size() << std::endl;
            std::cout << "Total Processing Time: " << totalTime << "ms" << std::endl;
            std::cout << std::string(70, '-') << std::endl;
            
            // Force GPU context cleanup after batch completion to prevent resource accumulation
            if (method == GPU_OPENCL) {
                cout << "Performing comprehensive GPU context cleanup after batch..." << endl;
                
                // Force all operations to complete
                clFinish(gOpenCLContext.queue);
                
                // Flush the command queue
                clFlush(gOpenCLContext.queue);
                
                // Additional aggressive memory management
                // Force synchronization and memory reclaim
                clFinish(gOpenCLContext.queue);
                
                // Small delay to let GPU driver fully clean up
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                cout << "GPU context comprehensive reset completed" << endl;
            }
            
            return ret;
        }
        
        // Handle single image processing (/process endpoint)
        json request = json::parse(req_data.body);
        
        string imageData = request["imageData"];
        json filters = request["filters"];
        json intensities = request["intensities"];
        string methodStr = request["method"];
        
        // Log incoming request details
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "NEW PROCESSING REQUEST" << std::endl;
        std::cout << "Processing Method: " << methodStr << std::endl;
        std::cout << "Color Filter: " << (filters["Color Adjustments"].is_null() ? "None" : filters["Color Adjustments"].get<string>()) << std::endl;
        std::cout << "Effect Filter: " << (filters["Effects"].is_null() ? "None" : filters["Effects"].get<string>()) << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        // Extract base64 image data
        string base64Data = extractBase64FromDataURL(imageData);
        vector<uchar> imageBuffer = base64Decode(base64Data);
        
        // Decode image
        cv::Mat image = cv::imdecode(imageBuffer, cv::IMREAD_COLOR);
        if (image.empty()) {
            throw runtime_error("Failed to decode image");
        }
        
        // Calculate memory size safely to avoid integer overflow
        size_t totalPixels = static_cast<size_t>(image.cols) * static_cast<size_t>(image.rows);
        double imageSizeMB = (totalPixels * 3.0) / (1024.0 * 1024.0);
        std::cout << "IMAGE INFO: " << image.cols << "x" << image.rows 
                  << " (" << std::fixed << std::setprecision(1) << imageSizeMB << " MB)" << std::endl;
        
        // Process each filter
        cv::Mat processedImage = image.clone();
        double totalProcessingTime = 0.0;
        
        ProcessingMethod method = getProcessingMethod(methodStr);
        bool isRecommendedMode = (methodStr == "Recommended");
        
        // Apply filters from Color Adjustments category
        if (!filters["Color Adjustments"].is_null()) {
            string filterName = filters["Color Adjustments"];
            FilterType filter = getFilterType(filterName);
            float intensity = intensities.contains(filterName) ? (float)intensities[filterName] : 100.0f;
            
            if (isRecommendedMode) {
                // Recommended mode: determine best method and use only that one
                ProcessingMethod recommendedMethod = selectRecommendedMethod(processedImage, filter);
                
                std::cout << "\nRECOMMENDED MODE - COLOR FILTER: " << filterName 
                          << " (Intensity: " << intensity << "%)" << std::endl;
                std::cout << "RECOMMENDATION: " << (recommendedMethod == GPU_OPENCL ? "GPU (OpenCL)" : "CPU (OpenMP)") << std::endl;
                std::cout << "Using recommended method only..." << std::endl;
                
                // Use only the recommended method
                auto result = processImage(processedImage, filter, intensity, recommendedMethod);
                processedImage = result.first;
                totalProcessingTime += result.second;
                
                std::cout << "Color filter completed in " << result.second << "ms" << std::endl;
                
            } else {
                std::cout << "\nAPPLYING COLOR FILTER: " << filterName 
                          << " (Intensity: " << intensity << "%)" << std::endl;
                
                auto result = processImage(processedImage, filter, intensity, method);
                processedImage = result.first;
                totalProcessingTime += result.second;
                
                std::cout << "Color filter completed in " << result.second << "ms" << std::endl;
            }
        }
        
        // Apply filters from Effects category
        if (!filters["Effects"].is_null()) {
            string filterName = filters["Effects"];
            FilterType filter = getFilterType(filterName);
            float intensity = intensities.contains(filterName) ? (float)intensities[filterName] : 100.0f;
            
            if (isRecommendedMode) {
                // Recommended mode: determine best method and use only that one
                ProcessingMethod recommendedMethod = selectRecommendedMethod(processedImage, filter);
                
                std::cout << "\nRECOMMENDED MODE - EFFECT FILTER: " << filterName 
                          << " (Intensity: " << intensity << "%)" << std::endl;
                std::cout << "RECOMMENDATION: " << (recommendedMethod == GPU_OPENCL ? "GPU (OpenCL)" : "CPU (OpenMP)") << std::endl;
                std::cout << "Using recommended method only..." << std::endl;
                
                // Use only the recommended method
                auto result = processImage(processedImage, filter, intensity, recommendedMethod);
                processedImage = result.first;
                totalProcessingTime += result.second;
                
                std::cout << "Effect filter completed in " << result.second << "ms" << std::endl;
                
            } else {
                std::cout << "\nAPPLYING EFFECT FILTER: " << filterName 
                          << " (Intensity: " << intensity << "%)" << std::endl;
                
                auto result = processImage(processedImage, filter, intensity, method);
                processedImage = result.first;
                totalProcessingTime += result.second;
                
                std::cout << "Effect filter completed in " << result.second << "ms" << std::endl;
            }
        }
        
        // Encode processed image back to base64
        vector<uchar> outputBuffer;
        cv::imencode(".jpg", processedImage, outputBuffer);
        string processedBase64 = "data:image/jpeg;base64," + base64Encode(outputBuffer);
        
        // Create response
        json response;
        response["processedImage"] = processedBase64;
        response["processingTime"] = totalProcessingTime;
        
        string responseStr = response.dump();
        
        struct MHD_Response *http_response = MHD_create_response_from_buffer(
            responseStr.length(), (void*)responseStr.c_str(), MHD_RESPMEM_MUST_COPY);
        
        // Add CORS headers
        MHD_add_response_header(http_response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(http_response, "Content-Type", "application/json");
        
        MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, http_response);
        MHD_destroy_response(http_response);
        
        std::cout << "\nREQUEST COMPLETED!" << std::endl;
        std::cout << "Method: " << methodStr << std::endl;
        std::cout << "Total Processing Time: " << totalProcessingTime << "ms" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        
        return ret;
        
    } catch (const exception& e) {
        cerr << "Error processing request: " << e.what() << endl;
        
        json errorResponse;
        errorResponse["error"] = e.what();
        string errorStr = errorResponse.dump();
        
        struct MHD_Response *response = MHD_create_response_from_buffer(
            errorStr.length(), (void*)errorStr.c_str(), MHD_RESPMEM_MUST_COPY);
        
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Content-Type", "application/json");
        
        MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        
        return ret;
    }
}

//MAIN FUNCTION ----------------------------------------------------

int main() {
    cout << "\n" << string(70, '=') << endl;
    cout << "IMAGE PROCESSOR SERVER STARTUP" << endl;
    cout << string(70, '=') << endl;
    
    // Initialize OpenMP
    cout << "OpenMP threads available: " << omp_get_max_threads() << endl;
    
    // Initialize OpenCL
    if (!initializeOpenCL()) {
        cout << "Warning: OpenCL initialization failed, GPU processing will not be available" << endl;
    }
    
    cout << string(70, '-') << endl;
    cout << "Server starting on http://localhost:" << SERVER_PORT << endl;
    cout << "Press Enter to stop the server..." << endl;
    cout << string(70, '=') << endl;
    
    // Create HTTP server
    struct MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_SELECT_INTERNALLY,
        SERVER_PORT,
        NULL, NULL,
        &handleRequest, NULL,
        MHD_OPTION_CONNECTION_LIMIT, MAX_CONNECTIONS,
        MHD_OPTION_END
    );
    
    if (daemon == NULL) {
        cerr << "Error: Failed to start HTTP server on port " << SERVER_PORT << endl;
        return 1;
    }
    
    // Server startup message already displayed above
    
    // Wait for user input to stop
    getchar();
    
    // Cleanup
    MHD_stop_daemon(daemon);
    gOpenCLContext.cleanup();
    
    cout << "Server stopped." << endl;
    return 0;
}