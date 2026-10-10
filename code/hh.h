#if !defined(HH_H)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

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
	b32 isAnalog;

	f32 startX;
	f32 startY;

	f32 minX;
	f32 minY;

	f32 maxX;
	f32 maxY;

	f32 endX;
	f32 endY;

	union
	{
		GameButtonState buttons[6];
		struct{
			GameButtonState up;
			GameButtonState down;
			GameButtonState left;
			GameButtonState right;
			GameButtonState leftShoulder;
			GameButtonState rightShoulder;
		};
	};
};

struct GameInput
{
	GameControllerInput controllers[4];
};

// Services that the game provides to the platform layer.
internal void
GameUpdateAndRender(GameInput* input,
                    GameOffscreenBuffer* buffer,
                    GameSoundOutputBuffer* soundBuffer);

#define HH_H
#endif
