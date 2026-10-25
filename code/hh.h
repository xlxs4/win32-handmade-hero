#if !defined(HH_H)

/*
	HANDMADE_INTERNAL:
		0 - Build for public release
		1 - Build for developer only

	HANDMADE_SLOW:
		0 - No slow code allowed!
		1 - Slow code welcome
*/

#if HANDMADE_SLOW
#define Assert(expression) if(!(expression)) { *(int*)0 = 0; }
#else
#define Assert(expression)
#endif

#define Kilobytes(value) ((value)*1024LL)
#define Megabytes(value) (Kilobytes(value)*1024LL)
#define Gigabytes(value) (Megabytes(value)*1024LL)
#define Terabytes(value) (Gigabytes(value)*1024LL)

#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

inline u32
SafeTruncateUInt64(u64 value)
{
	Assert(value <= 0xFFFFFFFF);
	u32 result = (u32)value;
	return result;
}

// Services that the game provides to the platform layer.

#if HANDMADE_INTERNAL
struct DebugReadFileResult
{
	u32   contentsSize;
	void* contents;
};

internal DebugReadFileResult DEBUGPlatformReadEntireFile(char* filename);
internal void                DEBUGPlatformFreeFileMemory(void* bitmapMemory);
internal b32                 DEBUGPlatformWriteEntireFile(char* filename,
		                                                    u32 memorySize, void* memory);
#endif

struct GameOffscreenBuffer
{
	// Pixels are always 32-bits wide, memory order BB GG RR XX.
	void*      memory;
	int        width;
	int        height;
	int        pitch;
};

struct GameSoundOutputBuffer
{
	i16* samples;
	int  samplesPerSecond;
	int  sampleCount;
};

struct GameButtonState
{
	int halfTransitionCount;
	b32 endedDown;
};

struct GameControllerInput
{
	b32 isConnected;
	b32 isAnalog;
	f32 stickAverageX;
	f32 stickAverageY;

	union
	{
		GameButtonState buttons[12];
		struct{
			GameButtonState moveUp;
			GameButtonState moveDown;
			GameButtonState moveLeft;
			GameButtonState moveRight;

			GameButtonState actionUp;
			GameButtonState actionDown;
			GameButtonState actionLeft;
			GameButtonState actionRight;

			GameButtonState leftShoulder;
			GameButtonState rightShoulder;

			GameButtonState back;
			GameButtonState start;
		};
	};
};

struct GameInput
{
	GameControllerInput controllers[5];
};
inline GameControllerInput* GetController(GameInput* input, int unsigned controllerIndex)
{
	Assert(controllerIndex < ArrayCount(input->controllers));
	GameControllerInput* result = &input->controllers[controllerIndex];
	return result;
}

struct GameState
{
	int xOffset;
	int yOffset;
	int toneHz;
};

struct GameMemory
{
	b32   isInitialized;

	u64   permanentStorageSize;
	void* permanentStorage; // REQUIRED to be cleared to zero at startup

	u64   transientStorageSize;
	void* transientStorage; // REQUIRED to be cleared to zero at startup
};

internal void
GameUpdateAndRender(GameMemory* memory,
                    GameInput* input,
                    GameOffscreenBuffer* buffer,
                    GameSoundOutputBuffer* soundBuffer);

#define HH_H
#endif
