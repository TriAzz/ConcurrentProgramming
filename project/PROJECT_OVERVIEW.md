# Project Overview: Real-Time Image Filter Processor

## 1. Project Scope

The project is a web-based image processing application designed for real time filter application and performance comparison between CPU and GPU processing. This app allows you to utlise either CPU OpenMP parallelisation for single image processing, or large scale batch processing using the GPU via OpenCL

### Key Features:
- **Real Time Filtering:** Users can upload an image and apply a variety of filters, seeing the results instantly.
- **GPU vs. CPU Processing:** The application allows users to switch between GPU (via OpenCL) and CPU (via OpenMP) processing to compare performance.
- **Batch Processing:** Users can apply multiple filters with varying intensities to a single image, generating a large number of processed images automatically.
- **Performance Analytics:** The application logs and displays the time taken for different processing tasks, providing insights into the efficiency of each method.

## 2. Project Design

The application is architected with a clear separation between the frontend and backend.

### Frontend Architecture (React + TypeScript)
- **Component Based Structure:** The UI is broken down into reusable components located in `src/components/`.
  - `App.tsx`: The main application component that manages the overall state.
  - `ControlPanel.tsx`: Houses the controls for selecting filters, adjusting intensities, and choosing processing methods.
  - `ImageViewer.tsx`: Displays the original and processed images.
  - `BatchProcessor.tsx`: Provides the interface for configuring and running batch processing jobs.
  - `BatchResultsViewer.tsx`: Displays the results from a batch processing job.
- **State Management:** State is managed within React components using hooks (`useState`, `useEffect`, etc.).
- **API Communication:** The frontend communicates with the backend via a RESTful API to process images.

### Backend Architecture (C++ with OpenCL/OpenMP)
- **High-Performance Core:** The backend is a C++ server (`image_processor_server.cpp`) designed for high-performance image manipulation.
- **HTTP Server:** It uses the `libmicrohttpd` library to handle HTTP requests from the frontend.
- **Image Processing:** Image processing is handled by the OpenCV library.
- **Parallel Processing:**
  - **CPU:** Utilises OpenMP for parallelizing filter application on the CPU.
  - **GPU:** Utilises OpenCL (`image_filters.cl`) to offload processing to the GPU.
- **API Endpoints:**
  - `/process`: Handles single image processing requests.
  - `/batch`: Handles batch processing requests.

## 3. Implementation Details

### Frontend
- The frontend is built using Vite, with React and TypeScript.
- It communicates with the backend server at `http://localhost:8080`.
- The UI allows for dynamic selection of filters and adjustment of their intensities.
- For batch processing, it calculates the total number of image combinations and provides options for saving results to disk or viewing them in the browser.

### Backend
- The C++ server is compiled using `g++` with flags to enable OpenMP and link against OpenCL and OpenCV libraries.
- The `build.sh` script and `Makefile` provide instructions for compiling the server.
- The server receives image data as a base64 encoded string, decodes it, processes it with OpenCV, and returns the processed image back to the frontend.
- The OpenCL kernels in `image_filters.cl` are loaded and executed for GPU-based processing.

## 4. Evaluation

The application's performance was evaluated by comparing the processing times of the CPU (OpenMP) and GPU (OpenCL) implementations across different workloads.

### Key Findings:
- **Single Image Processing:** For individual images, the CPU consistently outperforms the GPU. The overhead associated with transferring image data to and from the GPU makes it less efficient for single, small tasks. While some filters show comparable performance, the CPU is generally the faster option.
- **Small to Medium Batches:** When processing a small to medium number of image variations, the CPU continues to hold a performance advantage. The batch size is not large enough to offset the GPU's data transfer overhead.
- **Large-Scale Batches:** For very large batch processing jobs, the GPU becomes significantly more efficient. The parallel architecture of the GPU allows it to process a massive number of images concurrently, overcoming the initial setup costs and resulting in substantially faster overall completion times compared to the CPU.

This evaluation demonstrates a clear trade-off between CPU and GPU processing. The optimal choice depends on the scale of the task: the CPU is better suited for quick, individual image edits, while the GPU excels at large-scale, parallel workloads.
