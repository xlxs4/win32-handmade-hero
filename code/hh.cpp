#include "hh.h"

internal void
GameOutputSound(GameState* gameState, GameSoundOutputBuffer* buffer, int toneHz)
{
	i16 toneVolume = 3000;
	int wavePeriod = buffer->samplesPerSecond / toneHz;

	i16 *sampleOut = buffer->samples;
	for(int sampleIndex = 0; sampleIndex < buffer->sampleCount; ++sampleIndex)
	{
		f32 sineValue = sinf(gameState->tSine);
		i16 sampleValue = (i16)(sineValue * toneVolume);
		*sampleOut++ = sampleValue;
		*sampleOut++ = sampleValue;
		gameState->tSine += 2.0f * PI32 * 1.0f / (f32)wavePeriod;
		// Wrap the sine wave back to [0, 2pi) so that we don't get
		// increasingly larger floating point errors.
		if(gameState->tSine > 2.0f * PI32)
		{
			gameState->tSine -= 2.0f * PI32;
		}
	}
}

internal void
RenderWeirdGradient(GameOffscreenBuffer* buffer,
                    int xOffset, int yOffset)
{
	u8* row = (u8*)buffer->memory;
	for(int y = 0; y < buffer->height; ++y)
	{
		u32* pixel = (u32*)row;
		for(int x = 0; x < buffer->width; ++x)
		{
			// Due to little endian architecture, if you
			// have RR GG BB XX 32 bits, you'll get
			//      XX BB GG RR
			// Apparently, the people doing Win32 bitmaps wanted the
			// values in the CPU register to be intuitive. They didn't care
			// about the padding bit, they wanted get RR GG BB in the register.
			// So they flipped the order around to BB GG RR XX, to get
			//                                     XX RR GG BB 
			u8 blue = (u8)(x + xOffset);
			u8 green = (u8)(y + yOffset);
			*pixel++ = ((green << 8) | blue);
		}

		row += buffer->pitch;
	}
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	// Assert that the buttons array has a count equal to the struct members,
	// in a poor, hacky way
	Assert((&input->controllers[0].start - &input->controllers[0].buttons[0]) ==
			(ArrayCount(input->controllers[0].buttons) - 1));
	Assert(sizeof(GameState) <= memory->permanentStorageSize);

	GameState* gameState = (GameState*)memory->permanentStorage;
	if(!memory->isInitialized)
	{
		char* filename = __FILE__;

		DebugReadFileResult file = memory->DEBUGPlatformReadEntireFile(filename);
		if(file.contents)
		{
			memory->DEBUGPlatformWriteEntireFile("test.out", file.contentsSize, file.contents);
			memory->DEBUGPlatformFreeFileMemory(file.contents);
		}

		gameState->toneHz = 256;
		gameState->tSine = 0.0f;

		memory->isInitialized = true;
	}

	for(int controllerIndex = 0;
		 controllerIndex < ArrayCount(input->controllers);
		 ++controllerIndex)
	{
		GameControllerInput *controller = GetController(input, controllerIndex);
		if(controller->isAnalog)
		{
			gameState->xOffset += (int)(4.0f*(controller->stickAverageX));
			gameState->toneHz = 256 + (int)(128.0f * (controller->stickAverageY));
		}
		else
		{
			if(controller->moveLeft.endedDown)
			{
				gameState->xOffset -= 1;
			}
			if(controller->moveRight.endedDown)
			{
				gameState->xOffset += 1;
			}
		}

		if(controller->actionDown.endedDown)
		{
			gameState->yOffset += 1;
		}
	}

	RenderWeirdGradient(buffer, gameState->xOffset, gameState->yOffset);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	GameState* gameState = (GameState*)memory->permanentStorage;
	GameOutputSound(gameState, soundBuffer, gameState->toneHz);
}

