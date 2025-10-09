//UTILITY FUNCTIONS SECTION ----------------------------------------------------

//Function to clamp floating point values to valid uchar range (0-255)
uchar clamp_uchar(float value) {
    return (value < 0) ? 0 : ((value > 255) ? 255 : (uchar)value);
}

//Function to convert RGB color values to HSV color space for advanced color processing
float3 rgb_to_hsv(float3 rgb) {
    float r = rgb.x / 255.0f;
    float g = rgb.y / 255.0f;
    float b = rgb.z / 255.0f;
    
    float max_val = fmax(fmax(r, g), b);
    float min_val = fmin(fmin(r, g), b);
    float delta = max_val - min_val;
    
    float h = 0.0f, s = 0.0f, v = max_val;
    
    if (delta > 0.0f) {
        s = delta / max_val;
        
        if (max_val == r) {
            h = 60.0f * (((g - b) / delta) + (g < b ? 6.0f : 0.0f));
        } else if (max_val == g) {
            h = 60.0f * (((b - r) / delta) + 2.0f);
        } else {
            h = 60.0f * (((r - g) / delta) + 4.0f);
        }
    }
    
    return (float3)(h, s, v);
}

//Function to convert HSV color values back to RGB color space
float3 hsv_to_rgb(float3 hsv) {
    float h = hsv.x;
    float s = hsv.y;
    float v = hsv.z;
    
    if (s == 0.0f) {
        return (float3)(v * 255.0f, v * 255.0f, v * 255.0f);
    }
    
    h = h / 60.0f;
    int i = (int)floor(h);
    float f = h - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    
    float3 rgb;
    switch (i % 6) {
        case 0: rgb = (float3)(v, t, p); break;
        case 1: rgb = (float3)(q, v, p); break;
        case 2: rgb = (float3)(p, v, t); break;
        case 3: rgb = (float3)(p, q, v); break;
        case 4: rgb = (float3)(t, p, v); break;
        case 5: rgb = (float3)(v, p, q); break;
        default: rgb = (float3)(0, 0, 0); break;
    }
    
    return rgb * 255.0f;
}

//COLOR FILTER KERNELS SECTION ---------------------------------------------

//Kernel for grayscale conversion using standard luminance formula
__kernel void grayscale_filter(__global uchar* input, __global uchar* output, int width, int height) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    
    uchar r = input[idx];
    uchar g = input[idx + 1];
    uchar b = input[idx + 2];
    
    //Standard grayscale conversion formula
    uchar gray = (uchar)(0.299f * r + 0.587f * g + 0.114f * b);
    
    output[idx] = gray;
    output[idx + 1] = gray;
    output[idx + 2] = gray;
}

//Kernel for sepia tone effect creating vintage photo appearance
__kernel void sepia_filter(__global uchar* input, __global uchar* output, int width, int height) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    
    float r = (float)input[idx];
    float g = (float)input[idx + 1];
    float b = (float)input[idx + 2];
    
    //Sepia tone transformation matrix
    output[idx] = clamp_uchar(0.393f * r + 0.769f * g + 0.189f * b);
    output[idx + 1] = clamp_uchar(0.349f * r + 0.686f * g + 0.168f * b);
    output[idx + 2] = clamp_uchar(0.272f * r + 0.534f * g + 0.131f * b);
}

//Kernel for color inversion effect
__kernel void invert_filter(__global uchar* input, __global uchar* output, int width, int height) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    
    output[idx] = 255 - input[idx];
    output[idx + 1] = 255 - input[idx + 1];
    output[idx + 2] = 255 - input[idx + 2];
}

//Kernel for saturation adjustment using HSV color space
__kernel void saturation_filter(__global uchar* input, __global uchar* output, int width, int height, float intensity) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    
    float3 rgb = (float3)(input[idx], input[idx + 1], input[idx + 2]);
    float3 hsv = rgb_to_hsv(rgb);
    
    //Adjust saturation: 0% = grayscale, 100% = original, >100% = oversaturated
    hsv.y *= (intensity / 100.0f);
    hsv.y = clamp(hsv.y, 0.0f, 1.0f);
    
    float3 new_rgb = hsv_to_rgb(hsv);
    
    output[idx] = clamp_uchar(new_rgb.x);
    output[idx + 1] = clamp_uchar(new_rgb.y);
    output[idx + 2] = clamp_uchar(new_rgb.z);
}

//Kernel for brightness adjustment with linear scaling
__kernel void brightness_filter(__global uchar* input, __global uchar* output, int width, int height, float intensity) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    
    float r = (float)input[idx];
    float g = (float)input[idx + 1];
    float b = (float)input[idx + 2];
    
    //Brightness adjustment: 0% = black, 100% = original, 200% = doubled
    float factor = intensity / 100.0f;
    
    output[idx] = clamp_uchar(r * factor);
    output[idx + 1] = clamp_uchar(g * factor);
    output[idx + 2] = clamp_uchar(b * factor);
}

//Kernel for contrast adjustment using midpoint expansion
__kernel void contrast_filter(__global uchar* input, __global uchar* output, int width, int height, float intensity) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    
    float r = (float)input[idx];
    float g = (float)input[idx + 1];
    float b = (float)input[idx + 2];
    
    //Contrast adjustment: expand/contract around midpoint (128)
    float factor = intensity / 100.0f;
    
    output[idx] = clamp_uchar((r - 128.0f) * factor + 128.0f);
    output[idx + 1] = clamp_uchar((g - 128.0f) * factor + 128.0f);
    output[idx + 2] = clamp_uchar((b - 128.0f) * factor + 128.0f);
}

//EFFECT FILTER KERNELS SECTION -----------------------------------------------

//Kernel for blur effect with percentage-based radius scaling
__kernel void blur_filter(__global uchar* input, __global uchar* output, int width, int height, float intensity) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    
    //Calculate blur radius with performance optimization (max 20 pixels)
    int blur_radius;
    if (intensity == 0.0f) {
        blur_radius = 0; //No blur at 0% intensity
    } else {
        blur_radius = max(1, min(20, (int)((intensity / 100.0f) * 20.0f)));
    }
    
    //Early exit optimization for no blur case
    if (blur_radius == 0) {
        output[idx] = input[idx];
        output[idx + 1] = input[idx + 1];
        output[idx + 2] = input[idx + 2];
        return;
    }

    float3 sum = (float3)(0, 0, 0);
    int count = 0;

    //Apply box blur within calculated radius
    for (int dy = -blur_radius; dy <= blur_radius; dy++) {
        for (int dx = -blur_radius; dx <= blur_radius; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                int nidx = (ny * width + nx) * 3;
                sum.x += input[nidx];
                sum.y += input[nidx + 1];
                sum.z += input[nidx + 2];
                count++;
            }
        }
    }

    //Apply the averaged blur result
    output[idx] = clamp_uchar(sum.x / count);
    output[idx + 1] = clamp_uchar(sum.y / count);
    output[idx + 2] = clamp_uchar(sum.z / count);
}

//Kernel for pixelate effect with percentage-based block scaling
__kernel void pixelate_filter(__global uchar* input, __global uchar* output, int width, int height, float intensity) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    
    //Calculate block size for pixelation effect (0% = original, 100% = maximum)
    int blockSize;
    if (intensity == 0.0f) {
        blockSize = 1; //No pixelation at 0% intensity
    } else {
        //Higher intensity creates larger blocks for increased pixelation
        blockSize = max(1, (int)((intensity / 100.0f) * min((float)width, (float)height) / 10.0f));
    }
    
    //Find the block center for this pixel
    int blockX = (x / blockSize) * blockSize + blockSize / 2;
    int blockY = (y / blockSize) * blockSize + blockSize / 2;
    
    //Clamp to image boundaries
    blockX = min(blockX, width - 1);
    blockY = min(blockY, height - 1);
    
    int blockIdx = (blockY * width + blockX) * 3;
    
    output[idx] = input[blockIdx];
    output[idx + 1] = input[blockIdx + 1];
    output[idx + 2] = input[blockIdx + 2];
}

//Kernel for edge detection using Sobel operator
__kernel void edge_detection_filter(__global uchar* input, __global uchar* output, int width, int height, float intensity) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    
    //Convert to grayscale first for edge detection
    uchar gray = (uchar)(0.299f * input[idx] + 0.587f * input[idx + 1] + 0.114f * input[idx + 2]);
    
    if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
        //Border pixels remain as grayscale
        output[idx] = gray;
        output[idx + 1] = gray;
        output[idx + 2] = gray;
        return;
    }
    
    //Sobel X and Y kernels for edge detection
    float gx = 0, gy = 0;
    
    //Apply Sobel operators
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            int nidx = (ny * width + nx) * 3;
            
            uchar pixel = (uchar)(0.299f * input[nidx] + 0.587f * input[nidx + 1] + 0.114f * input[nidx + 2]);
            
            //Sobel X kernel weights
            int sx_weight = (dx == -1) ? -1 : (dx == 1) ? 1 : 0;
            if (dy == -1 || dy == 1) sx_weight *= (dy == 0) ? 2 : 1;
            
            //Sobel Y kernel weights  
            int sy_weight = (dy == -1) ? -1 : (dy == 1) ? 1 : 0;
            if (dx == -1 || dx == 1) sy_weight *= (dx == 0) ? 2 : 1;
            
            gx += pixel * sx_weight;
            gy += pixel * sy_weight;
        }
    }
    
    //Calculate edge magnitude
    float magnitude = sqrt(gx * gx + gy * gy);
    magnitude = magnitude * (intensity / 100.0f);
    
    uchar edge_value = clamp_uchar(magnitude);
    
    output[idx] = edge_value;
    output[idx + 1] = edge_value;
    output[idx + 2] = edge_value;
}

//Kernel for emboss effect creating 3D appearance
__kernel void emboss_filter(__global uchar* input, __global uchar* output, int width, int height, float intensity) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    
    if (x == 0 || y == 0 || x >= width - 1 || y >= height - 1) {
        //Border pixels
        output[idx] = 128;
        output[idx + 1] = 128;
        output[idx + 2] = 128;
        return;
    }
    
    //Emboss kernel convolution
    float sum = 0;
    sum += -2 * (0.299f * input[((y-1) * width + (x-1)) * 3] + 0.587f * input[((y-1) * width + (x-1)) * 3 + 1] + 0.114f * input[((y-1) * width + (x-1)) * 3 + 2]);
    sum += -1 * (0.299f * input[((y-1) * width + x) * 3] + 0.587f * input[((y-1) * width + x) * 3 + 1] + 0.114f * input[((y-1) * width + x) * 3 + 2]);
    sum += 0 * (0.299f * input[((y-1) * width + (x+1)) * 3] + 0.587f * input[((y-1) * width + (x+1)) * 3 + 1] + 0.114f * input[((y-1) * width + (x+1)) * 3 + 2]);
    sum += -1 * (0.299f * input[(y * width + (x-1)) * 3] + 0.587f * input[(y * width + (x-1)) * 3 + 1] + 0.114f * input[(y * width + (x-1)) * 3 + 2]);
    sum += 1 * (0.299f * input[(y * width + x) * 3] + 0.587f * input[(y * width + x) * 3 + 1] + 0.114f * input[(y * width + x) * 3 + 2]);
    sum += 1 * (0.299f * input[(y * width + (x+1)) * 3] + 0.587f * input[(y * width + (x+1)) * 3 + 1] + 0.114f * input[(y * width + (x+1)) * 3 + 2]);
    sum += 0 * (0.299f * input[((y+1) * width + (x-1)) * 3] + 0.587f * input[((y+1) * width + (x-1)) * 3 + 1] + 0.114f * input[((y+1) * width + (x-1)) * 3 + 2]);
    sum += 1 * (0.299f * input[((y+1) * width + x) * 3] + 0.587f * input[((y+1) * width + x) * 3 + 1] + 0.114f * input[((y+1) * width + x) * 3 + 2]);
    sum += 2 * (0.299f * input[((y+1) * width + (x+1)) * 3] + 0.587f * input[((y+1) * width + (x+1)) * 3 + 1] + 0.114f * input[((y+1) * width + (x+1)) * 3 + 2]);
    
    sum = sum * (intensity / 100.0f) + 128;
    uchar result = clamp_uchar(sum);
    
    output[idx] = result;
    output[idx + 1] = result;
    output[idx + 2] = result;
}

//Kernel for sharpen filter enhancing image details
__kernel void sharpen_filter(__global uchar* input, __global uchar* output, int width, int height, float intensity) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    
    if (x == 0 || y == 0 || x >= width - 1 || y >= height - 1) {
        //Border pixels remain unchanged
        output[idx] = input[idx];
        output[idx + 1] = input[idx + 1];
        output[idx + 2] = input[idx + 2];
        return;
    }
    
    //Sharpen kernel: center weight increases with intensity
    float center_weight = 1.0f + (intensity / 100.0f) * 4.0f;
    float neighbor_weight = -(intensity / 100.0f);
    
    for (int c = 0; c < 3; c++) {
        float sum = 0;
        sum += neighbor_weight * input[((y-1) * width + x) * 3 + c];  // top
        sum += neighbor_weight * input[(y * width + (x-1)) * 3 + c];  // left
        sum += center_weight * input[(y * width + x) * 3 + c];        // center
        sum += neighbor_weight * input[(y * width + (x+1)) * 3 + c];  // right
        sum += neighbor_weight * input[((y+1) * width + x) * 3 + c];  // bottom
        
        output[idx + c] = clamp_uchar(sum);
    }
}

//Kernel for median blur noise reduction
__kernel void median_blur_filter(__global uchar* input, __global uchar* output, int width, int height, float intensity) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    int radius = max(1, (int)(intensity / 100.0f * 3.0f));
    
    //Collect neighboring pixels for each color channel
    uchar values_r[49]; // Maximum 7x7 neighborhood
    uchar values_g[49];
    uchar values_b[49];
    int count = 0;
    
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            
            if (nx >= 0 && nx < width && ny >= 0 && ny < height && count < 49) {
                int nidx = (ny * width + nx) * 3;
                values_r[count] = input[nidx];
                values_g[count] = input[nidx + 1];
                values_b[count] = input[nidx + 2];
                count++;
            }
        }
    }
    
    //Simple bubble sort for median finding (sufficient for small arrays)
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (values_r[j] > values_r[j + 1]) {
                uchar temp = values_r[j];
                values_r[j] = values_r[j + 1];
                values_r[j + 1] = temp;
            }
            if (values_g[j] > values_g[j + 1]) {
                uchar temp = values_g[j];
                values_g[j] = values_g[j + 1];
                values_g[j + 1] = temp;
            }
            if (values_b[j] > values_b[j + 1]) {
                uchar temp = values_b[j];
                values_b[j] = values_b[j + 1];
                values_b[j + 1] = temp;
            }
        }
    }
    
    //Set median values
    int median_idx = count / 2;
    output[idx] = values_r[median_idx];
    output[idx + 1] = values_g[median_idx];
    output[idx + 2] = values_b[median_idx];
}

//Kernel for morphological opening operation (erosion followed by dilation)
__kernel void morphological_open_filter(__global uchar* input, __global uchar* output, int width, int height, float intensity) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    int radius = max(1, (int)(intensity / 100.0f * 2.0f));
    
    //Convert to grayscale for morphological operation
    uchar gray = (uchar)(0.299f * input[idx] + 0.587f * input[idx + 1] + 0.114f * input[idx + 2]);
    
    //Erosion: find minimum in neighborhood
    uchar min_val = 255;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                int nidx = (ny * width + nx) * 3;
                uchar neighbor_gray = (uchar)(0.299f * input[nidx] + 0.587f * input[nidx + 1] + 0.114f * input[nidx + 2]);
                min_val = min(min_val, neighbor_gray);
            }
        }
    }
    
    //For opening, we'd need a second pass for dilation
    //Simplified: just apply erosion effect
    output[idx] = min_val;
    output[idx + 1] = min_val;
    output[idx + 2] = min_val;
}

//Kernel for bilateral filter preserving edges while reducing noise
__kernel void bilateral_filter(__global uchar* input, __global uchar* output, int width, int height, float intensity) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * 3;
    int radius = max(1, (int)(intensity / 100.0f * 3.0f));
    
    float sigma_space = radius / 2.0f;
    float sigma_color = intensity;
    
    float3 center_color = (float3)(input[idx], input[idx + 1], input[idx + 2]);
    float3 sum = (float3)(0, 0, 0);
    float weight_sum = 0;
    
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                int nidx = (ny * width + nx) * 3;
                float3 neighbor_color = (float3)(input[nidx], input[nidx + 1], input[nidx + 2]);
                
                //Spatial weight (Gaussian based on distance)
                float spatial_dist = sqrt((float)(dx * dx + dy * dy));
                float spatial_weight = exp(-(spatial_dist * spatial_dist) / (2 * sigma_space * sigma_space));
                
                //Color weight (Gaussian based on color difference)
                float3 color_diff = center_color - neighbor_color;
                float color_dist = sqrt(color_diff.x * color_diff.x + color_diff.y * color_diff.y + color_diff.z * color_diff.z);
                float color_weight = exp(-(color_dist * color_dist) / (2 * sigma_color * sigma_color));
                
                float total_weight = spatial_weight * color_weight;
                sum += neighbor_color * total_weight;
                weight_sum += total_weight;
            }
        }
    }
    
    if (weight_sum > 0) {
        sum /= weight_sum;
        output[idx] = clamp_uchar(sum.x);
        output[idx + 1] = clamp_uchar(sum.y);
        output[idx + 2] = clamp_uchar(sum.z);
    } else {
        output[idx] = input[idx];
        output[idx + 1] = input[idx + 1];
        output[idx + 2] = input[idx + 2];
    }
}
