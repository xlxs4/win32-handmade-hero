#include <windows.h>

#include <cstdint>

#define internal		static
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

global_variable bool globalRunning;

struct Win32_OffscreenBuffer
{
	// slv: Pixels are always 32-bits wide, memory order BB GG RR XX
	BITMAPINFO info;
	void* 	   memory;
	int 	   width;
	int 	   height;
	int 	   pitch;
};

global_variable Win32_OffscreenBuffer globalBackbuffer;

struct Win32_Window_Dimension
{
	int width;
	int height;
};

internal Win32_Window_Dimension
Win32_GetWindowDimension(HWND window)
{
	Win32_Window_Dimension result;

	RECT clientRect;
	GetClientRect(window, &clientRect);
	result.width = clientRect.right - clientRect.left;
	result.height = clientRect.bottom - clientRect.top;

	return result;
}

internal void
Win32_RenderWeirdGradient(Win32_OffscreenBuffer buffer,
						  int 					xOffset,
						  int 					yOffset)
{
	u8* row = (u8*)buffer.memory;
	for(int y = 0; y < buffer.height; ++y)
	{
		u32* pixel = (u32*)row;
		for(int x = 0; x < buffer.width; ++x)
		{
            // slv: Due to little endian architecture, if you
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

		row += buffer.pitch;
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
	// slv: For uncompressed RGB bitmaps, if biHeight is positive, bitmap is a bottom-up DIB
	// with its origin at the lower-left corner. If it's negative, it's a top-down DIB
	// with its origin at the upper-left corner. We do top-down DIB here. Refer to the
	// BITMAPINFOHEADER structure in the Win32 API docs.
	buffer->info.bmiHeader.biHeight = -buffer->height;
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = 32;
	buffer->info.bmiHeader.biCompression = BI_RGB;

	int bitmapMemorySize = (buffer->width*buffer->height)*bytesPerPixel;
	buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	buffer->pitch = width*bytesPerPixel;
}

internal void
Win32_DisplayBufferInWindow(HDC deviceContext,
							int windowWidth, int windowHeight,
				   			Win32_OffscreenBuffer buffer)
{
	StretchDIBits(
		deviceContext,
		0, 0, windowWidth, windowHeight,
		0, 0, buffer.width, buffer.height,
		buffer.memory,
		&buffer.info,
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
			globalRunning = false;
		} break;

		case WM_CLOSE:
		{
			globalRunning = false;
		} break;
	
		case WM_ACTIVATEAPP:
		{
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT paint;
			HDC deviceContext = BeginPaint(window, &paint);

			Win32_Window_Dimension dimension = Win32_GetWindowDimension(window);
			Win32_DisplayBufferInWindow(
				deviceContext,
				dimension.width, dimension.height,
				globalBackbuffer
			);
			EndPaint(window, &paint);
		} break;

		case WM_SETCURSOR:
		{
			SetCursor(0);
		} break;

		default:
		{
			result = DefWindowProc(window, message, wParam, lParam);
		} break;
	}

	return result;
}

int
WinMain(HINSTANCE instance,
		HINSTANCE prevInstance,
		LPSTR     commandLine,
		int       showCode)
{
 	WNDCLASSA windowClass = {};

	Win32_ResizeDIBSection(&globalBackbuffer, 1280, 720);

	// slv: Casey added CS_OWNDC. We don't, because that's not good practice.
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
  			int xOffset = 0;
  			int yOffset = 0;
  			globalRunning = true;
  			while(globalRunning)
  			{
  				while(PeekMessage(&message, 0, 0, 0, PM_REMOVE))
  				{
  					if(message.message == WM_QUIT)
  					{
  						globalRunning = false;
  					}
	  				TranslateMessage(&message);
	  				DispatchMessageA(&message);
  				}

  				Win32_RenderWeirdGradient(globalBackbuffer, xOffset, yOffset);

  				HDC deviceContext = GetDC(window);
				Win32_Window_Dimension dimension = Win32_GetWindowDimension(window);
  				Win32_DisplayBufferInWindow(
  					deviceContext,
  					dimension.width, dimension.height,
  					globalBackbuffer
  				);
  				ReleaseDC(window, deviceContext);
  				++xOffset;
  				yOffset += 2;
  			}
  		}
  		else
  		{

  		}
 	}
 	else
 	{
 	}

 	return 0;
}
