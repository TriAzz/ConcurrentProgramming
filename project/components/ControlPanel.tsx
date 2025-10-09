import React, { useRef } from 'react';
import { FilterGroup } from './FilterGroup';
import { FolderOpenIcon } from './icons/FolderOpenIcon';
import { SaveIcon } from './icons/SaveIcon';
import { ResetIcon } from './icons/ResetIcon';
import { ChevronUpIcon } from './icons/ChevronUpIcon';
import { ChevronDownIcon } from './icons/ChevronDownIcon';
import { FILTERS } from '../constants';
import type { ActiveFilters, FilterIntensities } from '../types';
import { FilterCategory, ProcessingMethod } from '../types';

interface ControlPanelProps {
    onImageUpload: (file: File) => void;
    onSaveImage: () => void;
    onResetAll: () => void;
    processingMethod: ProcessingMethod;
    onProcessingMethodChange: (method: ProcessingMethod) => void;
    isGpuAvailable: boolean;
    activeFilters: ActiveFilters;
    onFilterToggle: (filterName: string, category: FilterCategory) => void;
    filterIntensities: FilterIntensities;
    onIntensityChange: (filterName: string, value: number) => void;
    isProcessing: boolean;
}

export const ControlPanel: React.FC<ControlPanelProps> = (props) => {
    //STATE MANAGEMENT SECTION ----------------------------------------------------
    
    const fileInputRef = useRef<HTMLInputElement>(null);

    //EVENT HANDLERS SECTION ----------------------------------------------------

    const handleFileOpenClick = () => {
        fileInputRef.current?.click();
    };

    const handleFileSelected = (e: React.ChangeEvent<HTMLInputElement>) => {
        const files = e.target.files;
        if (files && files.length > 0) {
            props.onImageUpload(files[0]);
        }
        e.target.value = ''; // Reset file input
    };

    const colorFilters = FILTERS.filter(f => f.category === FilterCategory.Color);
    const effectFilters = FILTERS.filter(f => f.category === FilterCategory.Effect);

    const disabledClass = "disabled:opacity-50 disabled:cursor-not-allowed";

    //COMPONENT RENDER SECTION ----------------------------------------------------

    return (
        <div className="p-6 flex flex-col h-full overflow-y-auto">
            <h1 className="text-2xl font-bold text-gray-100 mb-6">Controls</h1>
            
            <div className="grid grid-cols-2 gap-3 mb-6">
                <button onClick={handleFileOpenClick} disabled={props.isProcessing} className={`flex items-center justify-center gap-2 bg-indigo-600 hover:bg-indigo-500 text-white font-semibold py-2 px-4 rounded-md transition-colors ${disabledClass}`}>
                    <FolderOpenIcon /> Open Image...
                </button>
                <input type="file" ref={fileInputRef} onChange={handleFileSelected} accept="image/*" className="hidden" />
                
                <button onClick={props.onSaveImage} disabled={props.isProcessing} className={`flex items-center justify-center gap-2 bg-gray-700 hover:bg-gray-600 text-white font-semibold py-2 px-4 rounded-md transition-colors ${disabledClass}`}>
                    <SaveIcon /> Save As...
                </button>
            </div>
            
            <div className="flex-grow space-y-6">
                <FilterGroup 
                    title={FilterCategory.Color}
                    filters={colorFilters}
                    activeFilter={props.activeFilters[FilterCategory.Color]}
                    onFilterToggle={(name) => props.onFilterToggle(name, FilterCategory.Color)}
                    filterIntensities={props.filterIntensities}
                    onIntensityChange={props.onIntensityChange}
                    isDisabled={props.isProcessing}
                />
                
                <FilterGroup 
                    title={FilterCategory.Effect}
                    filters={effectFilters}
                    activeFilter={props.activeFilters[FilterCategory.Effect]}
                    onFilterToggle={(name) => props.onFilterToggle(name, FilterCategory.Effect)}
                    filterIntensities={props.filterIntensities}
                    onIntensityChange={props.onIntensityChange}
                    isDisabled={props.isProcessing}
                />
            </div>
            
            <div className="mt-6 border-t border-gray-700 pt-6">
                <div className="flex items-center justify-between mb-4">
                    <h3 className="text-lg font-semibold text-gray-300">Settings</h3>
                     <button onClick={props.onResetAll} disabled={props.isProcessing} className={`flex items-center gap-2 text-sm text-indigo-400 hover:text-indigo-300 transition-colors ${disabledClass}`}>
                        <ResetIcon /> Reset All
                    </button>
                </div>
                
                <div className="space-y-3">
                    <label className="block text-sm font-medium text-gray-400">Processing Method</label>
                    <div className="flex flex-col gap-3">
                        <label className={`flex items-center gap-2 ${props.isProcessing ? 'cursor-not-allowed' : 'cursor-pointer'}`}>
                            <input type="radio" name="processing-method" value={ProcessingMethod.GPU}
                                checked={props.processingMethod === ProcessingMethod.GPU}
                                onChange={() => props.onProcessingMethodChange(ProcessingMethod.GPU)}
                                disabled={!props.isGpuAvailable || props.isProcessing}
                                className="form-radio bg-gray-800 border-gray-600 text-indigo-500 focus:ring-indigo-500"
                            />
                            <span className={props.isGpuAvailable ? '' : 'text-gray-500'}>GPU (OpenCL)</span>
                        </label>
                        <label className={`flex items-center gap-2 ${props.isProcessing ? 'cursor-not-allowed' : 'cursor-pointer'}`}>
                            <input type="radio" name="processing-method" value={ProcessingMethod.CPU}
                                checked={props.processingMethod === ProcessingMethod.CPU}
                                onChange={() => props.onProcessingMethodChange(ProcessingMethod.CPU)}
                                disabled={props.isProcessing}
                                className="form-radio bg-gray-800 border-gray-600 text-indigo-500 focus:ring-indigo-500"
                            />
                            <span>CPU (OpenMP)</span>
                        </label>
                        <label className={`flex items-center gap-2 ${props.isProcessing ? 'cursor-not-allowed' : 'cursor-pointer'}`}>
                            <input type="radio" name="processing-method" value={ProcessingMethod.RECOMMENDED}
                                checked={props.processingMethod === ProcessingMethod.RECOMMENDED}
                                onChange={() => props.onProcessingMethodChange(ProcessingMethod.RECOMMENDED)}
                                disabled={!props.isGpuAvailable || props.isProcessing}
                                className="form-radio bg-gray-800 border-gray-600 text-blue-500 focus:ring-blue-500"
                            />
                            <span className="text-blue-400 font-medium">Recommended</span>
                        </label>
                    </div>
                    {!props.isGpuAvailable && <p className="text-xs text-yellow-500">No compatible GPU detected. CPU is default.</p>}
                    {props.processingMethod === ProcessingMethod.RECOMMENDED && (
                        <p className="text-xs text-blue-400">
                            Intelligently analyzes your image and filter type to automatically select the optimal processing method (GPU or CPU) for best performance.
                        </p>
                    )}
                </div>
            </div>
        </div>
    );
};
