import React from 'react';
import type { Filter, FilterIntensities } from '../types';

interface FilterGroupProps {
    title: string;
    filters: Filter[];
    activeFilter: string | null;
    onFilterToggle: (filterName: string) => void;
    filterIntensities: FilterIntensities;
    onIntensityChange: (filterName: string, value: number) => void;
    isDisabled: boolean;
}

export const FilterGroup: React.FC<FilterGroupProps> = ({ title, filters, activeFilter, onFilterToggle, filterIntensities, onIntensityChange, isDisabled }) => {
    return (
        <div>
            <h3 className="text-lg font-semibold text-gray-300 mb-3">{title}</h3>
            <div className="grid grid-cols-2 gap-2">
                {filters.map(filter => {
                    const isActive = activeFilter === filter.name;
                    return (
                        <div key={filter.name} className="col-span-1">
                            <button
                                onClick={() => onFilterToggle(filter.name)}
                                disabled={isDisabled}
                                className={`w-full text-sm font-medium py-2 px-3 rounded-md transition-all duration-150 ${
                                    isActive 
                                    ? 'bg-indigo-600 text-white shadow-md' 
                                    : 'bg-gray-700/50 hover:bg-gray-700 text-gray-300'
                                } disabled:opacity-50 disabled:cursor-not-allowed`}
                            >
                                {filter.name}
                            </button>
                            {isActive && filter.hasSlider && filter.slider && (
                                <div className="mt-2 p-2 bg-gray-800 rounded-md">
                                    <input
                                        type="range"
                                        min={filter.slider.min}
                                        max={filter.slider.max}
                                        step={filter.slider.step}
                                        value={filterIntensities[filter.name] ?? filter.slider.defaultValue}
                                        onChange={(e) => onIntensityChange(filter.name, parseFloat(e.target.value))}
                                        disabled={isDisabled}
                                        className="w-full h-2 bg-gray-600 rounded-lg appearance-none cursor-pointer disabled:bg-gray-700 disabled:cursor-not-allowed"
                                    />
                                    <div className="text-xs text-gray-400 text-center mt-1">
                                        {filterIntensities[filter.name] ?? filter.slider.defaultValue}{filter.slider.unit}
                                    </div>
                                </div>
                            )}
                        </div>
                    );
                })}
            </div>
        </div>
    );
};
