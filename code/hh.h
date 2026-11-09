/*
	HANDMADE_INTERNAL:
		0 - Build for public release
		1 - Build for developer only

	HANDMADE_SLOW:
		0 - No slow code allowed!
		1 - Slow code welcome
*/

#if !defined(HH_H)

#include <cstdint>
#include <cmath>
#include <stdio.h>

#define internal        static
#define local_persist   static
#define global_variable static

#define PI32 3.14159265359f

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float  f32;
typedef double f64;

typedef int32_t b32;

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

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void* memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DebugReadFileResult name(char* filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) b32 name(char* filename, u32 memorySize, void* memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif

struct GameOffscreenBuffer
{
	// Pixels are always 32-bits wide, memory order BB GG RR XX.
	void* memory;
	int   width;
	int   height;
	int   pitch;
	int   bytesPerPixel;
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

	f32 tSine;

	int playerX;
	int playerY;
	f32 tJump;
};

struct GameMemory
{
	b32   isInitialized;

	u64   permanentStorageSize;
	void* permanentStorage; // REQUIRED to be cleared to zero at startup

	u64   transientStorageSize;
	void* transientStorage; // REQUIRED to be cleared to zero at startup

	debug_platform_free_file_memory* DEBUGPlatformFreeFileMemory;
	debug_platform_read_entire_file* DEBUGPlatformReadEntireFile;
	debug_platform_write_entire_file* DEBUGPlatformWriteEntireFile;
};

#define GAME_UPDATE_AND_RENDER(name) void name(GameMemory* memory, GameInput* input, GameOffscreenBuffer* buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub) {}

#define GAME_GET_SOUND_SAMPLES(name) void name(GameMemory* memory, GameSoundOutputBuffer* soundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub) {}

#define HH_H
#endif
