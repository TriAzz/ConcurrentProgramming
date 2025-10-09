# Makefile for Image Processor Server
# Use with: make

CXX = g++
CXXFLAGS = -std=c++17 -O3 -fopenmp -Wall
INCLUDES = -I/mingw64/include -I/mingw64/include/opencv4 -I/mingw64/include/nlohmann
LIBS = -L/mingw64/lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_highgui -lmicrohttpd -lssl -lcrypto -lOpenCL -fopenmp

SOURCE = image_processor_server.cpp
TARGET = image_processor_server.exe

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCE)
	@echo "Building Image Processor Server..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCE) $(LIBS) -o $(TARGET)
	@echo "✅ Build complete! Run with: make run"

clean:
	@echo "Cleaning build files..."
	rm -f $(TARGET)
	@echo "Clean complete."

run: $(TARGET)
	@echo "Starting Image Processor Server on http://localhost:8080"
	@echo "Press Ctrl+C to stop the server."
	./$(TARGET)

install-deps:
	@echo "Installing required dependencies..."
	pacman -S mingw-w64-x86_64-opencv mingw-w64-x86_64-libmicrohttpd
	pacman -S mingw-w64-x86_64-nlohmann-json mingw-w64-x86_64-openssl
	@echo "Dependencies installed."

help:
	@echo "Available targets:"
	@echo "  all        - Build the server (default)"
	@echo "  clean      - Remove build files"
	@echo "  run        - Build and run the server"
	@echo "  install-deps - Install required dependencies"
	@echo "  help       - Show this help message"