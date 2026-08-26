#include <windows.h>
#include <xinput.h>
#include <dsound.h>

#include <cstdint>
#include <cmath>

#define internal        static
#define local_persist   static
#define global_variable static

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

#define PI32 3.14159265359

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
Win32_LoadXInput(void)
{
	// https://learn.microsoft.com/en-us/windows/win32/xinput/xinput-versions
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if(!XInputLibrary)
	{
		HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}
	if(!XInputLibrary)
	{
		HMODULE XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
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

// TODO: Use a more modern API instead of DirectShow?
struct Win32_OffscreenBuffer
{
	// Pixels are always 32-bits wide, memory order BB GG RR XX.
	BITMAPINFO info;
	void*      memory;
	int        width;
	int        height;
	int        pitch;
};

global_variable Win32_OffscreenBuffer gBackbuffer;

struct Win32_WindowDimension
{
	int width;
	int height;
};

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
Win32_RenderWeirdGradient(Win32_OffscreenBuffer* buffer,
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

struct Win32_SoundOutput
{
	int samplesPerSecond;
	int toneHz;
	i16 toneVolume;
	u32 runningSampleIndex;
	int wavePeriod;
	int bytesPerSample;
	int gSecondaryBufferSize;
	f32 tSine;
	int latencySampleCount;
};

internal void
Win32_FillSoundBuffer(Win32_SoundOutput* soundOutput, DWORD byteToLock, DWORD bytesToWrite)
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
		i16* sampleOut = (i16*)region1;
		for(DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex)
		{
			f32 sineValue = sinf(soundOutput->tSine);
			i16 sampleValue = (i16)(sineValue * soundOutput->toneVolume);
			*sampleOut++ = sampleValue;
			*sampleOut++ = sampleValue;
			soundOutput->tSine += 2.0f * PI32 / (f32)soundOutput->wavePeriod;
			++soundOutput->runningSampleIndex;
		}

		DWORD region2SampleCount = region2Size/soundOutput->bytesPerSample;
		sampleOut = (i16*)region2;
		for(DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex)
		{
			f32 sineValue = sinf(soundOutput->tSine);
			i16 sampleValue = (i16)(sineValue * soundOutput->toneVolume);
			*sampleOut++ = sampleValue;
			*sampleOut++ = sampleValue;
			soundOutput->tSine += 2.0f * PI32 / (f32)soundOutput->wavePeriod;
			++soundOutput->runningSampleIndex;
		}

		gSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
	}
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

			// Graphics test.
			int xOffset = 0;
			int yOffset = 0;

			// Sound test.
			Win32_SoundOutput soundOutput = {};
			soundOutput.samplesPerSecond = 48000;
			soundOutput.toneHz = 440;
			soundOutput.toneVolume = 4000;
			soundOutput.wavePeriod = soundOutput.samplesPerSecond / soundOutput.toneHz;
			soundOutput.bytesPerSample = sizeof(i16) * 2;
			soundOutput.gSecondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
			soundOutput.latencySampleCount = soundOutput.samplesPerSecond / 15;

			Win32_InitDSound(window, soundOutput.samplesPerSecond, soundOutput.gSecondaryBufferSize);
			Win32_FillSoundBuffer(&soundOutput, 0, soundOutput.latencySampleCount * soundOutput.bytesPerSample);
			gSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			gRunning = true;

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
				for (DWORD controllerIndex = 0;
					 controllerIndex < XUSER_MAX_COUNT;
					 ++controllerIndex)
				{
					XINPUT_STATE controllerState;
					if(XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
					{
						XINPUT_GAMEPAD* pad = &controllerState.Gamepad;

						b32 dPadUp    = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						b32 dPadDown  = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						b32 dPadLeft  = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						b32 dPadRight = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

						b32 btnStart = (pad->wButtons & XINPUT_GAMEPAD_START);
						b32 btnBack  = (pad->wButtons & XINPUT_GAMEPAD_BACK);

						b32 leftShoulder  = (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						b32 rightShoulder = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

						b32 btnA = (pad->wButtons & XINPUT_GAMEPAD_A);
						b32 btnB = (pad->wButtons & XINPUT_GAMEPAD_B);
						b32 btnX = (pad->wButtons & XINPUT_GAMEPAD_X);
						b32 btnY = (pad->wButtons & XINPUT_GAMEPAD_Y);

						i16 leftStickX = pad->sThumbLX;
						if(abs(leftStickX) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
						{
							xOffset += leftStickX >> 14;
						}

						i16 leftStickY = pad->sThumbLY;
						if(abs(leftStickY) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
						{
							yOffset -= leftStickY >> 14;
						}

						i16 rightStickY = pad->sThumbRY;
						if(abs(rightStickY) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
						{
							soundOutput.toneHz = 440 + (int)(256.0f * ((f32)rightStickY / 30000.0f));
							soundOutput.wavePeriod = soundOutput.samplesPerSecond/soundOutput.toneHz;
						}
					}
				}

				Win32_RenderWeirdGradient(&gBackbuffer, xOffset, yOffset);

				DWORD playCursor;
				// We don't care about the write cursor.
				DWORD writeCursor;
				if(SUCCEEDED(gSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor)))
				{
					DWORD byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) %
					                    soundOutput.gSecondaryBufferSize;
					DWORD targetCursor = (playCursor +
						                  (soundOutput.latencySampleCount * soundOutput.bytesPerSample)) %
					                      soundOutput.gSecondaryBufferSize;
					DWORD bytesToWrite;
					if(byteToLock > targetCursor)
					{
						bytesToWrite = soundOutput.gSecondaryBufferSize - byteToLock;
						bytesToWrite += targetCursor;
					}
					else
					{
						bytesToWrite = targetCursor - byteToLock;
					}
					Win32_FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite);
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

				char buffer[256];
				wsprintfA(buffer, "%dms/f, %df/s, %dmc/f\n", msPerFrame, fps, mcpf);
				OutputDebugStringA(buffer);

				lastCycleCount = endCycleCount;
				lastCounter = endCounter;
			}
		}
	}

	return 0;
}
