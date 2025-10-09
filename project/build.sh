echo "Building Image Processor Server..."

# Compiler and flags
CXX="g++"
CXXFLAGS="-std=c++17 -O3 -fopenmp -Wall"

# Include paths
INCLUDES="-I/mingw64/include -I/mingw64/include/opencv4 -I/mingw64/include/nlohmann"

# Library paths and libraries
LIBS="-L/mingw64/lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_highgui"
LIBS="$LIBS -lmicrohttpd -lssl -lcrypto -lOpenCL -fopenmp"

# Source file
SOURCE="image_processor_server.cpp"

# Output executable
OUTPUT="image_processor_server.exe"

# Compile command
echo "Compiling with command:"
echo "$CXX $CXXFLAGS $INCLUDES $SOURCE $LIBS -o $OUTPUT"
echo ""

$CXX $CXXFLAGS $INCLUDES $SOURCE $LIBS -o $OUTPUT

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Build successful! Executable created: $OUTPUT"
    echo ""
    echo "To run the server:"
    echo "  ./$OUTPUT"
    echo ""
    echo "The server will run on http://localhost:8080"
else
    echo ""
    echo "❌ Build failed! Check the error messages above."
    echo ""
    echo "Common fixes:"
    echo "  1. Make sure you're in the MSYS2 MINGW64 terminal"
    echo "  2. Verify all packages are installed:"
    echo "     pacman -S mingw-w64-x86_64-opencv mingw-w64-x86_64-libmicrohttpd"
    echo "     pacman -S mingw-w64-x86_64-nlohmann-json mingw-w64-x86_64-openssl"
    echo "  3. Check that OpenCL is available through CUDA toolkit"
fi