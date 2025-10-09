import React, { useState, useEffect, useCallback, useRef } from 'react';
import { ControlPanel } from './components/ControlPanel';
import { ImageViewer } from './components/ImageViewer';
import { BatchProcessor } from './components/BatchProcessor';
import { BatchResultsViewer } from './components/BatchResultsViewer';
import { FILTERS } from './constants';
import type { ActiveFilters, FilterIntensities, BatchResult } from './types';
import { ProcessingMethod, FilterCategory } from './types';

const App: React.FC = () => {
    //STATE MANAGEMENT SECTION ----------------------------------------------------
    
    //Processing mode control (single image vs batch processing)
    const [processingMode, setProcessingMode] = useState<'single' | 'batch'>('single');
    
    //Image state management
    const [originalImage, setOriginalImage] = useState<string | null>(null);        //User's uploaded image
    const [displayedImage, setDisplayedImage] = useState<string | null>(null);      //Processed result from backend
    
    //User interface state
    const [imageName, setImageName] = useState<string>('');
    const [isGpuAvailable, setIsGpuAvailable] = useState<boolean>(true);
    const [processingMethod, setProcessingMethod] = useState<ProcessingMethod>(ProcessingMethod.GPU);
    const [isProcessing, setIsProcessing] = useState<boolean>(false);
    
    //Batch processing state
    const [batchResults, setBatchResults] = useState<BatchResult[]>([]);
    
    //Debounce reference for slider input handling
    const debounceTimeoutRef = useRef<number | null>(null);

    //FILTER MANAGEMENT SECTION ----------------------------------------------------

    //Initialize filter intensities with default values from constants
    const initialIntensities = FILTERS.reduce((acc, filter) => {
        if (filter.hasSlider && filter.slider) {
            acc[filter.name] = filter.slider.defaultValue;
        }
        return acc;
    }, {} as FilterIntensities);

    //Filter state management
    const [activeFilters, setActiveFilters] = useState<ActiveFilters>({
        [FilterCategory.Color]: null,
        [FilterCategory.Effect]: null,
    });
    const [filterIntensities, setFilterIntensities] = useState<FilterIntensities>(initialIntensities);

    //GPU DETECTION AND INITIALIZATION SECTION ----------------------------------------------------

    //Effect hook for GPU availability detection on component mount
    useEffect(() => {
        //Simulated GPU detection for UI purposes - backend handles actual GPU availability
        const gpuDetected = Math.random() > 0.1;
        setIsGpuAvailable(gpuDetected);
        setProcessingMethod(gpuDetected ? ProcessingMethod.GPU : ProcessingMethod.CPU);
    }, []);

    const processImageOnBackend = useCallback(async (action: string, method: ProcessingMethod, currentFilters: ActiveFilters, currentIntensities: FilterIntensities) => {
        if (!originalImage) return;

        setIsProcessing(true);
        
        try {
            const response = await fetch('http://localhost:8080/process', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    imageData: originalImage,
                    filters: currentFilters,
                    intensities: currentIntensities,
                    method: method,
                }),
            });

            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(`Backend Error: ${response.status} ${errorText}`);
            }

            const result = await response.json();

            setDisplayedImage(result.processedImage);
            //Processing completed successfully

        } catch (error) {
            //Processing failed - error handled by component
            //Revert to original image on failure
            setDisplayedImage(originalImage);
        } finally {
            setIsProcessing(false);
        }

    }, [originalImage]);

    //EVENT HANDLERS SECTION ----------------------------------------------------

    const handleImageUpload = (file: File) => {
        const reader = new FileReader();
        reader.onload = (e) => {
            const imageDataUrl = e.target?.result as string;
            setOriginalImage(imageDataUrl);
            setDisplayedImage(imageDataUrl); // Initially display the original image
            setImageName(file.name);
            //New image loaded successfully
        };
        reader.readAsDataURL(file);
    };

    const handleFilterToggle = useCallback((filterName: string, category: FilterCategory) => {
        const newActiveFilters = { ...activeFilters };
        const currentActive = newActiveFilters[category];
        const newActive = currentActive === filterName ? null : filterName;
        newActiveFilters[category] = newActive;
        
        setActiveFilters(newActiveFilters);
        
        const action = newActive ? `Apply ${filterName}` : `Remove ${filterName}`;
        processImageOnBackend(action, processingMethod, newActiveFilters, filterIntensities);
        
    }, [activeFilters, filterIntensities, processingMethod, processImageOnBackend]);

    const handleIntensityChange = (filterName: string, value: number) => {
        const newIntensities = { ...filterIntensities, [filterName]: value };
        setFilterIntensities(newIntensities);

        if (debounceTimeoutRef.current) {
            clearTimeout(debounceTimeoutRef.current);
        }

        debounceTimeoutRef.current = window.setTimeout(() => {
            processImageOnBackend(`Adjust ${filterName}`, processingMethod, activeFilters, newIntensities);
        }, 250);
    };

    const handleProcessingMethodChange = (method: ProcessingMethod) => {
        setProcessingMethod(method);
        const hasActiveFilters = Object.values(activeFilters).some(f => f !== null);
        if (hasActiveFilters) {
            processImageOnBackend(`Re-apply All`, method, activeFilters, filterIntensities);
        }
    };
    
    const handleResetAll = () => {
        const hadActiveFilters = Object.values(activeFilters).some(f => f !== null);
        const newActiveFilters = { [FilterCategory.Color]: null, [FilterCategory.Effect]: null };
        setActiveFilters(newActiveFilters);
        setFilterIntensities(initialIntensities);
        setDisplayedImage(originalImage); // Reset view to original image
        if (hadActiveFilters) {
            //Filters reset successfully
        }
    };

    const handleSaveImage = () => {
        if (!displayedImage || !imageName) {
            alert("Please load an image first.");
            return;
        }

        const link = document.createElement('a');
        const fileExtension = imageName.split('.').pop()?.toLowerCase();
        let fileName = `filtered-${imageName.split('.').slice(0, -1).join('.') || imageName}.png`;

        if (fileExtension === 'jpg' || fileExtension === 'jpeg') {
            fileName = `filtered-${imageName.split('.').slice(0, -1).join('.') || imageName}.jpg`;
        }

        link.download = fileName;
        link.href = displayedImage; // Use the processed image from the backend
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
        //Image saved successfully
    };

    //BATCH PROCESSING HANDLERS SECTION ----------------------------------------------------

    //Batch processing handlers
    const handleBatchProcessingStart = () => {
        setIsProcessing(true);
        setBatchResults([]);
    };

    const handleBatchProcessingComplete = (results: BatchResult[]) => {
        setBatchResults(results);
        setIsProcessing(false);
    };

    const handleBatchError = (error: string) => {
        setIsProcessing(false);
        //Batch processing error handled
    };

    //COMPONENT RENDER SECTION ----------------------------------------------------

    return (
        <div className="flex h-screen w-screen">
            <main className="flex-1 bg-gray-800/50 flex flex-col">
                {/* Mode Toggle */}
                <div className="bg-gray-900 border-b border-gray-700 p-4">
                    <div className="flex items-center gap-4">
                        <span className="text-white font-medium">Processing Mode:</span>
                        <div className="flex bg-gray-800 rounded-lg p-1">
                            <button
                                onClick={() => setProcessingMode('single')}
                                className={`px-4 py-2 rounded-md text-sm font-medium transition-colors ${
                                    processingMode === 'single'
                                        ? 'bg-blue-600 text-white'
                                        : 'text-gray-300 hover:text-white'
                                }`}
                            >
                                Single Image
                            </button>
                            <button
                                onClick={() => setProcessingMode('batch')}
                                className={`px-4 py-2 rounded-md text-sm font-medium transition-colors ${
                                    processingMode === 'batch'
                                        ? 'bg-blue-600 text-white'
                                        : 'text-gray-300 hover:text-white'
                                }`}
                            >
                                Batch Processing
                            </button>
                        </div>
                    </div>
                </div>

                <div className="flex-1 overflow-auto">
                    {processingMode === 'single' ? (
                        <ImageViewer 
                            image={displayedImage} // Pass the processed image to the viewer
                            onImageUpload={handleImageUpload} 
                            isProcessing={isProcessing}
                        />
                    ) : (
                        <div className="p-6 space-y-6">
                            <BatchProcessor
                                onProcessingStart={handleBatchProcessingStart}
                                onProcessingComplete={handleBatchProcessingComplete}
                                onError={handleBatchError}
                            />
                            <BatchResultsViewer results={batchResults} />
                        </div>
                    )}
                </div>
            </main>
            
            {processingMode === 'single' && (
                <aside className="w-[380px] bg-gray-900 border-l border-gray-700 flex flex-col">
                    <ControlPanel
                        onImageUpload={handleImageUpload}
                        onSaveImage={handleSaveImage}
                        onResetAll={handleResetAll}
                        processingMethod={processingMethod}
                        onProcessingMethodChange={handleProcessingMethodChange}
                        isGpuAvailable={isGpuAvailable}
                        activeFilters={activeFilters}
                        onFilterToggle={handleFilterToggle}
                        filterIntensities={filterIntensities}
                        onIntensityChange={handleIntensityChange}
                        isProcessing={isProcessing}
                    />
                </aside>
            )}
        </div>
    );
};

export default App;
