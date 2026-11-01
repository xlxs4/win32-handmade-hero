#if !defined(WIN32_HH_H)

// TODO: Use a more modern API instead of DirectShow?
struct Win32_OffscreenBuffer
{
	// Pixels are always 32-bits wide, memory order BB GG RR XX.
	BITMAPINFO info;
	void*      memory;
	int        width;
	int        height;
	int        pitch;
	int        bytesPerPixel;
};

struct Win32_WindowDimension
{
	int width;
	int height;
};

struct Win32_SoundOutput
{
	int   samplesPerSecond;
	u32   runningSampleIndex;
	int   bytesPerSample;
	DWORD secondaryBufferSize;
	DWORD safetyBytes;
	f32   tSine;
	int   latencySampleCount;
};

struct Win32_DebugTimeMarker
{
	DWORD outputPlayCursor;
	DWORD outputWriteCursor;
	DWORD outputLocation;
	DWORD outputByteCount;
	DWORD expectedFlipPlayCursor;
	DWORD flipPlayCursor;
	DWORD flipWriteCursor;
};

#define WIN32_HH_H
#endif
