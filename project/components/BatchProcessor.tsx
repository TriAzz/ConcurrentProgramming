import React, { useState, useRef } from 'react';
import { 
    ProcessingMethod, 
    FilterCategory, 
    BatchFilterSelection, 
    BatchFilterRanges, 
    BatchRequest, 
    BatchResponse,
    BatchResult,
    FilterRange
} from '../types';
import { FILTERS } from '../constants';

interface BatchProcessorProps {
    onProcessingStart: () => void;
    onProcessingComplete: (results: BatchResult[]) => void;
    onError: (error: string) => void;
}

export const BatchProcessor = ({
    onProcessingStart,
    onProcessingComplete,
    onError
}) => {
    //STATE MANAGEMENT SECTION ----------------------------------------------------
    
    const [selectedImage, setSelectedImage] = useState<string | null>(null);
    const [selectedFilters, setSelectedFilters] = useState<BatchFilterSelection>({
        colorFilters: [],
        effectFilters: []
    });
    const [filterRanges, setFilterRanges] = useState<BatchFilterRanges>({});
    const [processingMethod, setProcessingMethod] = useState<ProcessingMethod>(ProcessingMethod.RECOMMENDED);
    const [isProcessing, setIsProcessing] = useState(false);
    const [saveToDisk, setSaveToDisk] = useState<boolean>(false);
    const fileInputRef = useRef<HTMLInputElement>(null);

    //Fixed output path to project's output_images folder
    const outputPath = "output_images";

    const colorFilters = FILTERS.filter(f => f.category === FilterCategory.Color);
    const effectFilters = FILTERS.filter(f => f.category === FilterCategory.Effect);

    //EVENT HANDLERS SECTION ----------------------------------------------------

    const handleImageUpload = (event: any) => {
        const file = event.target.files?.[0];
        if (file) {
            const reader = new FileReader();
            reader.onload = (e) => {
                setSelectedImage(e.target?.result as string);
                //Image uploaded successfully
            };
            reader.readAsDataURL(file);
        }
    };

    const handleFilterToggle = (filterName: string, category: FilterCategory) => {
        const categoryKey = category === FilterCategory.Color ? 'colorFilters' : 'effectFilters';
        
        setSelectedFilters(prev => {
            const currentFilters = [...prev[categoryKey]];
            const index = currentFilters.indexOf(filterName);
            
            if (index >= 0) {
                //Remove filter
                currentFilters.splice(index, 1);
                //Remove its range
                setFilterRanges(prevRanges => {
                    const newRanges = { ...prevRanges };
                    delete newRanges[filterName];
                    return newRanges;
                });
            } else {
                //Add filter
                currentFilters.push(filterName);
                //Set default range
                const filter = FILTERS.find(f => f.name === filterName);
                if (filter?.hasSlider && filter.slider) {
                    setFilterRanges(prevRanges => ({
                        ...prevRanges,
                        [filterName]: {
                            min: filter.slider!.defaultValue,
                            max: filter.slider!.defaultValue,
                            increment: filter.slider!.step
                        }
                    }));
                }
            }
            
            return {
                ...prev,
                [categoryKey]: currentFilters
            };
        });
    };

    const handleRangeChange = (filterName: string, field: keyof FilterRange, value: number) => {
        setFilterRanges(prev => ({
            ...prev,
            [filterName]: {
                ...prev[filterName],
                [field]: value
            }
        }));
    };

    const handleInputBlur = (filterName: string, field: keyof FilterRange, value: string) => {
        const numValue = parseFloat(value);
        if (!isNaN(numValue)) {
            handleRangeChange(filterName, field, numValue);
        } else {
            //Reset to current value if invalid
            const filter = FILTERS.find(f => f.name === filterName);
            if (filter?.hasSlider && filter.slider) {
                handleRangeChange(filterName, field, filter.slider.defaultValue);
            }
        }
    };

    const handleInputKeyPress = (e: any, filterName: string, field: keyof FilterRange, value: string) => {
        if (e.key === 'Enter') {
            handleInputBlur(filterName, field, value);
        }
    };

    const handleSingleValueToggle = (filterName: string, useSingleValue: boolean) => {
        setFilterRanges(prev => ({
            ...prev,
            [filterName]: {
                ...prev[filterName],
                useSingleValue
            }
        }));
    };

    //CALCULATION FUNCTIONS SECTION ----------------------------------------------------

    const getTotalCombinations = () => {
        //Calculate combinations as: (color filter variations) × (effect filter variations)
        //Each image gets exactly ONE color filter AND ONE effect filter applied
        
        let colorVariations = 0;
        let effectVariations = 0;
        
        //Count all color filter variations (sliders + non-sliders)
        selectedFilters.colorFilters.forEach(filterName => {
            const filter = FILTERS.find(f => f.name === filterName);
            const range = filterRanges[filterName];
            
            if (filter?.hasSlider && range) {
                //Filter with slider
                if (range.useSingleValue) {
                    //Single value mode - only use min value
                    colorVariations += 1;
                } else {
                    //Range mode - add all possible intensity values
                    const values = Math.floor((range.max - range.min) / range.increment) + 1;
                    colorVariations += values;
                }
            } else {
                //Non-slider filter (Sepia, Greyscale, Invert) - each counts as 1 variation
                colorVariations += 1;
            }
        });
        
        //Count all effect filter variations (sliders + non-sliders)  
        selectedFilters.effectFilters.forEach(filterName => {
            const filter = FILTERS.find(f => f.name === filterName);
            const range = filterRanges[filterName];
            
            if (filter?.hasSlider && range) {
                //Filter with slider
                if (range.useSingleValue) {
                    //Single value mode - only use min value
                    effectVariations += 1;
                } else {
                    //Range mode - add all possible intensity values
                    const values = Math.floor((range.max - range.min) / range.increment) + 1;
                    effectVariations += values;
                }
            } else {
                //Non-slider filter - each counts as 1 variation
                effectVariations += 1;
            }
        });
        
        //Total combinations = color variations × effect variations
        //Each image gets one color filter AND one effect filter
        return Math.max(1, colorVariations) * Math.max(1, effectVariations);
    };

    //BATCH PROCESSING SECTION ----------------------------------------------------

    const handleProcess = async () => {
        if (!selectedImage) {
            onError('Please select an image first');
            return;
        }

        if (selectedFilters.colorFilters.length === 0 && selectedFilters.effectFilters.length === 0) {
            onError('Please select at least one filter');
            return;
        }

        const totalCombinations = getTotalCombinations();
        const forceDiskStorage = totalCombinations >= 100;
        const willUseDiskStorage = forceDiskStorage || saveToDisk;
        
        //Check if folder is required but not selected
        //Note: Output folder is now fixed to "output_images"
        
        if (totalCombinations > 100) {
            const storageInfo = willUseDiskStorage ? 
                ` Images will be saved to: ${outputPath}` : 
                ' Images will be displayed in the browser.';
            if (!confirm(`This will generate ${totalCombinations} images.${storageInfo} Continue?`)) {
                return;
            }
        }

        setIsProcessing(true);
        onProcessingStart();
        //Starting batch processing

        const request: BatchRequest = {
            imageData: selectedImage,
            method: processingMethod,
            selectedFilters,
            filterRanges,
            saveToDisk: willUseDiskStorage,
            outputPath: willUseDiskStorage ? outputPath : undefined
        };

        try {
            const response = await fetch('http://localhost:8080/batch', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(request),
            });

            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(`Backend Error: ${response.status} - ${errorText || response.statusText}`);
            }

            const result: BatchResponse = await response.json();
            
            //Batch processing completed successfully
            
            onProcessingComplete(result.results);
        } catch (error) {
            onError(`Batch processing failed: ${error}`);
        } finally {
            setIsProcessing(false);
        }
    };

    //COMPONENT RENDER SECTION ----------------------------------------------------

    return (
        <div className="p-6 bg-gray-900 rounded-lg shadow-lg border border-gray-700">
            <h2 className="text-2xl font-bold mb-6 text-white">Batch Filter Processor</h2>
            
            {/* Image Upload */}
            <div className="mb-6">
                <label className="block text-sm font-medium text-gray-300 mb-2">
                    Upload Image
                </label>
                <div className="flex items-center gap-4">
                    <button
                        onClick={() => fileInputRef.current?.click()}
                        className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors"
                    >
                        📁 Choose Image
                    </button>
                    <input
                        ref={fileInputRef}
                        type="file"
                        accept="image/*"
                        onChange={handleImageUpload}
                        className="hidden"
                    />
                    {selectedImage && (
                        <span className="text-sm text-green-400">Image selected ✓</span>
                    )}
                </div>
                {selectedImage && (
                    <div className="mt-4">
                        <img 
                            src={selectedImage} 
                            alt="Selected" 
                            className="max-w-xs max-h-48 rounded-lg border border-gray-600"
                        />
                    </div>
                )}
            </div>

            {/* Processing Method */}
            <div className="mb-6">
                <label className="block text-sm font-medium text-gray-300 mb-2">
                    Processing Method
                </label>
                <select
                    value={processingMethod}
                    onChange={(e) => setProcessingMethod(e.target.value as ProcessingMethod)}
                    className="w-full p-2 bg-gray-800 border border-gray-600 rounded-lg text-white focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                >
                    {Object.values(ProcessingMethod).map((method) => (
                        <option key={method} value={method}>
                            {method}
                        </option>
                    ))}
                </select>
            </div>

            {/* Storage Options */}
            <div className="mb-6">
                <label className="block text-sm font-medium text-gray-300 mb-2">
                    Storage Options
                </label>
                <div className="space-y-3">
                    <div className="text-sm text-gray-400">
                        {getTotalCombinations() >= 100 ? (
                            <span className="text-yellow-400">
                                Large batch detected ({getTotalCombinations().toLocaleString()} combinations). 
                                Images will be automatically saved to disk.
                            </span>
                        ) : (
                            <span>
                                Small batch ({getTotalCombinations().toLocaleString()} combinations). 
                                Choose storage method below.
                            </span>
                        )}
                    </div>
                    
                    {getTotalCombinations() < 100 && (
                        <div className="space-y-2">
                            <label className="flex items-center gap-2">
                                <input
                                    type="radio"
                                    name="storageMode"
                                    checked={!saveToDisk}
                                    onChange={() => setSaveToDisk(false)}
                                    className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 focus:ring-blue-500"
                                />
                                <span className="text-white">Display in browser (faster preview)</span>
                            </label>
                            <label className="flex items-center gap-2">
                                <input
                                    type="radio"
                                    name="storageMode"
                                    checked={saveToDisk}
                                    onChange={() => setSaveToDisk(true)}
                                    className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 focus:ring-blue-500"
                                />
                                <span className="text-white">Save to disk (better for large batches)</span>
                            </label>
                        </div>
                    )}
                    
                    {(saveToDisk || getTotalCombinations() >= 100) && (
                        <div className="mt-3">
                            <div className="text-sm text-gray-400 mb-2">
                                📁 Images will be saved to: <span className="text-green-400 font-mono">output_images/</span>
                            </div>
                            <div className="text-xs text-gray-500">
                                💡 Output folder is automatically created in the project directory
                            </div>
                        </div>
                    )}
                </div>
            </div>

            {/* Color Filters */}
            <div className="mb-6">
                <h3 className="text-lg font-semibold text-white mb-3">Color Filters</h3>
                <div className="space-y-3">
                    {colorFilters.map((filter) => (
                        <div key={filter.name} className="border border-gray-600 rounded-lg p-4 bg-gray-800">
                            <label className="flex items-center gap-2 mb-2">
                                <input
                                    type="checkbox"
                                    checked={selectedFilters.colorFilters.includes(filter.name)}
                                    onChange={() => handleFilterToggle(filter.name, FilterCategory.Color)}
                                    className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
                                />
                                <span className="font-medium text-white">{filter.name}</span>
                            </label>
                            
                            {selectedFilters.colorFilters.includes(filter.name) && filter.hasSlider && filter.slider && (
                                <div className="ml-6 space-y-2">
                                    <label className="flex items-center space-x-2">
                                        <input
                                            type="checkbox"
                                            checked={filterRanges[filter.name]?.useSingleValue || false}
                                            onChange={(e) => handleSingleValueToggle(filter.name, e.target.checked)}
                                            className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
                                        />
                                        <span className="text-sm text-gray-300">Use single value (ignore range)</span>
                                    </label>
                                    
                                    {filterRanges[filter.name]?.useSingleValue ? (
                                        <div>
                                            <label className="block text-xs text-gray-400">Value</label>
                                            <input
                                                type="number"
                                                min={filter.slider.min}
                                                max={filter.slider.max}
                                                step={filter.slider.step}
                                                defaultValue={filterRanges[filter.name]?.min || filter.slider.defaultValue}
                                                onBlur={(e) => handleInputBlur(filter.name, 'min', e.target.value)}
                                                onKeyPress={(e) => handleInputKeyPress(e, filter.name, 'min', e.currentTarget.value)}
                                                className="w-full p-1 text-sm bg-gray-700 border border-gray-600 rounded text-white focus:ring-1 focus:ring-blue-500"
                                            />
                                        </div>
                                    ) : (
                                        <div className="grid grid-cols-3 gap-2">
                                            <div>
                                                <label className="block text-xs text-gray-400">Min</label>
                                                <input
                                                    type="number"
                                                    min={filter.slider.min}
                                                    max={filter.slider.max}
                                                    step={filter.slider.step}
                                                    defaultValue={filterRanges[filter.name]?.min || filter.slider.defaultValue}
                                                    onBlur={(e) => handleInputBlur(filter.name, 'min', e.target.value)}
                                                    onKeyPress={(e) => handleInputKeyPress(e, filter.name, 'min', e.currentTarget.value)}
                                                    className="w-full p-1 text-sm bg-gray-700 border border-gray-600 rounded text-white focus:ring-1 focus:ring-blue-500"
                                                />
                                            </div>
                                            <div>
                                                <label className="block text-xs text-gray-400">Max</label>
                                                <input
                                                    type="number"
                                                    min={filter.slider.min}
                                                    max={filter.slider.max}
                                                    step={filter.slider.step}
                                                    defaultValue={filterRanges[filter.name]?.max || filter.slider.defaultValue}
                                                    onBlur={(e) => handleInputBlur(filter.name, 'max', e.target.value)}
                                                    onKeyPress={(e) => handleInputKeyPress(e, filter.name, 'max', e.currentTarget.value)}
                                                    className="w-full p-1 text-sm bg-gray-700 border border-gray-600 rounded text-white focus:ring-1 focus:ring-blue-500"
                                                />
                                            </div>
                                            <div>
                                                <label className="block text-xs text-gray-400">Step</label>
                                                <input
                                                    type="number"
                                                    min={filter.slider.step}
                                                    max={filter.slider.max}
                                                    step={filter.slider.step}
                                                    defaultValue={filterRanges[filter.name]?.increment || filter.slider.step}
                                                    onBlur={(e) => handleInputBlur(filter.name, 'increment', e.target.value)}
                                                    onKeyPress={(e) => handleInputKeyPress(e, filter.name, 'increment', e.currentTarget.value)}
                                                    className="w-full p-1 text-sm bg-gray-700 border border-gray-600 rounded text-white focus:ring-1 focus:ring-blue-500"
                                                />
                                            </div>
                                        </div>
                                    )}
                                </div>
                            )}
                        </div>
                    ))}
                </div>
            </div>

            {/* Effect Filters */}
            <div className="mb-6">
                <h3 className="text-lg font-semibold text-white mb-3">Effect Filters</h3>
                <div className="space-y-3">
                    {effectFilters.map((filter) => (
                        <div key={filter.name} className="border border-gray-600 rounded-lg p-4 bg-gray-800">
                            <label className="flex items-center gap-2 mb-2">
                                <input
                                    type="checkbox"
                                    checked={selectedFilters.effectFilters.includes(filter.name)}
                                    onChange={() => handleFilterToggle(filter.name, FilterCategory.Effect)}
                                    className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
                                />
                                <span className="font-medium text-white">{filter.name}</span>
                            </label>
                            
                            {selectedFilters.effectFilters.includes(filter.name) && filter.hasSlider && filter.slider && (
                                <div className="ml-6 space-y-2">
                                    <label className="flex items-center space-x-2">
                                        <input
                                            type="checkbox"
                                            checked={filterRanges[filter.name]?.useSingleValue || false}
                                            onChange={(e) => handleSingleValueToggle(filter.name, e.target.checked)}
                                            className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
                                        />
                                        <span className="text-sm text-gray-300">Use single value (ignore range)</span>
                                    </label>
                                    
                                    {filterRanges[filter.name]?.useSingleValue ? (
                                        <div>
                                            <label className="block text-xs text-gray-400">Value</label>
                                            <input
                                                type="number"
                                                min={filter.slider.min}
                                                max={filter.slider.max}
                                                step={filter.slider.step}
                                                defaultValue={filterRanges[filter.name]?.min || filter.slider.defaultValue}
                                                onBlur={(e) => handleInputBlur(filter.name, 'min', e.target.value)}
                                                onKeyPress={(e) => handleInputKeyPress(e, filter.name, 'min', e.currentTarget.value)}
                                                className="w-full p-1 text-sm bg-gray-700 border border-gray-600 rounded text-white focus:ring-1 focus:ring-blue-500"
                                            />
                                        </div>
                                    ) : (
                                        <div className="grid grid-cols-3 gap-2">
                                            <div>
                                                <label className="block text-xs text-gray-400">Min</label>
                                                <input
                                                    type="number"
                                                    min={filter.slider.min}
                                                    max={filter.slider.max}
                                                    step={filter.slider.step}
                                                    defaultValue={filterRanges[filter.name]?.min || filter.slider.defaultValue}
                                                    onBlur={(e) => handleInputBlur(filter.name, 'min', e.target.value)}
                                                    onKeyPress={(e) => handleInputKeyPress(e, filter.name, 'min', e.currentTarget.value)}
                                                    className="w-full p-1 text-sm bg-gray-700 border border-gray-600 rounded text-white focus:ring-1 focus:ring-blue-500"
                                                />
                                            </div>
                                            <div>
                                                <label className="block text-xs text-gray-400">Max</label>
                                                <input
                                                    type="number"
                                                    min={filter.slider.min}
                                                    max={filter.slider.max}
                                                    step={filter.slider.step}
                                                    defaultValue={filterRanges[filter.name]?.max || filter.slider.defaultValue}
                                                    onBlur={(e) => handleInputBlur(filter.name, 'max', e.target.value)}
                                                    onKeyPress={(e) => handleInputKeyPress(e, filter.name, 'max', e.currentTarget.value)}
                                                    className="w-full p-1 text-sm bg-gray-700 border border-gray-600 rounded text-white focus:ring-1 focus:ring-blue-500"
                                                />
                                            </div>
                                            <div>
                                                <label className="block text-xs text-gray-400">Step</label>
                                                <input
                                                    type="number"
                                                    min={filter.slider.step}
                                                    max={filter.slider.max}
                                                    step={filter.slider.step}
                                                    defaultValue={filterRanges[filter.name]?.increment || filter.slider.step}
                                                    onBlur={(e) => handleInputBlur(filter.name, 'increment', e.target.value)}
                                                    onKeyPress={(e) => handleInputKeyPress(e, filter.name, 'increment', e.currentTarget.value)}
                                                    className="w-full p-1 text-sm bg-gray-700 border border-gray-600 rounded text-white focus:ring-1 focus:ring-blue-500"
                                                />
                                            </div>
                                        </div>
                                    )}
                                </div>
                            )}
                        </div>
                    ))}
                </div>
            </div>

            {/* Combination Count */}
            <div className="mb-6 p-4 bg-gray-800 border border-gray-600 rounded-lg">
                <div className="flex justify-between items-center">
                    <span className="font-medium text-white">Total Combinations:</span>
                    <span className="text-xl font-bold text-blue-400">{getTotalCombinations()}</span>
                </div>
            </div>

            {/* Process Button */}
            <button
                onClick={handleProcess}
                disabled={isProcessing || !selectedImage}
                className={`w-full py-3 px-6 rounded-lg font-semibold transition-colors ${
                    isProcessing || !selectedImage
                        ? 'bg-gray-600 text-gray-400 cursor-not-allowed'
                        : 'bg-blue-600 text-white hover:bg-blue-700'
                }`}
            >
                {isProcessing ? 'Processing...' : 'Start Batch Processing'}
            </button>
        </div>
    );
};
