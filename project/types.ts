
//============================================================================
// TYPE DEFINITIONS - TYPESCRIPT INTERFACES AND ENUMS
//============================================================================
// Comprehensive type definitions for the image processor application
// including processing methods, filter configurations, batch processing
// structures, and API request/response interfaces
//============================================================================

//PROCESSING CONFIGURATION SECTION ----------------------------------------------------

//Enumeration of available processing methods for image operations
export enum ProcessingMethod {
    GPU = 'GPU (OpenCL)',
    CPU = 'CPU (OpenMP)',
    RECOMMENDED = 'Recommended',
}

//Enumeration of filter categories for UI organization
export enum FilterCategory {
    Color = 'Color Adjustments',
    Effect = 'Effects',
}

//FILTER DEFINITION INTERFACES SECTION ----------------------------------------------------

//Interface defining the structure of an image filter with its properties
export interface Filter {
    name: string;
    category: FilterCategory;
    hasSlider: boolean;
    slider?: {
        min: number;
        max: number;
        step: number;
        defaultValue: number;
        unit: string;
    };
}

//Interface for tracking currently active filters by category
export interface ActiveFilters {
    [FilterCategory.Color]: string | null;
    [FilterCategory.Effect]: string | null;
}

//Interface for storing filter intensity values by filter name
export interface FilterIntensities {
    [filterName: string]: number;
}

//LOGGING INTERFACE SECTION ----------------------------------------------------

//Interface for log entries in the performance monitoring system
export interface LogEntry {
    id: number;
    message: string;
    isSeparator: boolean;
}

//BATCH PROCESSING INTERFACES SECTION ----------------------------------------------------
export interface FilterRange {
    min: number;
    max: number;
    increment: number;
    useSingleValue?: boolean; // When true, only use min value (ignore max/increment)
}

export interface BatchFilterSelection {
    colorFilters: string[];
    effectFilters: string[];
}

export interface BatchFilterRanges {
    [filterName: string]: FilterRange;
}

export interface BatchRequest {
    imageData: string;
    method: string;
    selectedFilters: BatchFilterSelection;
    filterRanges: BatchFilterRanges;
    saveToDisk?: boolean;
    outputPath?: string;
}

export interface AppliedFilter {
    name: string;
    intensity: number;
}

export interface BatchResult {
    processedImage: string; // Base64 data for memory mode, empty for disk mode
    filePath?: string; // File path for disk mode
    processingTime: number;
    combinationIndex: number;
    appliedFilters: AppliedFilter[];
}

export interface BatchResponse {
    results: BatchResult[];
    totalProcessingTime: number;
    averageProcessingTime: number;
    totalCombinations: number;
    savedToDisk?: boolean;
    outputPath?: string;
    gpuSuccessCount?: number;
    gpuFailureCount?: number;
}
