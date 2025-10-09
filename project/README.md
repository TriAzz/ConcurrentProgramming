# Image Processor - High-Performance Real-Time Image Processing

A professional-grade image processing application featuring real-time filtering with GPU acceleration, batch processing capabilities, and a modern React TypeScript frontend.

## Features

### 🎨 **Real-Time Image Processing**
- **14 Professional Filters:** Grayscale, Sepia, Blur, Pixelate, Edge Detection, and more
- **Live Preview:** Instant filter application with adjustable intensity sliders
- **GPU Acceleration:** OpenCL-powered processing with CPU fallback
- **Percentage-Based Effects:** Consistent filter strength across all image sizes

### ⚡ **High-Performance Batch Processing**
- **Four Processing Modes:** CPU/GPU × Memory/Disk with intelligent optimization
- **Pairwise Combinations:** Apply multiple filter combinations automatically
- **Professional Console Output:** Clean, unified logging across all processing modes
- **Storage Optimization:** Automatic memory vs disk selection based on batch size

### 🏗️ **Technical Architecture**
- **Frontend:** React TypeScript with modern component architecture
- **Backend:** C++ with OpenCL/OpenMP for maximum performance
- **API:** RESTful HTTP endpoints for seamless frontend-backend communication
- **Documentation:** Professional codebase with consistent comment styling

## Quick Start

### Prerequisites
- **Node.js** (for frontend)
- **C++ Compiler** (MSVC, GCC, or Clang)
- **OpenCL** (GPU acceleration)
- **OpenCV** (image processing)

### Installation & Setup

1. **Install Frontend Dependencies:**
   ```bash
   npm install
   ```

2. **Build Backend:**
   ```bash
   make
   # or use build.sh on Unix systems
   ```

3. **Run the Application:**
   ```bash
   # Start backend server
   ./image_processor_server.exe
   
   # Start frontend (in separate terminal)
   npm run dev
   ```

4. **Access Application:**
   Open [http://localhost:5173](http://localhost:5173) in your browser

## Usage

### Single Image Processing
1. Upload an image using the "Open Image" button
2. Select processing method (GPU/CPU)
3. Apply filters with real-time intensity adjustment
4. Save processed results

### Batch Processing
1. Select color and effect filters
2. Configure intensity ranges for each filter
3. Choose processing method and storage mode
4. Set output directory (for disk storage)
5. Process all combinations automatically

## License

Academic project for Concurrent Development coursework.
