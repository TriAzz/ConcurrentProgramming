//============================================================================
// VITE BUILD CONFIGURATION - DEVELOPMENT AND PRODUCTION SETUP
//============================================================================
// Vite configuration for the React TypeScript image processor frontend
// Includes development server settings, environment variable handling,
// React plugin integration, and path alias configuration
//============================================================================

import path from 'path';
import { defineConfig, loadEnv } from 'vite';
import react from '@vitejs/plugin-react';

//Function to configure Vite build settings based on environment mode
export default defineConfig(({ mode }) => {
    //Load environment variables from .env files
    const env = loadEnv(mode, '.', '');
    
    return {
        //Development server configuration
        server: {
            port: 3000,
            host: '0.0.0.0',
        },
        
        //Plugin configuration for React support
        plugins: [react()],
        
        //Environment variable definitions for client-side access
        define: {
            'process.env.API_KEY': JSON.stringify(env.GEMINI_API_KEY),
            'process.env.GEMINI_API_KEY': JSON.stringify(env.GEMINI_API_KEY)
        },
        
        //Module resolution configuration with path aliases
        resolve: {
            alias: {
                '@': path.resolve(__dirname, '.'),
            }
        }
    };
});
