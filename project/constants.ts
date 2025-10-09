//============================================================================
// FILTER CONFIGURATION CONSTANTS - IMAGE PROCESSING DEFINITIONS
//============================================================================
// Centralized filter definitions with metadata for both frontend UI
// and backend processing. Includes slider configurations, default values,
// and categorization for organized user interface presentation
//============================================================================

import { Filter, FilterCategory } from './types';

//FILTER DEFINITIONS SECTION ----------------------------------------------------

//Array of all available image filters with their configuration properties
export const FILTERS: Filter[] = [
    //Color adjustment filters (non-parameterized)
    {
        name: 'Grayscale',
        category: FilterCategory.Color,
        hasSlider: false,
    },
    {
        name: 'Sepia',
        category: FilterCategory.Color,
        hasSlider: false,
    },
    {
        name: 'Invert',
        category: FilterCategory.Color,
        hasSlider: false,
    },
    
    //Color adjustment filters (parameterized with intensity sliders)
    {
        name: 'Saturation',
        category: FilterCategory.Color,
        hasSlider: true,
        slider: { min: 0, max: 200, step: 1, defaultValue: 100, unit: '%' },
    },
    {
        name: 'Blur',
        category: FilterCategory.Effect,
        hasSlider: true,
        slider: { min: 0, max: 100, step: 1, defaultValue: 0, unit: '%' },
    },
    {
        name: 'Pixelate',
        category: FilterCategory.Effect,
        hasSlider: true,
        slider: { min: 0, max: 100, step: 1, defaultValue: 0, unit: '%' },
    },
    {
        name: 'Brightness',
        category: FilterCategory.Color,
        hasSlider: true,
        slider: { min: 0, max: 200, step: 1, defaultValue: 100, unit: '%' },
    },
    {
        name: 'Contrast',
        category: FilterCategory.Color,
        hasSlider: true,
        slider: { min: 0, max: 200, step: 1, defaultValue: 100, unit: '%' },
    },
    {
        name: 'Edge Detection',
        category: FilterCategory.Effect,
        hasSlider: true,
        slider: { min: 10, max: 200, step: 10, defaultValue: 100, unit: '%' },
    },
    {
        name: 'Emboss',
        category: FilterCategory.Effect,
        hasSlider: true,
        slider: { min: 50, max: 200, step: 10, defaultValue: 100, unit: '%' },
    },
    {
        name: 'Sharpen',
        category: FilterCategory.Effect,
        hasSlider: true,
        slider: { min: 10, max: 200, step: 10, defaultValue: 100, unit: '%' },
    },
    {
        name: 'Median Blur',
        category: FilterCategory.Effect,
        hasSlider: true,
        slider: { min: 10, max: 100, step: 10, defaultValue: 50, unit: '%' },
    },
    {
        name: 'Morphological Open',
        category: FilterCategory.Effect,
        hasSlider: true,
        slider: { min: 20, max: 100, step: 10, defaultValue: 40, unit: '%' },
    },
    {
        name: 'Bilateral Filter',
        category: FilterCategory.Effect,
        hasSlider: true,
        slider: { min: 10, max: 100, step: 10, defaultValue: 50, unit: '%' },
    },
];
