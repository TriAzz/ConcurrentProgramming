import React, { useState, useRef, useEffect } from 'react';

interface ImageViewerProps {
    image: string | null;
    onImageUpload: (file: File) => void;
    isProcessing: boolean;
}

const ZoomControls: React.FC<{ zoom: number; setZoom: (zoom: number) => void; minZoom: number; maxZoom: number }> = ({ zoom, setZoom, minZoom, maxZoom }) => (
    <div className="absolute bottom-4 left-1/2 -translate-x-1/2 bg-gray-900/70 rounded-full px-4 py-2 flex items-center space-x-3 text-white shadow-lg z-10">
        <button onClick={() => setZoom(Math.max(minZoom, zoom - 0.1))} className="text-lg">-</button>
        <input
            type="range"
            min={minZoom}
            max={maxZoom}
            step="0.01"
            value={zoom}
            onChange={(e) => setZoom(parseFloat(e.target.value))}
            className="w-40"
        />
        <button onClick={() => setZoom(Math.min(maxZoom, zoom + 0.1))} className="text-lg">+</button>
        <span className="w-12 text-center text-sm">{Math.round(zoom * 100)}%</span>
    </div>
);

const ProcessingOverlay: React.FC = () => (
    <div className="absolute inset-0 bg-black/60 flex flex-col items-center justify-center z-20">
        <svg className="animate-spin -ml-1 mr-3 h-10 w-10 text-white" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
            <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
            <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
        </svg>
        <p className="mt-4 text-lg text-white font-semibold">Processing via C++ Backend...</p>
    </div>
);


export const ImageViewer: React.FC<ImageViewerProps> = ({ image, onImageUpload, isProcessing }) => {
    const [isDraggingOver, setIsDraggingOver] = useState(false);
    const [zoom, setZoom] = useState(1);
    const [pan, setPan] = useState({ x: 0, y: 0 });
    const [isPanning, setIsPanning] = useState(false);

    const containerRef = useRef<HTMLDivElement>(null);
    const lastMousePos = useRef({ x: 0, y: 0 });

    const MIN_ZOOM = 0.2;
    const MAX_ZOOM = 5;

    //Reset pan and zoom when a new image is uploaded
    useEffect(() => {
        setPan({x: 0, y: 0});
        setZoom(1);
    }, [image]);

    const handleDragEnter = (e: React.DragEvent<HTMLDivElement>) => { e.preventDefault(); e.stopPropagation(); setIsDraggingOver(true); };
    const handleDragLeave = (e: React.DragEvent<HTMLDivElement>) => { e.preventDefault(); e.stopPropagation(); setIsDraggingOver(false); };
    const handleDragOver = (e: React.DragEvent<HTMLDivElement>) => { e.preventDefault(); e.stopPropagation(); };
    const handleDrop = (e: React.DragEvent<HTMLDivElement>) => {
        e.preventDefault(); e.stopPropagation(); setIsDraggingOver(false);
        if (isProcessing) return;
        const files = e.dataTransfer.files;
        if (files && files.length > 0 && files[0].type.startsWith('image/')) {
            onImageUpload(files[0]);
        } else { alert("Invalid file type. Please upload an image."); }
    };
    
    const handleWheel = (e: React.WheelEvent<HTMLDivElement>) => {
        e.preventDefault(); e.stopPropagation();
        if (isProcessing || !image) return;
        const newZoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, zoom - e.deltaY * 0.001));
        setZoom(newZoom);
    };
    
    const handleMouseDown = (e: React.MouseEvent<HTMLDivElement>) => { if (isProcessing || !image) return; setIsPanning(true); lastMousePos.current = { x: e.clientX, y: e.clientY }; };
    const handleMouseUp = () => { setIsPanning(false); };
    const handleMouseMove = (e: React.MouseEvent<HTMLDivElement>) => {
        if (!isPanning) return;
        const dx = e.clientX - lastMousePos.current.x;
        const dy = e.clientY - lastMousePos.current.y;
        setPan(prevPan => ({ x: prevPan.x + dx, y: prevPan.y + dy }));
        lastMousePos.current = { x: e.clientX, y: e.clientY };
    };

    return (
        <div
            ref={containerRef}
            className="w-full h-full flex items-center justify-center p-8 relative overflow-hidden"
            style={{ cursor: isPanning ? 'grabbing' : (image ? 'grab' : 'default') }}
            onDragEnter={handleDragEnter} onDragLeave={handleDragLeave} onDragOver={handleDragOver} onDrop={handleDrop}
            onWheel={handleWheel} onMouseDown={handleMouseDown} onMouseUp={handleMouseUp} onMouseMove={handleMouseMove} onMouseLeave={handleMouseUp}
        >
            {isProcessing && <ProcessingOverlay />}
            {image ? (
                <>
                    <img
                        src={image}
                        alt="Filtered result"
                        className="max-w-full max-h-full object-contain select-none pointer-events-none"
                        style={{
                            transform: `translate(${pan.x}px, ${pan.y}px) scale(${zoom})`,
                            transition: isPanning ? 'none' : 'transform 0.1s',
                        }}
                        draggable="false"
                    />
                    <ZoomControls zoom={zoom} setZoom={setZoom} minZoom={MIN_ZOOM} maxZoom={MAX_ZOOM} />
                </>
            ) : (
                <div className={`w-full h-full border-4 border-dashed rounded-2xl flex flex-col items-center justify-center transition-colors ${isDraggingOver ? 'border-indigo-500 bg-gray-700/50' : 'border-gray-600'}`}>
                    <svg xmlns="http://www.w3.org/2000/svg" className="h-24 w-24 text-gray-500" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1} d="M4 16l4.586-4.586a2 2 0 012.828 0L16 16m-2-2l1.586-1.586a2 2 0 012.828 0L20 14m-6-6h.01M6 20h12a2 2 0 002-2V6a2 2 0 00-2-2H6a2 2 0 00-2 2v12a2 2 0 002 2z" /></svg>
                    <p className="mt-4 text-xl text-gray-400">Drag & drop an image here</p>
                    <p className="mt-2 text-gray-500">or use the "Open Image..." button</p>
                </div>
            )}
        </div>
    );
};
