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

#include "hh.h"
#include "hh.cpp"

#include <windows.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_hh.h"

// We use XInput for gamepad support. For XInput to work, we need to link with a
// xinput DLL. That DLL might not be available in a user PC. Since the game can work
// fine without gamepad support (using the keyboard instead), we want to handle
// loading the XInput API code ourselves instead of the linker.

// XInputGetState

// We define a macro to generate the XInput function signature so that if anything
// in the signature changes, we can just change the macro definition instead of
// patching multiple pieces in the code.
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
// For each API function, we generate a stub and a function pointer
// that points to that function, or to our stub.
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
// Keep the code using the original function name, but actually use our own function.
#define XInputGetState XInputGetState_

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void
Win32_LoadXInput()
{
	// https://learn.microsoft.com/en-us/windows/win32/xinput/xinput-versions
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if(!XInputLibrary)
	{
		XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}
	if(!XInputLibrary)
	{
		XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
	}
	if(XInputLibrary)
	{
		// If the XInput library load was successful, make sure our function pointer points
		// to the actual XInput code.
		XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
	}
}

// TODO: Use a more modern sound API instead of DirectSound?
// https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416960(v=vs.85)?redirectedfrom=MSDN

// DirectSoundCreate
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

global_variable b32                   gRunning;
global_variable Win32_OffscreenBuffer gBackbuffer;
global_variable LPDIRECTSOUNDBUFFER   gSecondaryBuffer;
global_variable i64                   gPerfCountFrequency;

internal DebugReadFileResult
DEBUGPlatformReadEntireFile(char* filename)
{
	DebugReadFileResult result = {};

	HANDLE fileHandle = CreateFileA(filename,
			                         GENERIC_READ, FILE_SHARE_READ, 0,
											 OPEN_EXISTING, 0, 0);
	if(fileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER fileSize;
		if(GetFileSizeEx(fileHandle, &fileSize))
		{
			// ReadFile works only with a DWORD for the size, so we
			// make sure that the filesize fits in 4GB (DWORD is 32 bits)
			u32 fileSize32 = SafeTruncateUInt64(fileSize.QuadPart);
			// Typically we wouldn't use VirtualAlloc for such small sizes,
			// but this is test code :)
			result.contents = VirtualAlloc(0, fileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			if(result.contents)
			{
				DWORD bytesRead;
				if(ReadFile(fileHandle, result.contents, fileSize32, &bytesRead, 0) &&
						(fileSize32 == bytesRead))
				{
					result.contentsSize = fileSize32;
				}
				else
				{
					DEBUGPlatformFreeFileMemory(result.contents);
					result.contents = 0;
				}
			}
		}

		CloseHandle(fileHandle);
	}
	return result;
}

internal void
DEBUGPlatformFreeFileMemory(void* memory)
{
	if(memory)
	{
		VirtualFree(memory, 0, MEM_RELEASE);
	}
}

internal b32
DEBUGPlatformWriteEntireFile(char* filename, u32 memorySize, void* memory)
{
	b32 result = false;

	HANDLE fileHandle = CreateFileA(filename,
                                   GENERIC_WRITE, 0, 0,
                                   CREATE_ALWAYS, 0, 0);
	if(fileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD bytesWritten;
		if(WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0))
		{
			result = (bytesWritten == memorySize);
		}
		
		CloseHandle(fileHandle);
	}
	return result;
}

internal void
Win32_InitDSound(HWND window, i32 samplesPerSecond, i32 bufferSize)
{
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	if(DSoundLibrary)
	{
		direct_sound_create* DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
		LPDIRECTSOUND directSound;
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &directSound, 0)))
		{
			WAVEFORMATEX waveFormat = {};
			waveFormat.wFormatTag = WAVE_FORMAT_PCM;
			waveFormat.nChannels = 2;
			waveFormat.nSamplesPerSec = samplesPerSecond;
			waveFormat.wBitsPerSample = 16;
			waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
			waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

			if(SUCCEEDED(directSound->SetCooperativeLevel(window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC bufferDescription = {};
				bufferDescription.dwSize = sizeof(bufferDescription);
				bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
				LPDIRECTSOUNDBUFFER primaryBuffer;
				if(SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &primaryBuffer, 0)))
				{
					if(SUCCEEDED(primaryBuffer->SetFormat(&waveFormat)))
					{
						OutputDebugStringA("pri suc");
					}
				}
			}

			DSBUFFERDESC bufferDescription = {};
			bufferDescription.dwSize = sizeof(bufferDescription);
			bufferDescription.dwBufferBytes = bufferSize;
			bufferDescription.lpwfxFormat = &waveFormat;
			if(SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &gSecondaryBuffer, 0)))
			{
				OutputDebugStringA("sec suc");
			}
		}
	}
}

internal Win32_WindowDimension
Win32_GetWindowDimension(HWND window)
{
	Win32_WindowDimension result;

	RECT clientRect;
	GetClientRect(window, &clientRect);
	result.width = clientRect.right - clientRect.left;
	result.height = clientRect.bottom - clientRect.top;

	return result;
}

internal void
Win32_ResizeDIBSection(Win32_OffscreenBuffer* buffer, int width, int height)
{
	if(buffer->memory)
	{
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	}

	buffer->width = width;
	buffer->height = height;

	int bytesPerPixel = 4;
	buffer->bytesPerPixel = bytesPerPixel;

	buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
	buffer->info.bmiHeader.biWidth = buffer->width;
	// For uncompressed RGB bitmaps, if biHeight is positive, bitmap is a bottom-up DIB
	// with its origin at the lower-left corner. If it's negative, it's a top-down DIB
	// with its origin at the upper-left corner. We do top-down DIB here. Refer to the
	// BITMAPINFOHEADER structure in the Win32 API docs.
	buffer->info.bmiHeader.biHeight = -buffer->height;
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = 32;
	buffer->info.bmiHeader.biCompression = BI_RGB;

	int bitmapMemorySize = (buffer->width*buffer->height)*bytesPerPixel;
	buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	buffer->pitch = width*bytesPerPixel;
}

internal void
Win32_DisplayBufferInWindow(Win32_OffscreenBuffer* buffer,
	                         HDC deviceContext,
	                         int windowWidth, int windowHeight)
{
	StretchDIBits(
		deviceContext,
		0, 0, windowWidth, windowHeight,
		0, 0, buffer->width, buffer->height,
		buffer->memory,
		&buffer->info,
		DIB_RGB_COLORS, SRCCOPY
	);
}

LRESULT CALLBACK
Win32_MainWindowCallback(HWND   window,
                         UINT   message,
                         WPARAM wParam,
                         LPARAM lParam)
{
	LRESULT result = 0;

	switch(message)
	{
		case WM_DESTROY:
		{
			gRunning = false;
		} break;

		case WM_CLOSE:
		{
			gRunning = false;
		} break;
	
		case WM_ACTIVATEAPP:
		{
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT paint;
			HDC deviceContext = BeginPaint(window, &paint);

			Win32_WindowDimension dimension = Win32_GetWindowDimension(window);
			Win32_DisplayBufferInWindow(
				&gBackbuffer,
				deviceContext,
				dimension.width, dimension.height
			);
			EndPaint(window, &paint);
		} break;

		// TODO: Disable the cursor. For later.
		// case WM_SETCURSOR:
		// {
		// 	SetCursor(0);
		// } break;

		default:
		{
			result = DefWindowProcA(window, message, wParam, lParam);
		} break;
	}

	return result;
}

internal void
Win32_ClearBuffer(Win32_SoundOutput* soundOutput)
{
	VOID* region1;
	DWORD region1Size;
	VOID* region2;
	DWORD region2Size;
	if(SUCCEEDED(gSecondaryBuffer->Lock(
		0,
		soundOutput->secondaryBufferSize,
		&region1, &region1Size,
		&region2, &region2Size,
		0
	)))
	{
		u8* destSample = (u8*)region1;
		for(DWORD byteIndex = 0; byteIndex < region1Size; ++byteIndex)
		{
			*destSample++ = 0;
		}

		destSample = (u8*)region2;
		for(DWORD byteIndex = 0; byteIndex < region2Size; ++byteIndex)
		{
			*destSample++ = 0;
		}

		gSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
	}
}

internal void
Win32_FillSoundBuffer(Win32_SoundOutput* soundOutput, DWORD byteToLock, DWORD bytesToWrite,
	                   GameSoundOutputBuffer* sourceBuffer)
{
	VOID* region1;
	DWORD region1Size;
	VOID* region2;
	DWORD region2Size;
	if(SUCCEEDED(gSecondaryBuffer->Lock(
		byteToLock,
		bytesToWrite,
		&region1, &region1Size,
		&region2, &region2Size,
		0
	)))
	{
		DWORD region1SampleCount = region1Size/soundOutput->bytesPerSample;
		i16* destSample = (i16*)region1;
		i16* sourceSample = sourceBuffer->samples;
		for(DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex)
		{
			*destSample++ = *sourceSample++;
			*destSample++ = *sourceSample++;
			++soundOutput->runningSampleIndex;
		}

		DWORD region2SampleCount = region2Size/soundOutput->bytesPerSample;
		destSample = (i16*)region2;
		for(DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex)
		{
			*destSample++ = *sourceSample++;
			*destSample++ = *sourceSample++;
			++soundOutput->runningSampleIndex;
		}

		gSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
	}
}

internal void
Win32_ProcessKeyboardMessage(GameButtonState* newState, b32 isDown)
{
	Assert(newState->endedDown != isDown);
	newState->endedDown = isDown;
	++newState->halfTransitionCount;
}

internal void
Win32_ProcessDigitalXInputButton(DWORD XButtonInputState,
                                 DWORD buttonBit,
                                 GameButtonState* oldState,
                                 GameButtonState* newState)
{
	newState->endedDown = ((XButtonInputState & buttonBit) == buttonBit);
	newState->halfTransitionCount = 
		(oldState->endedDown != newState->endedDown) ? 1 : 0;
}

internal f32
Win32_ProcessXInputStickValue(SHORT value, SHORT deadzoneThreshold)
{
	f32 result = 0;
	if(value < -deadzoneThreshold)
	{
		result = (f32)((value + deadzoneThreshold) / (32768.0f - deadzoneThreshold));
	}
	else if(value > deadzoneThreshold)
	{
		result = (f32)((value - deadzoneThreshold) / (32767.0f - deadzoneThreshold));
	}
	return result;
}

internal void
Win32_ProcessPendingMessages(GameControllerInput* keyboardController)
{
	MSG message;
	while(PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		switch(message.message)
		{
			case WM_QUIT:
			{
				gRunning = false;
			} break;
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				u32 virtualKeyCode = (u32)message.wParam;
				b32 wasDown = ((message.lParam & (1 << 30)) != 0);
				b32 isDown  = ((message.lParam & (1 << 31)) == 0);
				if(wasDown != isDown)
				{
					if(virtualKeyCode == 'W')
					{
						Win32_ProcessKeyboardMessage(&keyboardController->moveUp, isDown);
					}
					else if(virtualKeyCode == 'A')
					{
						Win32_ProcessKeyboardMessage(&keyboardController->moveLeft, isDown);
					}
					else if(virtualKeyCode == 'S')
					{	
						Win32_ProcessKeyboardMessage(&keyboardController->moveDown, isDown);
					}
					else if(virtualKeyCode == 'D')
					{	
						Win32_ProcessKeyboardMessage(&keyboardController->moveRight, isDown);
					}
					else if(virtualKeyCode == 'Q')
					{	
						Win32_ProcessKeyboardMessage(&keyboardController->leftShoulder, isDown);
					}
					else if(virtualKeyCode == 'E')
					{
						Win32_ProcessKeyboardMessage(&keyboardController->rightShoulder, isDown);
					}
					else if(virtualKeyCode == VK_UP)
					{
						Win32_ProcessKeyboardMessage(&keyboardController->actionUp, isDown);
					}
					else if(virtualKeyCode == VK_LEFT)
					{	
						Win32_ProcessKeyboardMessage(&keyboardController->actionLeft, isDown);
					}
					else if(virtualKeyCode == VK_DOWN)
					{	
						Win32_ProcessKeyboardMessage(&keyboardController->actionDown, isDown);
					}
					else if(virtualKeyCode == VK_RIGHT)
					{
						Win32_ProcessKeyboardMessage(&keyboardController->actionRight, isDown);
					}
					else if(virtualKeyCode == VK_ESCAPE)
					{
						Win32_ProcessKeyboardMessage(&keyboardController->start, isDown);
					}
					else if(virtualKeyCode == VK_SPACE)
					{
						Win32_ProcessKeyboardMessage(&keyboardController->back, isDown);
					}
				}

				b32 altKeyWasDown = message.lParam & (1 << 29);
				if((virtualKeyCode == VK_F4) && altKeyWasDown)
				{
					gRunning = false;
				}
			} break;

			default:
			{
				TranslateMessage(&message);
				DispatchMessageA(&message);
			} break;
		}
	}
}

inline LARGE_INTEGER
Win32_GetWallClock(void)
{
	LARGE_INTEGER result;
	QueryPerformanceCounter(&result);
	return result;
}

inline f32
Win32_GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
	f32 result = ((f32)(end.QuadPart - start.QuadPart) /
	                             (f32)gPerfCountFrequency);
	return result;
}

internal void
Win32_DebugDrawVertical(Win32_OffscreenBuffer* backbuffer,
		                  int x, int top, int bottom,
								u32 color)
{
	u8* pixel = ((u8*)backbuffer->memory +
			       x*backbuffer->bytesPerPixel +
					 top*backbuffer->pitch);
	for(int y = top; y < bottom; ++y)
	{
		*(u32*)pixel = color;
		pixel += backbuffer->pitch;
	}
}

inline void
Win32_DrawSoundBufferMarker(Win32_OffscreenBuffer* backbuffer,
									 Win32_SoundOutput* soundOutput,
									 f32 c,
									 int padX, int top, int bottom,
									 DWORD value, u32 color)
{
	Assert(value < soundOutput->secondaryBufferSize);
	f32 xF32 = (c * (f32)value);
	int x = padX + (int)xF32;
	Win32_DebugDrawVertical(backbuffer, x, top, bottom, color);
}

internal void
Win32_DebugSyncDisplay(Win32_OffscreenBuffer* backbuffer,
		                 int markerCount, Win32_DebugTimeMarker* markers,
							  Win32_SoundOutput* soundOutput,
							  f32 targetSecondsPerFrame)
{
	int padX = 16;
	int padY = 16;
	int top = padY;
	int bottom = backbuffer->height - padY;

	f32 c = (f32)(backbuffer->width - 2*padX) / (f32)soundOutput->secondaryBufferSize;
	for (int markerIndex = 0;
		  markerIndex < markerCount;
		  ++markerIndex)
	{
		Win32_DebugTimeMarker* thisMarker = &markers[markerIndex];
		Win32_DrawSoundBufferMarker(backbuffer, soundOutput,
											 c, padX, top, bottom,
											 thisMarker->playCursor, 0xFFFFFFFF);
		Win32_DrawSoundBufferMarker(backbuffer, soundOutput,
											 c, padX, top, bottom,
											 thisMarker->writeCursor, 0xFFFF0000);
	}
}

int CALLBACK
WinMain(HINSTANCE instance,
        HINSTANCE prevInstance,
        LPSTR     commandLine,
        int       showCode)
{
	LARGE_INTEGER perfCountFrequencyResult;
	QueryPerformanceFrequency(&perfCountFrequencyResult);
	gPerfCountFrequency = perfCountFrequencyResult.QuadPart;

	UINT desiredSchedulerMS = 1;
	b32 sleepIsGranular = (timeBeginPeriod(desiredSchedulerMS) == TIMERR_NOERROR);

	Win32_LoadXInput();

	WNDCLASSA windowClass = {};

	Win32_ResizeDIBSection(&gBackbuffer, 1280, 720);

	// Casey added CS_OWNDC. We don't, because that's not good practice.
	// Refer to https://devblogs.microsoft.com/oldnewthing/20060601-06/?p=31003
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = Win32_MainWindowCallback;
	windowClass.hInstance = instance;
	windowClass.lpszClassName = "HandmadeHerowindowClass";

#define framesOfAudioLatency 3
#define monitorRefreshHz 60
#define gameUpdateHz (monitorRefreshHz / 2)
	f32 targetSecondsPerFrame = 1.0f / (f32)gameUpdateHz;

	if(RegisterClassA(&windowClass))
	{
		HWND window = CreateWindowExA(
			0,
			windowClass.lpszClassName,
			"Handmade Hero",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT, CW_USEDEFAULT,
			CW_USEDEFAULT, CW_USEDEFAULT,
			0,
			0,
			windowClass.hInstance,
			0
		);
		if(window)
		{
			Win32_SoundOutput soundOutput = {};
			soundOutput.samplesPerSecond = 48000;
			soundOutput.bytesPerSample = sizeof(i16) * 2;
			soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
			soundOutput.latencySampleCount = framesOfAudioLatency *
				(soundOutput.samplesPerSecond / gameUpdateHz);

			Win32_InitDSound(window, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize);
			Win32_ClearBuffer(&soundOutput);
			gSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			gRunning = true;

			// An extra copy of the buffer that will get filled by the game, and
			// then be passed to the DirectSound ringbuffer. Worth it to not
			// pass the weird ringbuffer complexity to the game code.
			i16* samples = (i16*)VirtualAlloc(0, soundOutput.secondaryBufferSize,
				                         MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
			// We use a specific base address for our memory allocation internally
			// to get some juicy determinism for debugging pointers and whatnot.
			LPVOID baseAddress = (LPVOID)Terabytes(2);
#else
			LPVOID baseAddress = 0;
#endif

			GameMemory gameMemory = {};
			gameMemory.permanentStorageSize = Megabytes(64);
			gameMemory.transientStorageSize = Gigabytes(4);

			u64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
			gameMemory.permanentStorage = VirtualAlloc(baseAddress, totalSize,
		                                             MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			gameMemory.transientStorage = ((u8*)gameMemory.permanentStorage +
				gameMemory.permanentStorageSize);

			if(samples && gameMemory.permanentStorage && gameMemory.transientStorage)
			{
				GameInput input[2] = {};
				GameInput* newInput = &input[0];
				GameInput* oldInput = &input[1];

				LARGE_INTEGER lastCounter = Win32_GetWallClock();

				int debugTimeMarkerIndex = 0;
				Win32_DebugTimeMarker debugTimeMarkers[gameUpdateHz / 2] = {};

				DWORD lastPlayCursor = 0;
				b32 soundIsValid = false;

				u64 lastCycleCount = __rdtsc();
				while(gRunning)
				{
					GameControllerInput *oldKeyboardController = GetController(oldInput, 0);
					GameControllerInput *newKeyboardController = GetController(newInput, 0);
					*newKeyboardController = {};
					newKeyboardController->isConnected = true;
					for (int buttonIndex = 0;
					     buttonIndex < ArrayCount(newKeyboardController->buttons);
						  ++buttonIndex)
					{
						newKeyboardController->buttons[buttonIndex].endedDown =
							oldKeyboardController->buttons[buttonIndex].endedDown;
					}

					Win32_ProcessPendingMessages(newKeyboardController);

					DWORD maxControllerCount = XUSER_MAX_COUNT;
					if(maxControllerCount > (ArrayCount(newInput->controllers) - 1))
					{
						maxControllerCount = (ArrayCount(newInput->controllers) - 1);
					}

					for (DWORD controllerIndex = 0;
						 controllerIndex < maxControllerCount;
						 ++controllerIndex)
					{
						DWORD ourControllerIndex = controllerIndex + 1;
						GameControllerInput *oldController = GetController(oldInput, ourControllerIndex);
						GameControllerInput *newController = GetController(newInput, ourControllerIndex);

						XINPUT_STATE controllerState;
						if(XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
						{
							newController->isConnected = true;

							XINPUT_GAMEPAD* pad = &controllerState.Gamepad;

							newController->isAnalog = true;
							newController->stickAverageX = Win32_ProcessXInputStickValue(
									pad->sThumbLX,
							      XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
							newController->stickAverageY = Win32_ProcessXInputStickValue(
									pad->sThumbLY,
							      XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

							if((newController->stickAverageX != 0.0f) ||
								(newController->stickAverageY != 0.0f))
							{
								newController->isAnalog = true;
							}

							if(pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
							{
								newController->stickAverageY = 1.0f;
								newController->isAnalog = false;
							}
							if(pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
							{
								newController->stickAverageY = -1.0f;
								newController->isAnalog = false;
							}
							if(pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
							{
								newController->stickAverageX = -1.0f;
								newController->isAnalog = false;
							}
							if(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
							{
								newController->stickAverageX = 1.0f;
								newController->isAnalog = false;
							}
							
							f32 threshold = 0.5f;
							Win32_ProcessDigitalXInputButton(
								(newController->stickAverageX < -threshold) ? 1 : 0,
								1,
								&oldController->moveLeft,
								&newController->moveLeft
							);
							Win32_ProcessDigitalXInputButton(
								(newController->stickAverageX > threshold) ? 1 : 0,
								1,
								&oldController->moveRight,
								&newController->moveRight
							);
							Win32_ProcessDigitalXInputButton(
								(newController->stickAverageY < -threshold) ? 1 : 0,
								1,
								&oldController->moveDown,
								&newController->moveDown
							);
							Win32_ProcessDigitalXInputButton(
								(newController->stickAverageY > threshold) ? 1 : 0,
								1,
								&oldController->moveUp,
								&newController->moveUp
							);

							Win32_ProcessDigitalXInputButton(
								pad->wButtons,
								XINPUT_GAMEPAD_A,
								&oldController->actionDown,
								&newController->actionDown
							);
							Win32_ProcessDigitalXInputButton(
								pad->wButtons,
								XINPUT_GAMEPAD_B,
								&oldController->actionRight,
								&newController->actionRight
							);
							Win32_ProcessDigitalXInputButton(
								pad->wButtons,
								XINPUT_GAMEPAD_X,
								&oldController->actionLeft,
								&newController->actionLeft
							);
							Win32_ProcessDigitalXInputButton(
								pad->wButtons,
								XINPUT_GAMEPAD_Y,
								&oldController->actionUp,
								&newController->actionUp
							);

							Win32_ProcessDigitalXInputButton(
								pad->wButtons,
								XINPUT_GAMEPAD_LEFT_SHOULDER,
								&oldController->leftShoulder,
								&newController->leftShoulder
							);
							Win32_ProcessDigitalXInputButton(
								pad->wButtons,
								XINPUT_GAMEPAD_RIGHT_SHOULDER,
								&oldController->rightShoulder,
								&newController->rightShoulder
							);

							Win32_ProcessDigitalXInputButton(
								pad->wButtons,
								XINPUT_GAMEPAD_BACK,
								&oldController->back,
								&newController->back
							);

							Win32_ProcessDigitalXInputButton(
								pad->wButtons,
								XINPUT_GAMEPAD_START,
								&oldController->start,
								&newController->start
							);
						}
						else
						{
							newController->isConnected = false;
						}
					}

					DWORD byteToLock = 0;
					DWORD targetCursor = 0;
					DWORD bytesToWrite = 0;
					if(soundIsValid)
					{
						byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) %
						                    soundOutput.secondaryBufferSize;
						targetCursor = (lastPlayCursor +
							                  (soundOutput.latencySampleCount * soundOutput.bytesPerSample)) %
						                      soundOutput.secondaryBufferSize;
						if(byteToLock > targetCursor)
						{
							bytesToWrite = soundOutput.secondaryBufferSize - byteToLock;
							bytesToWrite += targetCursor;
						}
						else
						{
							bytesToWrite = targetCursor - byteToLock;
						}
					}

					GameSoundOutputBuffer soundBuffer = {};
					soundBuffer.samples = samples;
					soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
					soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;

					GameOffscreenBuffer gameBuffer = {};
					gameBuffer.memory = gBackbuffer.memory;
					gameBuffer.width = gBackbuffer.width;
					gameBuffer.height = gBackbuffer.height;
					gameBuffer.pitch = gBackbuffer.pitch;

					GameUpdateAndRender(&gameMemory, newInput, &gameBuffer, &soundBuffer);

					if(soundIsValid)
					{
#if HANDMADE_INTERNAL
						DWORD playCursor;
						DWORD writeCursor;
						gSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor);

						char textBuffer[256];
						_snprintf_s(textBuffer, sizeof(textBuffer),
										"LPC:%u, BTL:%u, TC:%u, BTW:%u - PC:%u, WC:%u\n",
										lastPlayCursor, byteToLock, targetCursor, bytesToWrite,
										playCursor, writeCursor);
						OutputDebugStringA(textBuffer);
#endif
						Win32_FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);
					}

					LARGE_INTEGER workCounter = Win32_GetWallClock();
					f32 workSecondsElapsed = Win32_GetSecondsElapsed(lastCounter, workCounter);

					f32 secondsElapsedForFrame = workSecondsElapsed;
					if(secondsElapsedForFrame < targetSecondsPerFrame)
					{
						if(sleepIsGranular)
						{
							DWORD sleepMS = (DWORD)(1000.0f * (targetSecondsPerFrame -
										                          secondsElapsedForFrame));
							if(sleepMS > 0)
							{
								Sleep(sleepMS);
							}
						}

						while(secondsElapsedForFrame < targetSecondsPerFrame)
						{
							secondsElapsedForFrame = Win32_GetSecondsElapsed(lastCounter,
																							 Win32_GetWallClock());
						}
					}
					else
					{
					}

					LARGE_INTEGER endCounter = Win32_GetWallClock();
					f32 msPerFrame = 1000.0f * Win32_GetSecondsElapsed(lastCounter, endCounter);
					lastCounter = endCounter;

					HDC deviceContext = GetDC(window);
					Win32_WindowDimension dimension = Win32_GetWindowDimension(window);

#if HANDMADE_INTERNAL
					Win32_DebugSyncDisplay(&gBackbuffer,
							                 ArrayCount(debugTimeMarkers), debugTimeMarkers,
												  &soundOutput,
												  targetSecondsPerFrame);
#endif

					Win32_DisplayBufferInWindow(
						&gBackbuffer,
						deviceContext,
						dimension.width, dimension.height
					);
					ReleaseDC(window, deviceContext);

					DWORD playCursor;
					DWORD writeCursor;
					if(SUCCEEDED(gSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor)))
					{
						lastPlayCursor = playCursor;
						if(!soundIsValid)
						{
							soundOutput.runningSampleIndex = writeCursor / soundOutput.bytesPerSample;
							soundIsValid = true;
						}
					}
					else
					{
						soundIsValid = false;
					}

#if HANDMADE_INTERNAL
					{
						Win32_DebugTimeMarker* marker = &debugTimeMarkers[debugTimeMarkerIndex++];
						if(debugTimeMarkerIndex > ArrayCount(debugTimeMarkers))
						{
							debugTimeMarkerIndex = 0;
						}
						marker->playCursor = playCursor;
						marker->writeCursor = writeCursor;
					}
#endif

					GameInput* temp = newInput;
					newInput = oldInput;
					oldInput = temp;

					u64 endCycleCount = __rdtsc();
					u64 cyclesElapsed = endCycleCount - lastCycleCount;
					lastCycleCount = endCycleCount;

#if HANDMADE_INTERNAL
					// f64 fps = (f64)gPerfCountFrequency / (f64)counterElapsed;
					f64 fps = 0.0f;
					f64 mcpf = ((f64)cyclesElapsed / (1000.0f * 1000.0f));

					char FPSBuffer[256];
					_snprintf_s(FPSBuffer, sizeof(FPSBuffer),
					            "%.02fms/f, %.02ff/s, %.02fmc/f\n", msPerFrame, fps, mcpf);
					OutputDebugStringA(FPSBuffer);
#endif
				}
			}
		}
	}

	return 0;
}
