#include <cstdint>
#include <cmath>

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

global_variable LPDIRECTSOUNDBUFFER gSecondaryBuffer;

// DirectSoundCreate
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

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

global_variable b32 gRunning;

global_variable Win32_OffscreenBuffer gBackbuffer;

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

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			u32 virtualKeyCode = wParam;
			b32 wasDown = ((lParam & (1 << 30)) != 0);
			b32 isDown  = ((lParam & (1 << 31)) == 0);
			if(wasDown != isDown)
			{
				if(virtualKeyCode == 'W')
				{
				}
				else if(virtualKeyCode == 'A')
				{
				}
				else if(virtualKeyCode == 'S')
				{	
				}
				else if(virtualKeyCode == 'D')
				{	
				}
				else if(virtualKeyCode == 'Q')
				{	
				}
				else if(virtualKeyCode == 'E')
				{
				}
				else if(virtualKeyCode == VK_UP)
				{	
				}
				else if(virtualKeyCode == VK_LEFT)
				{	
				}
				else if(virtualKeyCode == VK_DOWN)
				{	
				}
				else if(virtualKeyCode == VK_RIGHT)
				{
				}
				else if(virtualKeyCode == VK_ESCAPE)
				{	
				}
				else if(virtualKeyCode == VK_SPACE)
				{
				}
			}

			b32 altKeyWasDown = lParam & (1 << 29);
			if((virtualKeyCode == VK_F4) && altKeyWasDown)
			{
				gRunning = false;
			}
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
		soundOutput->gSecondaryBufferSize,
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
Win32_ProcessDigitalXInputButton(DWORD XButtonInputState,
                                 DWORD buttonBit,
                                 GameButtonState* oldState,
                                 GameButtonState* newState)
{
	newState->endedDown = ((XButtonInputState & buttonBit) == buttonBit);
	newState->halfTransitionCount = 
		(oldState->endedDown != newState->endedDown) ? 1 : 0;
}

int
WinMain(HINSTANCE instance,
        HINSTANCE prevInstance,
        LPSTR     commandLine,
        int       showCode)
{
	LARGE_INTEGER perfCountFrequencyResult;
	QueryPerformanceFrequency(&perfCountFrequencyResult);
	i64 perfCountFrequency = perfCountFrequencyResult.QuadPart;

	Win32_LoadXInput();

	WNDCLASSA windowClass = {};

	Win32_ResizeDIBSection(&gBackbuffer, 1280, 720);

	// Casey added CS_OWNDC. We don't, because that's not good practice.
	// Refer to https://devblogs.microsoft.com/oldnewthing/20060601-06/?p=31003
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = Win32_MainWindowCallback;
	windowClass.hInstance = instance;
	windowClass.lpszClassName = "HandmadeHerowindowClass";

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
			MSG message;

			Win32_SoundOutput soundOutput = {};
			soundOutput.samplesPerSecond = 48000;
			soundOutput.bytesPerSample = sizeof(i16) * 2;
			soundOutput.gSecondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
			soundOutput.latencySampleCount = soundOutput.samplesPerSecond / 15;

			Win32_InitDSound(window, soundOutput.samplesPerSecond, soundOutput.gSecondaryBufferSize);
			Win32_ClearBuffer(&soundOutput);
			gSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			gRunning = true;

			// An extra copy of the buffer that will get filled by the game, and
			// then be passed to the DirectSound ringbuffer. Worth it to not
			// pass the weird ringbuffer complexity to the game code.
			i16* samples = (i16*)VirtualAlloc(0, soundOutput.gSecondaryBufferSize,
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

				LARGE_INTEGER lastCounter;
				QueryPerformanceCounter(&lastCounter);

				u64 lastCycleCount = __rdtsc();
				while(gRunning)
				{
					while(PeekMessage(&message, 0, 0, 0, PM_REMOVE))
					{
						if(message.message == WM_QUIT)
						{
							gRunning = false;
						}
						TranslateMessage(&message);
						DispatchMessageA(&message);
					}

					DWORD dwResult;   
					int maxControllerCount = XUSER_MAX_COUNT;
					if(maxControllerCount > ArrayCount(newInput->controllers))
					{
						maxControllerCount = ArrayCount(newInput->controllers);
					}

					for (DWORD controllerIndex = 0;
						 controllerIndex < maxControllerCount;
						 ++controllerIndex)
					{
						GameControllerInput *oldController = &oldInput->controllers[controllerIndex];
						GameControllerInput *newController = &newInput->controllers[controllerIndex];

						XINPUT_STATE controllerState;
						if(XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
						{
							XINPUT_GAMEPAD* pad = &controllerState.Gamepad;

							b32 dPadUp    = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
							b32 dPadDown  = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
							b32 dPadLeft  = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
							b32 dPadRight = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

							newController->isAnalog = true;
							newController->startX = oldController->endX;
							newController->startY = oldController->endY;

							f32 X;
							if(pad->sThumbLX < 0)
							{
								X = (f32)pad->sThumbLX / 32768.0f;
							}
							else
							{
								X = (f32)pad->sThumbLX / 32767.0f;
							}
							newController->minX = newController->maxX = newController->endX = X;

							f32 Y;
							if(pad->sThumbLY < 0)
							{
								Y = (f32)pad->sThumbLY / 32768.0f;
							}
							else
							{
								Y = (f32)pad->sThumbLY / 32767.0f;
							}
							newController->minY = newController->maxY = newController->endY = Y;
							
							Win32_ProcessDigitalXInputButton(
								pad->wButtons,
								XINPUT_GAMEPAD_A,
								&oldController->down,
								&newController->down
							);
							Win32_ProcessDigitalXInputButton(
								pad->wButtons,
								XINPUT_GAMEPAD_B,
								&oldController->right,
								&newController->right
							);
							Win32_ProcessDigitalXInputButton(
								pad->wButtons,
								XINPUT_GAMEPAD_X,
								&oldController->left,
								&newController->left
							);
							Win32_ProcessDigitalXInputButton(
								pad->wButtons,
								XINPUT_GAMEPAD_Y,
								&oldController->up,
								&newController->up
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

							// b32 btnStart = (pad->wButtons & XINPUT_GAMEPAD_START);
							// b32 btnBack  = (pad->wButtons & XINPUT_GAMEPAD_BACK);
						}
					}

					DWORD byteToLock = 0;
					DWORD targetCursor = 0;
					DWORD bytesToWrite = 0;
					DWORD playCursor = 0;
					DWORD writeCursor = 0;
					b32 soundIsValid = false;
					if(SUCCEEDED(gSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor)))
					{
						byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) %
						                    soundOutput.gSecondaryBufferSize;
						targetCursor = (playCursor +
							                  (soundOutput.latencySampleCount * soundOutput.bytesPerSample)) %
						                      soundOutput.gSecondaryBufferSize;
						bytesToWrite;
						if(byteToLock > targetCursor)
						{
							bytesToWrite = soundOutput.gSecondaryBufferSize - byteToLock;
							bytesToWrite += targetCursor;
						}
						else
						{
							bytesToWrite = targetCursor - byteToLock;
						}

						soundIsValid = true;
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
						Win32_FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);
					}

					HDC deviceContext = GetDC(window);
					Win32_WindowDimension dimension = Win32_GetWindowDimension(window);
					Win32_DisplayBufferInWindow(
						&gBackbuffer,
						deviceContext,
						dimension.width, dimension.height
					);
					ReleaseDC(window, deviceContext);

					u64 endCycleCount = __rdtsc();

					LARGE_INTEGER endCounter;
					QueryPerformanceCounter(&endCounter);

					u64 cyclesElapsed = endCycleCount - lastCycleCount;
					i64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
					// We cast these down to i32 because printf fmt specifiers work reliably
					// w/ i32, but not always w/ i64, apparently.
					i32 msPerFrame = (i32)((1000 * counterElapsed) / perfCountFrequency);
					i32 fps = perfCountFrequency / counterElapsed;
					i32 mcpf = (i32)(cyclesElapsed / (1000 * 1000));

	#if 0
					char buffer[256];
					wsprintfA(buffer, "%dms/f, %df/s, %dmc/f\n", msPerFrame, fps, mcpf);
					OutputDebugStringA(buffer);
	#endif

					lastCycleCount = endCycleCount;
					lastCounter = endCounter;

					GameInput* temp = newInput;
					newInput = oldInput;
					oldInput = temp;
				}
			}
		}
	}

	return 0;
}
