import React, { useRef, useEffect } from 'react';
import type { LogEntry } from '../types';
import { ChevronDownIcon } from './icons/ChevronDownIcon';

interface PerformanceLogProps {
    isVisible: boolean;
    entries: LogEntry[];
    onClose?: () => void;
}

export const PerformanceLog: React.FC<PerformanceLogProps> = ({ isVisible, entries, onClose }) => {
    const logContainerRef = useRef<HTMLDivElement>(null);

    useEffect(() => {
        if (logContainerRef.current) {
            logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;
        }
    }, [entries]);

    return (
        <div className={`absolute bottom-0 left-0 right-0 h-1/3 bg-gray-900/95 backdrop-blur-sm border-t border-gray-700 shadow-2xl transition-transform duration-300 ease-in-out ${isVisible ? 'translate-y-0' : 'translate-y-full'}`}>
            <div className="p-4 h-full flex flex-col">
                <div className="flex justify-between items-center mb-3 flex-shrink-0">
                    <h2 className="text-xl font-bold text-gray-100">Performance Log</h2>
                    {onClose && (
                        <button
                            onClick={onClose}
                            className="text-gray-400 hover:text-gray-200 transition-colors duration-200 p-1 rounded-lg hover:bg-gray-800"
                            title="Hide Performance Log"
                        >
                            <ChevronDownIcon />
                        </button>
                    )}
                </div>
                <div ref={logContainerRef} className="flex-grow overflow-y-auto pr-2">
                    <p className="text-xs text-gray-500 mb-2 italic text-center">
                        Note: Times are reported from the C++ backend.
                    </p>
                    <ul className="space-y-1 font-mono text-sm">
                        {entries.map(entry => (
                            <li key={entry.id} className={entry.isSeparator ? 'text-center text-yellow-400 pt-2 pb-1' : 'text-gray-400'}>
                                {entry.message}
                            </li>
                        ))}
                    </ul>
                </div>
            </div>
        </div>
    );
};
