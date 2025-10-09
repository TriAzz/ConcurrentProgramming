import React, { useState } from 'react';
import { BatchResult } from '../types';

interface BatchResultsViewerProps {
    results: BatchResult[];
}

//UTILITY FUNCTIONS SECTION ----------------------------------------------------

//Utility function to calculate base64 image size in bytes
const calculateImageSize = (base64String: string): number => {
    //Remove data URL prefix
    const base64Data = base64String.split(',')[1] || base64String;
    //Calculate actual size: base64 is ~4/3 of actual size, accounting for padding
    const padding = (base64Data.match(/=/g) || []).length;
    return Math.floor((base64Data.length * 3) / 4) - padding;
};

// Format bytes to human readable format
const formatFileSize = (bytes: number): string => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
};

//MAIN COMPONENT SECTION ----------------------------------------------------

export const BatchResultsViewer: React.FC<BatchResultsViewerProps> = ({ results }) => {
    //STATE MANAGEMENT SECTION ----------------------------------------------------
    
    const [selectedResult, setSelectedResult] = useState<BatchResult | null>(null);
    const [viewMode, setViewMode] = useState<'grid' | 'list'>('grid');

    //Check if results are saved to disk
    const isDiskMode = results.length > 0 && results[0].filePath && !results[0].processedImage;

    if (results.length === 0) {
        return (
            <div className="p-6 bg-gray-900 border border-gray-700 rounded-lg shadow-lg">
                <h3 className="text-lg font-semibold text-white mb-4">Batch Results</h3>
                <p className="text-gray-400">No results to display. Run batch processing first.</p>
            </div>
        );
    }

    if (isDiskMode) {
        return (
            <div className="p-6 bg-gray-900 border border-gray-700 rounded-lg shadow-lg">
                <div className="flex justify-between items-center mb-6">
                    <h3 className="text-lg font-semibold text-white">
                        Batch Results ({results.length} images saved to disk)
                    </h3>
                    <div className="flex gap-2">
                        <button
                            onClick={() => {
                                const firstResult = results[0];
                                if (firstResult.filePath) {
                                    const folderPath = firstResult.filePath.substring(0, firstResult.filePath.lastIndexOf('/'));
                                    alert(`Images saved to: ${folderPath}\n\nYou can now access your processed images from this folder.`);
                                }
                            }}
                            className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors"
                        >
                            📂 Open Folder Location
                        </button>
                    </div>
                </div>
                
                <div className="space-y-3">
                    <div className="bg-gray-800 border border-gray-600 rounded-lg p-4">
                        <h4 className="text-white font-medium mb-3">Processing Summary</h4>
                        <div className="space-y-2 text-sm">
                            <div className="flex justify-between">
                                <span className="text-gray-400">Total Images:</span>
                                <span className="text-white">{results.length}</span>
                            </div>
                            <div className="flex justify-between">
                                <span className="text-gray-400">Average Processing Time:</span>
                                <span className="text-white">
                                    {(results.reduce((sum, r) => sum + r.processingTime, 0) / results.length).toFixed(1)}ms
                                </span>
                            </div>
                            <div className="flex justify-between">
                                <span className="text-gray-400">Total Processing Time:</span>
                                <span className="text-white">
                                    {(results.reduce((sum, r) => sum + r.processingTime, 0) / 1000).toFixed(1)}s
                                </span>
                            </div>
                        </div>
                    </div>
                    
                    <div className="bg-gray-800 border border-gray-600 rounded-lg p-4 max-h-60 overflow-y-auto">
                        <h4 className="text-white font-medium mb-3">Generated Files</h4>
                        <div className="space-y-1 text-sm">
                            {results.slice(0, 50).map((result, index) => (
                                <div key={index} className="flex justify-between items-center py-1">
                                    <span className="text-gray-300 font-mono text-xs">
                                        {result.filePath?.split('/').pop() || `File ${index + 1}`}
                                    </span>
                                    <span className="text-gray-400">
                                        {result.processingTime.toFixed(1)}ms
                                    </span>
                                </div>
                            ))}
                            {results.length > 50 && (
                                <div className="text-gray-500 text-center py-2">
                                    ... and {results.length - 50} more files
                                </div>
                            )}
                        </div>
                    </div>
                </div>
            </div>
        );
    }

    const downloadImage = (result: BatchResult, index: number) => {
        const link = document.createElement('a');
        link.href = result.processedImage;
        link.download = `batch_result_${index + 1}.jpg`;
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
    };

    const downloadAll = async () => {
        if (results.length === 0) return;
        
        try {
            //Calculate total size
            const totalSize = results.reduce((sum, result) => {
                return sum + calculateImageSize(result.processedImage);
            }, 0);
            
            const formattedSize = formatFileSize(totalSize);
            
            if (!confirm(`This will download ${results.length} images (${formattedSize}). Continue?`)) {
                return;
            }
            
            //Create and download each image with a small delay to prevent browser blocking
            const downloadPromises = results.map((result, index) => {
                return new Promise<void>((resolve) => {
                    setTimeout(() => {
                        const link = document.createElement('a');
                        link.href = result.processedImage;
                        
                        //Create descriptive filename
                        const filters = result.appliedFilters.map(f => `${f.name}${f.intensity}`).join('_');
                        link.download = `batch_${index + 1}_${filters}.jpg`;
                        
                        document.body.appendChild(link);
                        link.click();
                        document.body.removeChild(link);
                        resolve();
                    }, index * 50); // 50ms delay between downloads
                });
            });
            
            await Promise.all(downloadPromises);
            
        } catch (error) {
            //Error downloading files - handled gracefully
            alert('Error occurred while downloading files. Please try again.');
        }
    };

    //Calculate total file size for display
    const totalSize = results.reduce((sum, result) => {
        return sum + calculateImageSize(result.processedImage);
    }, 0);
    const formattedTotalSize = formatFileSize(totalSize);

    //COMPONENT RENDER SECTION ----------------------------------------------------

    return (
        <div className="p-6 bg-gray-900 border border-gray-700 rounded-lg shadow-lg">
            <div className="flex justify-between items-center mb-6">
                <h3 className="text-lg font-semibold text-white">
                    Batch Results ({results.length} images)
                </h3>
                <div className="flex gap-2">
                    <button
                        onClick={() => setViewMode(viewMode === 'grid' ? 'list' : 'grid')}
                        className="px-4 py-2 bg-gray-700 text-white rounded-lg hover:bg-gray-600 transition-colors border border-gray-600"
                    >
                        {viewMode === 'grid' ? 'List View' : 'Grid View'}
                    </button>
                    <button
                        onClick={downloadAll}
                        className="px-4 py-2 bg-green-600 text-white rounded-lg hover:bg-green-700 transition-colors"
                        title={`Download all ${results.length} images (${formattedTotalSize})`}
                    >
                        Download All ({formattedTotalSize})
                    </button>
                </div>
            </div>

            {viewMode === 'grid' ? (
                <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-4">
                    {results.map((result, index) => (
                        <div
                            key={index}
                            className="border border-gray-600 bg-gray-800 rounded-lg overflow-hidden hover:shadow-lg hover:border-gray-500 transition-all cursor-pointer"
                            onClick={() => setSelectedResult(result)}
                        >
                            <img
                                src={result.processedImage}
                                alt={`Result ${index + 1}`}
                                className="w-full h-32 object-cover"
                            />
                            <div className="p-2">
                                <div className="text-xs text-gray-400 mb-1">
                                    Combination #{result.combinationIndex + 1}
                                </div>
                                <div className="text-xs text-gray-500">
                                    {result.processingTime.toFixed(1)}ms
                                </div>
                                <div className="text-xs text-gray-300 mt-1">
                                    {result.appliedFilters.map(f => f.name).join(' + ')}
                                </div>
                            </div>
                        </div>
                    ))}
                </div>
            ) : (
                <div className="space-y-3">
                    {results.map((result, index) => (
                        <div
                            key={index}
                            className="flex items-center gap-4 p-4 border border-gray-600 bg-gray-800 rounded-lg hover:bg-gray-750 cursor-pointer"
                            onClick={() => setSelectedResult(result)}
                        >
                            <img
                                src={result.processedImage}
                                alt={`Result ${index + 1}`}
                                className="w-16 h-16 object-cover rounded"
                            />
                            <div className="flex-1">
                                <div className="font-medium text-white">Combination #{result.combinationIndex + 1}</div>
                                <div className="text-sm text-gray-400">
                                    Filters: {result.appliedFilters.map(f => `${f.name} (${f.intensity}%)`).join(', ')}
                                </div>
                                <div className="text-sm text-gray-500">
                                    Processing time: {result.processingTime.toFixed(1)}ms
                                </div>
                            </div>
                            <button
                                onClick={(e) => {
                                    e.stopPropagation();
                                    downloadImage(result, index);
                                }}
                                className="px-3 py-1 bg-blue-600 text-white text-sm rounded hover:bg-blue-700 transition-colors"
                            >
                                Download
                            </button>
                        </div>
                    ))}
                </div>
            )}

            {/* Modal for viewing individual result */}
            {selectedResult && (
                <div className="fixed inset-0 bg-black bg-opacity-75 flex items-center justify-center z-50">
                    <div className="bg-gray-900 border border-gray-600 rounded-lg max-w-4xl max-h-[90vh] overflow-auto">
                        <div className="p-6">
                            <div className="flex justify-between items-center mb-4">
                                <h4 className="text-lg font-semibold text-white">
                                    Combination #{selectedResult.combinationIndex + 1}
                                </h4>
                                <button
                                    onClick={() => setSelectedResult(null)}
                                    className="text-gray-400 hover:text-gray-200 text-2xl"
                                >
                                    ÁE
                                </button>
                            </div>
                            
                            <img
                                src={selectedResult.processedImage}
                                alt="Full size result"
                                className="max-w-full max-h-96 mx-auto rounded-lg mb-4 border border-gray-600"
                            />
                            
                            <div className="space-y-2 text-white">
                                <div><strong>Processing Time:</strong> {selectedResult.processingTime.toFixed(2)}ms</div>
                                <div><strong>Applied Filters:</strong></div>
                                <ul className="ml-4 space-y-1">
                                    {selectedResult.appliedFilters.map((filter, idx) => (
                                        <li key={idx} className="text-sm text-gray-300">
                                            • {filter.name}: {filter.intensity}%
                                        </li>
                                    ))}
                                </ul>
                            </div>
                            
                            <div className="flex justify-end gap-2 mt-6">
                                <button
                                    onClick={() => downloadImage(selectedResult, selectedResult.combinationIndex)}
                                    className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors"
                                >
                                    Download
                                </button>
                                <button
                                    onClick={() => setSelectedResult(null)}
                                    className="px-4 py-2 bg-gray-700 text-white rounded-lg hover:bg-gray-600 transition-colors border border-gray-600"
                                >
                                    Close
                                </button>
                            </div>
                        </div>
                    </div>
                </div>
            )}
        </div>
    );
};
