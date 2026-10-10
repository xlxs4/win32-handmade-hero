#if !defined(HH_H)

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
}

// Services that the game provides to the platform layer.
internal void
GameUpdateAndRender(GameOffscreenBuffer* buffer, int xOffset, int yOffset,
                    GameSoundOutputBuffer* soundBuffer, int toneHz);

#define HH_H
#endif
