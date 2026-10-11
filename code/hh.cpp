internal void
GameOutputSound(GameSoundOutputBuffer* buffer, int toneHz)
{
	local_persist f32 tSine;
	i16 toneVolume = 3000;
	int wavePeriod = buffer->samplesPerSecond / toneHz;

	i16 *sampleOut = buffer->samples;
	for(int sampleIndex = 0; sampleIndex < buffer->sampleCount; ++sampleIndex)
	{
		f32 sineValue = sinf(tSine);
		i16 sampleValue = (i16)(sineValue * toneVolume);
		*sampleOut++ = sampleValue;
		*sampleOut++ = sampleValue;
		tSine += 2.0f * PI32 * 1.0f / (f32)wavePeriod;
		// Wrap the sine wave back to [0, 2pi) so that we don't get
		// increasingly larger floating point errors.
		if(tSine > 2.0f * PI32)
		{
			tSine -= 2.0f * PI32;
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
			u8 blue = (x + xOffset);
			u8 green = (y + yOffset);
			*pixel++ = ((green << 8) | blue);
		}

		row += buffer->pitch;
	}
}

internal void
GameUpdateAndRender(GameMemory* memory,
                    GameInput* input,
                    GameOffscreenBuffer* buffer,
                    GameSoundOutputBuffer* soundBuffer)
{
	Assert(sizeof(GameState) <= memory->permanentStorageSize);

	GameState* gameState = (GameState*)memory->permanentStorage;
	if(!memory->isInitialized)
	{
		gameState->toneHz = 440;

		memory->isInitialized = true;
	}

	GameControllerInput *input0 = &input->controllers[0];
	if(input0->isAnalog)
	{
		gameState->xOffset += (int)4.0f*(input0->endX);
		gameState->toneHz = 256 + (int)(128.0f * (input0->endY));
	}
	else
	{

	}

	if(input0->down.endedDown)
	{
		gameState->yOffset += 1;
	}

	
	GameOutputSound(soundBuffer, gameState->toneHz);
	RenderWeirdGradient(buffer, gameState->xOffset, gameState->yOffset);
}
