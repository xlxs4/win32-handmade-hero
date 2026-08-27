#if !defined(HH_H)

struct GameOffscreenBuffer
{
	// Pixels are always 32-bits wide, memory order BB GG RR XX.
	void*      memory;
	int        width;
	int        height;
	int        pitch;
};

// Services that the game provides to the platform layer.
internal void GameUpdateAndRender(GameOffscreenBuffer* buffer);

#define HH_H
#endif
