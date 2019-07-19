
/*
	- Make the right calls so Windows doesn't think we're "still loading" for a bit after we actuallly start
	- Save game locations
	- Getting a handle to our own executable file
	- Assert loading path
	- Threading (launch a thread)
	- Raw Iput (support for multiple keyboards)
	- ClipCursor (for mutimonitor support)
	- QuerryCancelAutoplay
	- WM_ACTIVATEAPP (for when we are not the active application)
	- Blit speed improvements (BitBlt)
	- Hardware acceleration (OpenGl or Direct3D or BOTH)
	- GetKeyboardLayout (for French keyboards, internation WASD support)
	- ChngeDisplaySetting option if we detect slow fullscreen blit
*/

#include "handmade_platform.h"
#include "handmade_shared.h"

#include <windows.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>
#include <gl/gl.h>

#include "win32_handmade.h"

global_variable platform_api Platform;

enum win32_rendering_type
{
	Win32RenderType_RenderOpenGL_DisplayOpenGL,
	Win32RenderType_RenderSoftware_DisplayOpenGL,
	Win32RenderType_RenderSoftware_DisplayGDI,
};

global_variable win32_rendering_type GlobalRenderingType;
global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;
global_variable bool32 DEBUGGlobalShowCursor;
global_variable WINDOWPLACEMENT GlobalWindowPosition = {sizeof(GlobalWindowPosition)};
global_variable GLuint GlobalBlitTextureHandle;

#define WGL_DRAW_TO_WINDOW_ARB                  0x2001
#define WGL_ACCELERATION_ARB                    0x2003
#define WGL_SUPPORT_OPENGL_ARB                  0x2010
#define WGL_DOUBLE_BUFFER_ARB                   0x2011
#define WGL_PIXEL_TYPE_ARB                      0x2013

#define WGL_FULL_ACCELERATION_ARB               0x2027
#define WGL_TYPE_RGBA_ARB                       0x202B

#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB        0x20A9

typedef HGLRC WINAPI wgl_create_context_attribs_arb(HDC hDC, HGLRC hShareContext,
	 const int *attribList);

typedef BOOL WINAPI wgl_get_pixel_format_attrib_iv_arb(HDC hdc,
	int iPixelFormat,
	int iLayerPlane,
	UINT nAttributes,
	const int *piAttributes,
	int *piValue);

typedef BOOL WINAPI wgl_get_pixel_format_attrib_fv_arb(HDC hdc,
	int iPixelFormat,
	int iLayerPlane,
	UINT nAttributes,
	const int *piAttributes,
	FLOAT *piValue);

typedef BOOL WINAPI wgl_choose_pixel_format_arb(HDC hdc,
	const int *piAttribList,
	const FLOAT *pfAttribFList,
	UINT nMaxFormats,
	int *piFormats,
	UINT *nNumFormats);

typedef BOOL WINAPI wgl_swap_interval_ext(int interval);
typedef const char * WINAPI wgl_get_extension_string_ext(void);

global_variable wgl_create_context_attribs_arb *wglCreateContextAttribsARB;
global_variable wgl_choose_pixel_format_arb *wglChoosePixelFormatARB;
global_variable wgl_swap_interval_ext *wglSwapIntervalEXT;
global_variable wgl_get_extension_string_ext *wglGetExtensionStringEXT;
global_variable b32 OpenGLSupportsSRGBFramebuffer;
global_variable GLuint OpenGLDefaultInternalTextureFormat;
global_variable GLuint OpenGLReserveBlitTextrue;

#include "handmade_sort.cpp"
#include "handmade_opengl.cpp"
#include "handmade_render.cpp"

//NOTE: XInputGetStae
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//NOTE: XInputSetStae
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND * ppDS, LPUNKNOWN  pUnkOuter )
typedef DIRECT_SOUND_CREATE(direct_sound_create);

#if HANDMADE_INTERNAL
DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
	if (Memory)
	{
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
	debug_read_file_result Result = {};

	HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize;
		if (GetFileSizeEx(FileHandle, &FileSize))
		{
			uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
			Result.Contents = VirtualAlloc(0, FileSize.QuadPart, MEM_COMMIT, PAGE_READWRITE);
			if (Result.Contents)
			{
				DWORD BytesRead;
				if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
					(FileSize32 == BytesRead))
				{
					// NOTE: File read successfully
					Result.ContentsSize = FileSize32;
				}
				else
				{
					// TODO: Loggin
					DEBUGPlatformFreeFileMemory(Result.Contents);
					Result.Contents = 0;
				}
			}
			else
			{
				// TODO: Loggin
			}
		}
		else
		{
			// TODO: Loggin
		}
		CloseHandle(FileHandle);
	}
	else
	{
		// TODO: Loggin
	}

	return(Result);
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
	bool32 Result = false;

	HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten;
		if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
		{
			// NOTE: File read successfully
			Result = (BytesWritten == MemorySize);
		}
		else
		{
			// TODO: Loggin
		}

		CloseHandle(FileHandle);
	}
	else
	{
		// TODO: Loggin
	}

	return(Result);
}

DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(DEBUGExecuteSystemCommand)
{
	debug_executing_process Result = {};

	STARTUPINFO StartupInfo = {};
	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
	StartupInfo.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION ProcessInfo = {};
	if (CreateProcess(
		Command,
		CommandLine,
		0,
		0,
		FALSE,
		0,
		0,
		Path,
		&StartupInfo,
		&ProcessInfo))
	{
		Assert(sizeof(Result.OSHandle) >= sizeof(ProcessInfo.hProcess));
		*(HANDLE *)&Result.OSHandle = ProcessInfo.hProcess;	
	}
	else
	{
		*(HANDLE *)&Result.OSHandle = INVALID_HANDLE_VALUE;
	}

	return(Result);
}

DEBUG_PLATFORM_GET_SYSTEM_PROCESS_STATE(DEBUGGetProcessState)
{
	debug_process_state Result = {};

	HANDLE hProcess = *(HANDLE *)&Process.OSHandle;
	if (hProcess != INVALID_HANDLE_VALUE)
	{
		Result.StartedSuccessfully = true;
		if (WaitForSingleObject(hProcess, INFINITE) == WAIT_OBJECT_0)
		{
			DWORD ReturnCode = 0;
			GetExitCodeProcess(hProcess, &ReturnCode);
			Result.ReturnCode = ReturnCode;
			CloseHandle(hProcess);
		}
		else
		{
			Result.IsRunning = true;
		}
	}

	return(Result);
}
#endif

inline FILETIME
Win32GetLastWriteTime(char *Filename)
{
	FILETIME LastWriteTime = {};

	WIN32_FILE_ATTRIBUTE_DATA Data;
	if (GetFileAttributesExA(Filename, GetFileExInfoStandard, &Data))
	{
		LastWriteTime = Data.ftLastWriteTime;
	}
	return(LastWriteTime);
}

inline b32
Win32TimeIsValid(FILETIME Time)
{
	b32 Result = (Time.dwLowDateTime != 0) && (Time.dwHighDateTime != 0);
	return(Result);
}

internal win32_game_code
Win32LoadGameCode(char *SourceDLLName, char *TempDLLName, char *LockFileName)
{
	win32_game_code Result = {};

	WIN32_FILE_ATTRIBUTE_DATA Ignored;
	if (!GetFileAttributesEx(LockFileName, GetFileExInfoStandard, &Ignored))
	{
		Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);
		
		CopyFile(SourceDLLName, TempDLLName, FALSE);

		Result.GameCodeDLL = LoadLibraryA(TempDLLName);
		if (Result.GameCodeDLL)
		{
			Result.UpdateAndRender = (game_update_and_render *)
				GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");

			Result.GetSoundSamples = (game_get_sound_samples *)
				GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");
			
			Result.DEBUGFrameEnd = (debug_game_frame_end *)
				GetProcAddress(Result.GameCodeDLL, "DEBUGGameFrameEnd");

			Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples && Result.DEBUGFrameEnd);
		}

	}

	if (!Result.IsValid)
	{
		DWORD Error = GetLastError();
		Result.UpdateAndRender = 0;
		Result.GetSoundSamples = 0;
		Result.DEBUGFrameEnd = 0;
	}

	return(Result);
}

internal void
Win32UnloadGameCode(win32_game_code *GameCode)
{
	if (GameCode->GameCodeDLL)
	{
		FreeLibrary(GameCode->GameCodeDLL);
		GameCode->GameCodeDLL = 0;
	}

	GameCode->IsValid = false;
	GameCode->UpdateAndRender = 0;
	GameCode->GetSoundSamples = 0;
}

internal void
Win32LoadXInput(void)
{
	//TODO: Test this on Windows 8
	HMODULE XinputLibrary = LoadLibraryA("xinput1_4.dll");

	if (!XinputLibrary)
	{
		XinputLibrary = LoadLibraryA("xinput1_3.dll");
	}

	if (XinputLibrary)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(XinputLibrary, "XInputGetState");
		if (!XInputGetState) {XInputGetState = XInputGetStateStub;}
		XInputSetState = (x_input_set_state *)GetProcAddress(XinputLibrary, "XInputSsetState");
		if (!XInputSetState) {XInputSetState = XInputSetStateStub;}
	}
	else
	{
		// TODO: Diagnostic
	}
}

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
	// NOTE: Load the library
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

	if (DSoundLibrary)
	{
		// NOTE: Get a DirectSound object
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)
			GetProcAddress(DSoundLibrary, "DirectSoundCreate");

		LPDIRECTSOUND DirectSound;
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDecsription = {};
				BufferDecsription.dwSize = sizeof(BufferDecsription);
				BufferDecsription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				// NOTE: "Create" a primaty buffer
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDecsription, &PrimaryBuffer, 0)))
				{
					HRESULT Error = SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat));  
					if(SUCCEEDED(Error))
					{
						OutputDebugStringA("PrimaryBuffer\n");
						// NOTE: We have finally set the format
					}
					else
					{
						// TODO: Diagnostic
					}
				}
				else
				{
					// TODO: Diagnostic
				}
			}
			else
			{
				// TODO: Diagnostic
			}
			// NOTE: "Create" a secondary buffer

			DSBUFFERDESC BufferDecsription = {};
			BufferDecsription.dwSize = sizeof(BufferDecsription);
			BufferDecsription.dwFlags = DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_GLOBALFOCUS;
#if HANDMADE_INTERNAL
			BufferDecsription.dwFlags |= DSBCAPS_GLOBALFOCUS;
#endif
			BufferDecsription.dwBufferBytes = BufferSize;
			BufferDecsription.lpwfxFormat = &WaveFormat;

			HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDecsription, &GlobalSecondaryBuffer, 0);
			if (SUCCEEDED(Error))
			{
				OutputDebugStringA("GlobalSecondaryBuffer\n");
			}
			// NOTE: Start it playing
		}
		else
		{
			// TODO: Diagnostic
		}
	}
	else
	{
		// TODO: Diagnostic
	}
}

int Win32OpenGLAttribs[] = 
{
	WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
	WGL_CONTEXT_MINOR_VERSION_ARB, 0,
	WGL_CONTEXT_FLAGS_ARB, 0//NOTE: Enable for testing WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB
#if HANDMADE_INTERNAL
	|WGL_CONTEXT_DEBUG_BIT_ARB
#endif
	,
	WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
	0,
};

internal win32_thread_startup
Win32ThreadStartupForGL(HDC OpenGLDC, HGLRC ShareContext)
{
	win32_thread_startup Result = {};

	Result.OpenGLDC = OpenGLDC;
	if (wglCreateContextAttribsARB)
	{
		Result.OpenGLRC = wglCreateContextAttribsARB(OpenGLDC, ShareContext, Win32OpenGLAttribs);
	}

	return(Result);
}

internal void
Win32SetPixelFormat(HDC WindowDC)
{
	int SuggestedPixelFormatIndex = 0;
	GLuint ExtendedPick = 0;

	if (wglChoosePixelFormatARB)
	{
		int IntAttribList[] = 
		{
			WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
			WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
			WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
#if HANDMADE_STREAMING
			WGL_DOUBLE_BUFFER_ARB, GL_FALSE,
#else
			WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
#endif
			WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
			WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
			0,
		};

		if (!OpenGLSupportsSRGBFramebuffer)
		{
			IntAttribList[10] = 0;
		}

		wglChoosePixelFormatARB(WindowDC, IntAttribList, 0, 1,
			&SuggestedPixelFormatIndex, &ExtendedPick);
	}

	if (!ExtendedPick)
	{
		PIXELFORMATDESCRIPTOR DesirePixelFormat = {};
		DesirePixelFormat.nSize = sizeof(DesirePixelFormat);
		DesirePixelFormat.nVersion = 1;
		DesirePixelFormat.iPixelType = PFD_TYPE_RGBA;
#if HANDMADE_STREAMING
		// NOTE: PFD_DOUBLEBUFFER appears to prevent OBS from reliably streaming the window
		DesirePixelFormat.dwFlags = PFD_SUPPORT_OPENGL|PFD_DRAW_TO_WINDOW;
#else
		DesirePixelFormat.dwFlags = PFD_SUPPORT_OPENGL|PFD_DRAW_TO_WINDOW|PFD_DOUBLEBUFFER;
#endif	
		DesirePixelFormat.cColorBits = 32;
		DesirePixelFormat.cAlphaBits = 8;
		DesirePixelFormat.iLayerType = PFD_MAIN_PLANE;

		SuggestedPixelFormatIndex = ChoosePixelFormat(WindowDC, &DesirePixelFormat);
	}

	PIXELFORMATDESCRIPTOR SuggestedPixelFormat;
	DescribePixelFormat(WindowDC, SuggestedPixelFormatIndex,
		sizeof(SuggestedPixelFormat), &SuggestedPixelFormat);
	SetPixelFormat(WindowDC, SuggestedPixelFormatIndex, &SuggestedPixelFormat);
}

internal void
Win32LoadWGLExtension(void)
{
	WNDCLASSA WindowClass = {};

	WindowClass.lpfnWndProc = DefWindowProcA;
	WindowClass.hInstance = GetModuleHandle(0);
	WindowClass.lpszClassName = "HandMadeWGLLoader";

	if (RegisterClass(&WindowClass))
	{
		HWND Window = 
			CreateWindowEx(
				0,
				WindowClass.lpszClassName,
				"Handmade Hero",
				0,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				0,
				0,
				WindowClass.hInstance,
				0);

		HDC WindowDC = GetDC(Window);
		Win32SetPixelFormat(WindowDC);
		HGLRC OpenGLRC = wglCreateContext(WindowDC);
		if (wglMakeCurrent(WindowDC, OpenGLRC))
		{
			wglChoosePixelFormatARB = 
				(wgl_choose_pixel_format_arb *)wglGetProcAddress("wglChoosePixelFormatARB");
			wglCreateContextAttribsARB = 
				(wgl_create_context_attribs_arb *)wglGetProcAddress("wglCreateContextAttribsARB");
			wglSwapIntervalEXT = (wgl_swap_interval_ext *)wglGetProcAddress("wglSwapIntervalEXT");
			wglGetExtensionStringEXT = (wgl_get_extension_string_ext *)wglGetProcAddress("wglGetExtensionsStringARB");

			if (wglGetExtensionStringEXT)
			{
				char *Extensions = (char *)wglGetExtensionStringEXT();
				char *At = Extensions;
				for (;*At;)
				{
					while (IsWhitespace(*At)){++At;}
					char *End = At;
					while (*End && !IsWhitespace(*End)) {++End;}

					umm Count = End - At;

					if (StringAreEqual(Count, At, "WGL_EXT_framebuffer_sRGB")) {OpenGLSupportsSRGBFramebuffer = true;}

					At = End;
				}
			}

			wglMakeCurrent(0, 0);
		}
		
		wglDeleteContext(OpenGLRC);
		ReleaseDC(Window, WindowDC);
		DestroyWindow(Window);
	}
}

internal HGLRC
Win32InitOpenGL(HDC WindowDC)
{
	Win32LoadWGLExtension();

	b32 ModernContext = true;
	HGLRC OpenGLRC = 0;
	if (wglCreateContextAttribsARB)
	{
		Win32SetPixelFormat(WindowDC);
		OpenGLRC = wglCreateContextAttribsARB(WindowDC, 0, Win32OpenGLAttribs);
	}

	if (!OpenGLRC)
	{
		ModernContext = false;
		OpenGLRC = wglCreateContext(WindowDC);
	}

	if (wglMakeCurrent(WindowDC, OpenGLRC))
	{
		OpenGLInit(ModernContext, OpenGLSupportsSRGBFramebuffer);
		if (wglSwapIntervalEXT)
		{
			wglSwapIntervalEXT(1);
		}

		glGenTextures(1, &OpenGLReserveBlitTextrue);
	}
	
	return(OpenGLRC);
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return(Result);
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;

	int BytesPerPixel = 4;
	Buffer->BytesPerPixel = BytesPerPixel;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	Buffer->Pitch = Align16(Width*BytesPerPixel);
	int BitmapMemorySize = (Buffer->Pitch*Buffer->Height);
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

internal void
Win32DisplayBufferInWindow(
	platform_work_queue *RenderQueue,
	game_render_commands *Commands,
	HDC DeviceContext, s32 WindowWidth, s32 WindowHeight,
	void *SortMemory, void *ClipRectMemory)
{
	SortEntries(Commands, SortMemory);
	LinearizeClipRects(Commands, ClipRectMemory);

	/* TODO: Do we want to check for resources like before? Probably?
		if (AllResourcesPresent(RenderGroup))
		{
			RenderToOutput(TranState->HighPriorityQueue, RenderGroup, DrawBuffer, &TranState->TranArena);
		}
	*/

	if (GlobalRenderingType == Win32RenderType_RenderOpenGL_DisplayOpenGL)
	{
		OpenGLRenderCommands(Commands, WindowWidth, WindowHeight);
		SwapBuffers(DeviceContext);
	}
	else
	{
		loaded_bitmap OutputTarget;
		OutputTarget.Memory = GlobalBackbuffer.Memory;
		OutputTarget.Width = GlobalBackbuffer.Width;
		OutputTarget.Height = GlobalBackbuffer.Height;
		OutputTarget.Pitch = GlobalBackbuffer.Pitch;

		SoftwareRenderCommands(RenderQueue, Commands, &OutputTarget);

		b32 DisplayViaHardware = true;
		if (GlobalRenderingType == Win32RenderType_RenderSoftware_DisplayOpenGL)
		{
			OpenGLDisplayBitmap(GlobalBackbuffer.Width, GlobalBackbuffer.Height, GlobalBackbuffer.Memory,
				GlobalBackbuffer.Pitch, WindowWidth, WindowHeight,
				OpenGLReserveBlitTextrue);
			SwapBuffers(DeviceContext);
		}
		else
		{
			Assert(GlobalRenderingType == Win32RenderType_RenderSoftware_DisplayGDI);
			// TODO: Centering / black bars?
			
			if ((WindowWidth >= GlobalBackbuffer.Width*2) &&
				(WindowHeight >= GlobalBackbuffer.Height*2))
			{
				StretchDIBits(
					DeviceContext,
					0, 0, 2*GlobalBackbuffer.Width, 2*GlobalBackbuffer.Height,
					0, 0, GlobalBackbuffer.Width, GlobalBackbuffer.Height,
					GlobalBackbuffer.Memory, 
					&GlobalBackbuffer.Info,
					DIB_RGB_COLORS, SRCCOPY);
			}
			else
			{
#if 0
				int OffsetX = 10
				int OffsetY = 10;

				PatBlt(DeviceContext, 0, 0, WindowWidth, OffsetY, BLACKNESS);
				PatBlt(DeviceContext, 0, OffsetY + Buffer->Height, WindowWidth, WindowHeight, BLACKNESS);
				PatBlt(DeviceContext, 0, 0, OffsetX, WindowHeight, BLACKNESS);
				PatBlt(DeviceContext, OffsetX + Buffer->Width, 0, WindowWidth, WindowHeight, BLACKNESS);
#else
				int OffsetX = 0;
				int OffsetY = 0;
#endif
				StretchDIBits(
					DeviceContext,
					OffsetX, OffsetY, GlobalBackbuffer.Width, GlobalBackbuffer.Height,
					0, 0, GlobalBackbuffer.Width, GlobalBackbuffer.Height,
					GlobalBackbuffer.Memory, 
					&GlobalBackbuffer.Info,
					DIB_RGB_COLORS, SRCCOPY);
			}
		}
	}
}

internal LRESULT CALLBACK 
Win32FadeWindowCallback(
	HWND Window,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam)
{
	LRESULT Result = 0;

	switch(Message)
	{
		case WM_CLOSE:
		{
		} break;
		
		case WM_SETCURSOR:
		{
			SetCursor(0);
		} break;
		
		default:
		{
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}

	return(Result);
}	

internal LRESULT CALLBACK 
Win32MainWindowCallback(
	HWND Window,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam)
{
	LRESULT Result = 0;

	switch(Message)
	{
		case WM_CLOSE:
		{
			GlobalRunning = false;
		} break;
		case WM_SETCURSOR:
		{
			if (DEBUGGlobalShowCursor)
			{
				Result = DefWindowProc(Window, Message, WParam, LParam);
			}
			else
			{
				SetCursor(0);
			}	
		} break;
		case WM_SIZE:
		{
		} break;
		case WM_ACTIVATEAPP:
		{
			if (WParam == TRUE)
			{
				SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
			}
			else
			{
				SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 64, LWA_ALPHA);
			}
		} break;
		case WM_DESTROY:
		{
			GlobalRunning = false;
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			Assert(!"NOOOOOOOOOO");
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint = {};
			HDC DeviceContext = BeginPaint(Window, &Paint);
#if 0
			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(
				&GlobalBackbuffer, DeviceContext,
				Dimension.Width, Dimension.Height);
#endif
			EndPaint(Window, &Paint);
		} break;
		default:
		{
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}

	return(Result);
}

internal void
Win32ClearBuffer(win32_sound_output *SoundOutput)
{
	void *Region1;
	DWORD Region1Size;
	void *Region2;
	DWORD Region2Size;
	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(
			0, SoundOutput->SecondaryByfferSize,
			&Region1, &Region1Size,
			&Region2, &Region2Size,
			0)))
	{
		uint8 *DestSample = (uint8 *)Region1;
		for (DWORD ByteIndex = 0;
			ByteIndex < Region1Size;
			++ByteIndex)
		{
			*DestSample++ = 0;
		}

		DestSample = (uint8 *)Region2;
		for (DWORD ByteIndex = 0;
			ByteIndex < Region2Size;
			++ByteIndex)
		{
			*DestSample++ = 0;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}


internal void
Win32FillSoundBuffer(win32_sound_output *SoundBuffer, DWORD ByteToLock, DWORD BytesToWrite, 
					game_sound_output_buffer *SourceBuffer)
{
	void *Region1;
	DWORD Region1Size;
	void *Region2;
	DWORD Region2Size;
	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(
			ByteToLock, BytesToWrite,
			&Region1, &Region1Size,
			&Region2, &Region2Size,
			0)))
	{
		// TODO: assert that Region1Size/Region2 is valid
		DWORD Region1SampleCount = Region1Size/SoundBuffer->BytesPerSample;
		int16 *DestSample = (int16 *)Region1;
		int16 *SourceSample = SourceBuffer->Samples;
		for (DWORD SampleIndex = 0;
			SampleIndex < Region1SampleCount;
			++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundBuffer->RunningSampleIndex;
		}

		DWORD Region2SampleCount = Region2Size/SoundBuffer->BytesPerSample;
		DestSample = (int16 *)Region2;
		for (DWORD SampleIndex = 0;
			SampleIndex < Region2SampleCount;
			++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundBuffer->RunningSampleIndex;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}


internal void
Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
	if(NewState->EndedDown != IsDown)
	{
		NewState->EndedDown = IsDown;
		++NewState->HalfTransitionCount;
	}
}


internal void
Win32ProcessXinputDigitalButton(DWORD XinputButtonState,
								game_button_state *OldState, DWORD ButtonBit,
								game_button_state *NewState)
{
	NewState->EndedDown = (XinputButtonState & ButtonBit) == ButtonBit; 
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
	real32 Result = 0;
	if (Value < -DeadZoneThreshold)
	{
		Result = (real32)Value / 32768.0f;
	}
	else if (Value > DeadZoneThreshold)
	{
		Result = (real32)Value / 32767.0f;
	}

	return(Result);
}

void
CatStrings(
	size_t SourceACount, char *SourceA,
	size_t SourceBCount, char *SourceB,
	int DestCount, char *Dest)
{
	// TODO: Dest bounds checking
	
	for (int Index = 0;
		Index < SourceACount;
		++Index)
	{
		*Dest++ = *SourceA++;
	}

	for (int Index = 0;
		Index < SourceBCount;
		++Index)
	{
		*Dest++ = *SourceB++;
	}

	*Dest++ = 0;
}

internal void
Win32BuildEXEPathFileName(win32_state *State, char *FileName, int DestCount, char *Dest)
{
	CatStrings(
		State->OnePastLastEXEFileNameSlash - State->EXEFileName, State->EXEFileName,
		StringLength(FileName), FileName,
		DestCount, Dest);
}

internal void
Win32GetInputFileLocation(win32_state *State, bool32 InputStream, int SlotIndex, int DestCount, char *Dest)
{
	char Temp[64];
	wsprintf(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
	Win32BuildEXEPathFileName(State, Temp, DestCount, Dest);
}

internal win32_replay_buffer *
Win32GetReplayBuffer(win32_state *State, int unsigned Index)
{
	Assert(Index > 0);
	Assert(Index < ArrayCount(State->ReplayBuffers));
	win32_replay_buffer *Result = &State->ReplayBuffers[Index];
	return(Result);
}

internal void
Win32BeginRecordingInput(win32_state *State, int InputRecordingIndex)
{
	win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputRecordingIndex);
	if (ReplayBuffer->MemoryBlock)
	{
		State->InputRecordingIndex = InputRecordingIndex;

		char FileName[WIN32_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(State, true, InputRecordingIndex, sizeof(FileName), FileName);
		State->RecordingHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
#if 0
		LARGE_INTEGER FilePosition;
		FilePosition.QuadPart = State->TotalSize;
		SetFilePointerEx(State->RecordingHandle, FilePosition, 0, FILE_BEGIN);
#endif
		CopyMemory(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
	}
}

internal void
Win32EndRecordingInput(win32_state *State)
{
	CloseHandle(State->RecordingHandle);
	State->InputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayBack(win32_state *State, int InputPlayingIndex)
{
	win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputPlayingIndex);
	if (ReplayBuffer->MemoryBlock)
	{
		State->InputPlayingIndex = InputPlayingIndex;
		char FileName[WIN32_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(State, true, InputPlayingIndex, sizeof(FileName), FileName);
		State->PlaybackHandle = CreateFileA(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
#if 0
		LARGE_INTEGER FilePosition;
		FilePosition.QuadPart = State->TotalSize;
		SetFilePointerEx(State->PlaybackHandle, FilePosition, 0, FILE_BEGIN);
#endif
		CopyMemory(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
	}
}

internal void
Win32EndInputPlayBack(win32_state *State)
{
	CloseHandle(State->PlaybackHandle);
	State->InputPlayingIndex = 0;
}

internal void
Win32RecordInput(win32_state *State, game_input *NewInput)
{
	DWORD BytesWritten;
	WriteFile(State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void
Win32PlayBackInput(win32_state *State, game_input *NewInput)
{
	DWORD BytesRead = 0;
	if (ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
	{
		// NOTE: There's still input
		if (BytesRead == 0)
		{
			// NOTE: We've hit the end of the stream go back to the beginning
			int PlayingIndex = State->InputPlayingIndex;
			Win32EndInputPlayBack(State);
			Win32BeginInputPlayBack(State, PlayingIndex);
		}
	}
}

void
ToggleFullscreen(HWND Window)
{
	// NOTE: This foloow Raymond Chan's prescription
	// for fullscreen toggling, see:
	// https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353/

	DWORD Style = GetWindowLong(Window, GWL_STYLE);
	if (Style & WS_OVERLAPPEDWINDOW) {
		MONITORINFO MonitorInfo = {sizeof(MonitorInfo)};
		if (GetWindowPlacement(Window, &GlobalWindowPosition) &&
			GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
		{
			SetWindowLong(Window, GWL_STYLE, Style & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(Window, HWND_TOP,
				MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
				MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
				MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	}
	else
	{
		SetWindowLong(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(Window, &GlobalWindowPosition);
		SetWindowPos(Window, NULL, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
			SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

internal void
Win32ProcessPendingMessages(win32_state *State, game_controller_input *KeyboardController)
{
	MSG Message;
	while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
	{
		switch(Message.message)
		{
			case WM_QUIT:
			{
				GlobalRunning = false;
			} break;

			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				uint32 VKCode = (uint32)Message.wParam;
				bool WasDown = ((Message.lParam & (1 << 30)) != 0);
				bool IsDown = ((Message.lParam & (1 << 31)) == 0);

				if (WasDown != IsDown)
				{
					if (VKCode == 'W')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
					} 
					else if (VKCode == 'A')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
					}
					else if (VKCode == 'S')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
					}
					else if (VKCode == 'D')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
					}
					else if (VKCode == 'Q')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
					}
					else if (VKCode == 'E')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
					}
					else if (VKCode == VK_UP)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
					}
					else if (VKCode == VK_LEFT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
					}
					else if (VKCode == VK_DOWN)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
					}
					else if (VKCode == VK_RIGHT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
					}
					else if (VKCode == VK_ESCAPE)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
					}
					else if (VKCode == VK_SPACE)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
					}
#if HANDMADE_INTERNAL
					else if (VKCode == 'P')
					{
						if (IsDown)
						{
							GlobalPause = !GlobalPause;
						}
					}
					else if (VKCode == 'L')
					{
						if (IsDown)
						{
							if (State->InputPlayingIndex == 0)
							{
								if (State->InputRecordingIndex == 0)
								{
									Win32BeginRecordingInput(State, 1);
								}
								else
								{
									Win32EndRecordingInput(State);
									Win32BeginInputPlayBack(State, 1);
								}
							}
							else
							{
								Win32EndInputPlayBack(State);
							}
						}
					}
#endif
				}

				if (IsDown)
				{
					bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
					if ((VKCode == VK_F4) && AltKeyWasDown)
					{
						GlobalRunning = false;
					}
					if ((VKCode == VK_RETURN) && AltKeyWasDown)
					{
						if (Message.hwnd)
						{
							ToggleFullscreen(Message.hwnd);
						}
					}
				}
			} break;
			default:
			{
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			} break;
		}
	}
}

inline LARGE_INTEGER
Win32GetWallClock(void)
{
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);

	return(Result);
}

inline r32
Win32GetWallClockSeconds()
{
	r32 Result = (r64)Win32GetWallClock().QuadPart / (r64)GlobalPerfCountFrequency;
	return(Result);
}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	real32 Result = (((real32)(End.QuadPart - Start.QuadPart)) / (real32)GlobalPerfCountFrequency);

	return(Result);
}

#if 0
internal void
HandleDebugCycleCounters(game_memory *Memory)
{
#if HANDMADE_INTERNAL
	OutputDebugStringA("DEBUG CYCLE COUNTS:\n");
	for (int CounterIndex = 0;
		CounterIndex < ArrayCount(Memory->Counters);
		++CounterIndex)
	{
		debug_cycle_counter *Counter = Memory->Counters + CounterIndex;
		
		if (Counter->HitCount)
		{
			char TextBuffer[256];
			_snprintf_s(TextBuffer, sizeof(TextBuffer),
				"  %d: %I64ucy %uh %I64ucy/h\n",
				CounterIndex,
				Counter->CycleCount,
				Counter->HitCount,
				Counter->CycleCount / Counter->HitCount);
			OutputDebugStringA(TextBuffer);
			Counter->HitCount = 0;
			Counter->CycleCount = 0;
		}
	}
#endif
}
#endif

#if 0
internal void
Win32DebugDrawVertical(
	win32_offscreen_buffer *BackBaffer, 
	int X, int Top, int Bottom, uint32 Color)
{
	if (Top < 0)
	{
		Top = 0;
	}

	if (Bottom >= BackBaffer->Height)
	{
		Bottom = 0;
	}

	if ((X >= 0) && (X < BackBaffer->Width))
	{
		uint8 *Pixel = 
			(uint8 *)BackBaffer->Memory + 
			X*BackBaffer->BytesPerPixel +
			Top*BackBaffer->Pitch;

		for (int Y = Top;
			Y < Bottom;
			++Y)
		{
			*(uint32 *)Pixel = Color;
			Pixel += BackBaffer->Pitch;
		}
	}
}

internal void
Win32DrawSoundBufferMarker(
	win32_offscreen_buffer *BackBaffer, 
	win32_sound_output *SoundOutput,
	real32 C, int PadX, int Top, int Bottom,
	DWORD Value, uint32 Color)
{
	real32 XReal32 = (C * (real32)Value);
	int X = PadX  + (int)XReal32;
	Win32DebugDrawVertical(BackBaffer, X, Top, Bottom, Color);
}

internal void
Win32DebugSyncDisplay(
	win32_offscreen_buffer *BackBaffer, 
	int MarkerCount, win32_debug_time_marker *Markers,
	int CurrentMarker,
	win32_sound_output *SoundOutput, real32 TargetSecondPerFrame)
{
	int PadX = 16;
	int PadY = 16;

	int LineHeight = 64;

	real32 C = (real32)(BackBaffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryByfferSize;
	for (int MarkerIndex = 0;
		MarkerIndex < MarkerCount;
		++MarkerIndex)
	{
		win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
		Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryByfferSize);
		Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryByfferSize);
		Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryByfferSize);
		Assert(ThisMarker->OutputByteCount < SoundOutput->SecondaryByfferSize);
		Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryByfferSize);
		Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryByfferSize);

		DWORD PlayColor = 0xFFFFFFFF;
		DWORD WriteColor = 0xFFFF0000;
		DWORD ExpectedFlipColor = 0xFFFFFF00;
		DWORD PlayWindowColor = 0xFFFF00FF;

		int Top = PadY;
		int Bottom = PadY + LineHeight;
		if (MarkerIndex == CurrentMarker)
		{
			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;

			int FirstTop = Top;

			Win32DrawSoundBufferMarker(BackBaffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor, PlayColor);
			Win32DrawSoundBufferMarker(BackBaffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);
			
			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;

			Win32DrawSoundBufferMarker(BackBaffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayColor);
			Win32DrawSoundBufferMarker(BackBaffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation +ThisMarker->OutputByteCount, WriteColor);

			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;

			Win32DrawSoundBufferMarker(BackBaffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
		}

		Win32DrawSoundBufferMarker(BackBaffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
		Win32DrawSoundBufferMarker(BackBaffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480*SoundOutput->BytesPerSample, PlayWindowColor);
		Win32DrawSoundBufferMarker(BackBaffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
	}
}
#endif

internal void
Win32GetEXEFileName(win32_state *State)
{
	DWORD SizeOfFilename = GetModuleFileNameA(0, State->EXEFileName, sizeof(State->EXEFileName));
	State->OnePastLastEXEFileNameSlash = State->EXEFileName;
	for (char *Scan = State->EXEFileName;
		*Scan;
		++Scan)
	{
		if (*Scan == '\\')
		{
			State->OnePastLastEXEFileNameSlash = Scan + 1;
		}
	}
}

internal void
Win32AddEntry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data)
{
	// TODO: Switch to InterlockedCompareExchange eventually
	// so that any thread can add?
	uint32 NewNextEntryToWrite = (Queue->NextEntryToWrite + 1) % ArrayCount(Queue->Entries);
	Assert(NewNextEntryToWrite != Queue->NextEntryToRead);
	platform_work_queue_entry *Entry = Queue->Entries + Queue->NextEntryToWrite;
	Entry->Callback = Callback;
	Entry->Data = Data;
	++Queue->CompletionGoal;
	_WriteBarrier();
	Queue->NextEntryToWrite = NewNextEntryToWrite;
	ReleaseSemaphore(Queue->SemaphoreHandle, 1, 0);
}

internal bool32
Win32DoNextWorkQueueEntry(platform_work_queue *Queue)
{
	bool32 WeShouldSleep = false;

	uint32 OriginalNextEntryToRead = Queue->NextEntryToRead;
	uint32 NewNextEntryToRead = (OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries);
	if (OriginalNextEntryToRead != Queue->NextEntryToWrite)
	{	
		uint32 Index = InterlockedCompareExchange(
			(uint32 volatile *)&Queue->NextEntryToRead,
			NewNextEntryToRead,
			OriginalNextEntryToRead);
		
		if (Index == OriginalNextEntryToRead)
		{
			platform_work_queue_entry Entry = Queue->Entries[Index];
			Entry.Callback(Queue, Entry.Data);
			InterlockedIncrement((uint32 volatile *)&Queue->CompletionCount);
		}
	}
	else
	{
		WeShouldSleep = true;
	}

	return(WeShouldSleep);
}

internal void
Win32CompleteAllWork(platform_work_queue *Queue)
{
	while(Queue->CompletionGoal != Queue->CompletionCount)
	{
		Win32DoNextWorkQueueEntry(Queue);
	}

	Queue->CompletionGoal = 0; 
	Queue->CompletionCount = 0; 
}

DWORD WINAPI
ThreadProc(LPVOID lpParameter)
{
	win32_thread_startup *Thread = (win32_thread_startup *)lpParameter;
	platform_work_queue *Queue = Thread->Queue;

	u32 TestTreadID = GetThreadID();
	Assert(TestTreadID == GetCurrentThreadId());

	if (Thread->OpenGLRC)
	{
		wglMakeCurrent(Thread->OpenGLDC, Thread->OpenGLRC);
	}

	for (;;)
	{
		if(Win32DoNextWorkQueueEntry(Queue))
		{
			WaitForSingleObjectEx(Queue->SemaphoreHandle, INFINITE, FALSE);
		}
	}	

	//return(0);
}

inline
PLATFORM_WORK_QUEUE_CALLBACK(DoWorkerWork)
{
	char Buffer[256];
	wsprintf(Buffer, "Thread %u: %s\n", GetCurrentThreadId(), (char *)Data);
	OutputDebugStringA(Buffer);
}

internal void
Win32MakeQueue(platform_work_queue *Queue, uint32 ThreadCount, win32_thread_startup *Startups)
{
	uint32 InitialCount = 0;

	Queue->CompletionGoal = 0;
	Queue->CompletionCount = 0; 
	
	Queue->NextEntryToWrite = 0;
	Queue->NextEntryToRead = 0;

	Queue->SemaphoreHandle = CreateSemaphoreEx(
		0,
		InitialCount,
		ThreadCount,
		0, 0, SEMAPHORE_ALL_ACCESS);

	for (uint32 ThreadIndex = 0;
		ThreadIndex < ThreadCount;
		++ThreadIndex)
	{
		win32_thread_startup *Startup = Startups + ThreadIndex;
		Startup->Queue = Queue;

		DWORD ThreadID;
		HANDLE ThreadHandle = CreateThread(0, 0, ThreadProc, Startup, 0, &ThreadID);
		CloseHandle(ThreadHandle);	
	}
}

struct win32_platform_file_handle
{
	HANDLE Win32Handle;
};

struct win32_platform_file_group
{
	HANDLE FindHandle;
	WIN32_FIND_DATAW FindData;
};

internal PLATFORM_GET_ALL_FILE_OF_TYPES_BEGIN(Win32GetAllFilesOfTypeBegin)
{
	platform_file_group Result = {};
	
	win32_platform_file_group *Win32FileGroup = 
		(win32_platform_file_group * )VirtualAlloc(
			0, sizeof(win32_platform_file_group),
			MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	Result.Platform = Win32FileGroup;

	wchar_t *WildCard = L"*.*";
	switch (Type)
	{
		case PlatformFileType_AssetFile:
		{
			WildCard = L"*.hha";
		} break;
		
		case PlatformFileType_SaveGameFile:
		{
			WildCard = L"*.hhs";
		} break;
	
		InvalidDefaultCase;
	}
	
	Result.FileCount = 0;

	WIN32_FIND_DATAW FindData;
	HANDLE FindHandle = FindFirstFileW(WildCard, &FindData);
	while (FindHandle != INVALID_HANDLE_VALUE)
	{
		++Result.FileCount;

		if (!FindNextFileW(FindHandle, &FindData))
		{
			break;
		}
	}

	if (FindHandle != INVALID_HANDLE_VALUE)
	{
		FindClose(FindHandle);
	}

	Win32FileGroup->FindHandle = FindFirstFileW(WildCard, &Win32FileGroup->FindData);

	return(Result);
}

internal PLATFORM_GET_ALL_FILE_OF_TYPES_END(Win32GetAllFilesOfTypeEnd)
{
	win32_platform_file_group *Win32FileGroup = (win32_platform_file_group *)FileGroup->Platform;
	if (Win32FileGroup)
	{
		if (Win32FileGroup->FindHandle != INVALID_HANDLE_VALUE)
		{
			FindClose(Win32FileGroup->FindHandle);
		}

		VirtualFree(Win32FileGroup, 0, MEM_RELEASE);
	}
}

internal PLATFORM_OPEN_NEXT_FILE(Win32OpenNextFile)
{
	win32_platform_file_group *Win32FileGroup = (win32_platform_file_group *)FileGroup->Platform;
	platform_file_handle Result = {};

	if (Win32FileGroup->FindHandle != INVALID_HANDLE_VALUE)
	{

		// TODO: If we want someday make an actual arena used be Win32
		win32_platform_file_handle *Win32Handle = (win32_platform_file_handle * )VirtualAlloc(
			0, sizeof(win32_platform_file_handle),
			MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		Result.Platform = (void *)Win32Handle;
	
		if (Win32Handle)
		{
			wchar_t *FileName = Win32FileGroup->FindData.cFileName;
			Win32Handle->Win32Handle = CreateFileW(FileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
			Result.NoErrors = (Win32Handle->Win32Handle != INVALID_HANDLE_VALUE);
		}

		if (!FindNextFileW(Win32FileGroup->FindHandle, &Win32FileGroup->FindData))
		{
			FindClose(Win32FileGroup->FindHandle);
			Win32FileGroup->FindHandle = INVALID_HANDLE_VALUE;
		}
	}
	
	return(Result);
}

internal PLATFORM_FILE_ERROR(Win32FileError)
{
#if HANDMADE_INTERNAL
	OutputDebugStringA("WIN32 FILE ERROR: ");
	OutputDebugStringA(Message);
	OutputDebugStringA("\n");
#endif
	Handle->NoErrors = false;
}

internal PLATFORM_READ_DATA_FROM_FILE(Win32ReadDataFromFile)
{
	if (PlatformNoFileErrors(Source))
	{
		win32_platform_file_handle *Handle = (win32_platform_file_handle *)Source->Platform;
		OVERLAPPED Overlapped = {};
		Overlapped.Offset = (u32)((Offset >> 0) & 0xFFFFFFFF);
		Overlapped.OffsetHigh = (u32)((Offset >> 32) & 0xFFFFFFFF);

		u32 FileSize32 = SafeTruncateUInt64(Size);

		DWORD BytesRead;
		if (ReadFile(Handle->Win32Handle, Dest, FileSize32, &BytesRead, &Overlapped) &&
			(FileSize32 == BytesRead))
		{
			// NOTE: File read succeded
		}
		else
		{
			Win32FileError(Source, "Read file failed");
		}
	}
}

/*
internal PLATFORM_FILE_ERROR(Win32FileError)
{
	CloseHandle(FileHandle);
}
*/

PLATFORM_ALLOCATE_MEMORY(Win32AllocateMemory)
{
	void *Result = VirtualAlloc(0, Size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	return(Result);
}

PLATFORM_DEALLOCATE_MEMORY(Win32DeallocateMemory)
{
	if (Memory)
	{
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

#if HANDMADE_INTERNAL
global_variable debug_table GlobalDebugTable_;
debug_table *GlobalDebugTable = &GlobalDebugTable_;
#endif

internal void
Win32FullRestart(char *SourceEXE, char *DestEXE, char *DeleteEXE)
{
	DeleteFile(DeleteEXE);
	if (MoveFile(DestEXE, DeleteEXE))
	{
		if (MoveFile(SourceEXE, DestEXE))
		{
			debug_executing_process Result = {};

			STARTUPINFO StartupInfo = {};
			StartupInfo.cb = sizeof(StartupInfo);

			PROCESS_INFORMATION ProcessInfo = {};
			if (CreateProcess(DestEXE,
				GetCommandLine(),
				0,
				0,
				FALSE,
				0,
				0,
				"D:\\handmade\\handmade\\data",
				&StartupInfo,
				&ProcessInfo))
			{
				CloseHandle(ProcessInfo.hProcess);
			}
			else
			{
				// TODO: Error!
			}
		}
	}
}

int CALLBACK 
WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR commandLine,
	int ShowCode)
{
	DEBUGSetEventRecording(true);
	
	win32_state Win32State = {};

	u32 ID = GetCurrentThreadId();

	LARGE_INTEGER PerCountFrequencyResult;
	QueryPerformanceFrequency(&PerCountFrequencyResult);
	GlobalPerfCountFrequency = PerCountFrequencyResult.QuadPart;

	Win32GetEXEFileName(&Win32State);

	char Win32EXEFullPath[WIN32_STATE_FILE_NAME_COUNT];
	Win32BuildEXEPathFileName(
		&Win32State, "win32_handmade.exe",
		sizeof(Win32EXEFullPath), Win32EXEFullPath);

	char TempWin32EXEFullPath[WIN32_STATE_FILE_NAME_COUNT];
	Win32BuildEXEPathFileName(
		&Win32State, "win32_handmade_temp.exe",
		sizeof(TempWin32EXEFullPath), TempWin32EXEFullPath);

	char DeleteWin32EXEFullPath[WIN32_STATE_FILE_NAME_COUNT];
	Win32BuildEXEPathFileName(
		&Win32State, "win32_handmade_old.exe",
		sizeof(DeleteWin32EXEFullPath), DeleteWin32EXEFullPath);

	char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
	Win32BuildEXEPathFileName(
		&Win32State, "handmade.dll",
		sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);

	char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
	Win32BuildEXEPathFileName(
		&Win32State, "handmade_temp.dll",
		sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);

	char GameCodeLockFullPath[WIN32_STATE_FILE_NAME_COUNT];
	Win32BuildEXEPathFileName(
		&Win32State, "lock.temp",
		sizeof(GameCodeLockFullPath), GameCodeLockFullPath);

	UINT DesiredShedulerMS = 1;
	bool32 SleepIsGranular = (timeBeginPeriod(DesiredShedulerMS) == TIMERR_NOERROR);

	Win32LoadXInput();

#if 1
	DEBUGGlobalShowCursor = true;
#endif
	WNDCLASSA WindowClass = {};

	// NOTE: 1080p display mode is  1920x1080 -> Half of that is 960x540
	Win32ResizeDIBSection(&GlobalBackbuffer, 1920, 1080);
	
	WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
	WindowClass.lpszClassName = "HandMadeHeroWindow";
	WindowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

	if (RegisterClass(&WindowClass))
	{
		HWND Window = 
			CreateWindowEx(
				0,
				WindowClass.lpszClassName,
				"Handmade Hero",
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				0,
				0,
				Instance,
				0);
		if (Window)
		{
			ToggleFullscreen(Window);
			HDC OpenGLDC = GetDC(Window);
			HGLRC OpenGLRC = Win32InitOpenGL(OpenGLDC);

			win32_thread_startup HighPriStartups[2] = {};
			platform_work_queue HighPriorityQueue = {};
			Win32MakeQueue(&HighPriorityQueue, ArrayCount(HighPriStartups), HighPriStartups);

			win32_thread_startup LowPriStartups[2] = {};
			LowPriStartups[0] = Win32ThreadStartupForGL(OpenGLDC, OpenGLRC);
			LowPriStartups[1] = Win32ThreadStartupForGL(OpenGLDC, OpenGLRC);
			platform_work_queue LowPriorityQueue = {};
			Win32MakeQueue(&LowPriorityQueue, ArrayCount(LowPriStartups), LowPriStartups);

			ReleaseDC(Window, OpenGLDC);

			win32_sound_output SoundOutput = {};

			// TODO: How do we reliably query on this on Windows
			int MonitorRefreshHz = 60;
			HDC RefreshDC = GetDC(Window);
			int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
			ReleaseDC(Window, RefreshDC);
			if (Win32RefreshRate > 1)
			{
				MonitorRefreshHz = Win32RefreshRate;
			}
			r32 GameUpdateHz = (r32)(MonitorRefreshHz / 2);
			r32 TargetSecondPerFrame = 1.0f / GameUpdateHz;

			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.BytesPerSample = sizeof(int16)*2;
			SoundOutput.SecondaryByfferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
			SoundOutput.SafetyBytes = (int)((real32)SoundOutput.SamplesPerSecond*(real32)SoundOutput.BytesPerSample / GameUpdateHz)/3.0f;
			Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryByfferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			GlobalRunning = true;

			umm CurrentSortMemorySize = Megabytes(1);
			void *SortMemory = Win32AllocateMemory(CurrentSortMemorySize);

			umm CurrentClipMemorySize = Megabytes(1);
			void *ClipRectMemory = Win32AllocateMemory(CurrentClipMemorySize);

			// TODO: Decide what out pushbuffer size is!
			u32 PushBufferSize = Megabytes(10);
			void *PushBuffer = Win32AllocateMemory(PushBufferSize);
#if 0
			while (GlobalRunning)
			{
				DWORD PlayCursor;
				DWORD WriteCursor;
				GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);

				char TextBuffer[256];
				_snprintf_s(
					TextBuffer, sizeof(TextBuffer), 
					"PC:%u WC:%u\n", PlayCursor, WriteCursor);
				OutputDebugStringA(TextBuffer);
			}
#endif

			// TODO: Pool with bitmap init
			// TODO: Remove MaxPossibleOverrun?
			u32 MaxPossibleOverrun = 2*4*sizeof(u16);
			int16 *Samples = (int16 *)VirtualAlloc(
				0, SoundOutput.SecondaryByfferSize + MaxPossibleOverrun, 
				MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
			LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
			LPVOID BaseAddress = 0;
#endif
			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(15);
			GameMemory.TransientStorageSize = Megabytes(256);
			GameMemory.DebugStorageSize = Megabytes(256);
#if HANDMADE_INTERNAL
			GameMemory.DebugTable = GlobalDebugTable;
#endif
			GameMemory.HighPriorityQueue = &HighPriorityQueue;
			GameMemory.LowPriorityQueue = &LowPriorityQueue;
			GameMemory.PlatformAPI.AddEntry = Win32AddEntry;
			GameMemory.PlatformAPI.CompleteAllWork = Win32CompleteAllWork;

			GameMemory.PlatformAPI.GetAllFilesOfTypeBegin = Win32GetAllFilesOfTypeBegin;
			GameMemory.PlatformAPI.GetAllFilesOfTypeEnd = Win32GetAllFilesOfTypeEnd;
			GameMemory.PlatformAPI.OpenNextFile = Win32OpenNextFile;
			GameMemory.PlatformAPI.ReadDataFromFile = Win32ReadDataFromFile;
			GameMemory.PlatformAPI.FileError = Win32FileError;

			GameMemory.PlatformAPI.AllocateMemory = Win32AllocateMemory;
			GameMemory.PlatformAPI.DeallocateMemory = Win32DeallocateMemory;

			GameMemory.PlatformAPI.AllocateTexture = AllocateTexture;
			GameMemory.PlatformAPI.DeallocateTexture = DeallocateTexture;

#if HANDMADE_INTERNAL
			GameMemory.PlatformAPI.DEBUGFreeFileMemory = DEBUGPlatformFreeFileMemory;
			GameMemory.PlatformAPI.DEBUGReadEntireFile = DEBUGPlatformReadEntireFile;
			GameMemory.PlatformAPI.DEBUGWriteEntireFile = DEBUGPlatformWriteEntireFile;
			GameMemory.PlatformAPI.DEBUGExecuteSystemCommand = DEBUGExecuteSystemCommand;
			GameMemory.PlatformAPI.DEBUGGetProcessState = DEBUGGetProcessState;
#endif
			Platform = GameMemory.PlatformAPI;
			// TODO: Handle memory footprints (USING SYSTEM METRICS)
			//
			// TODO: Use MEM_LARGE_PAGES and call adjust token
			// privileges when not on Windows Xp
			//
			// TODO: TransientStorage needs to be broken up 
			// into game transient and cache transient, and only
			// the former need be saved for state playback
			Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize + 
				GameMemory.DebugStorageSize;							
			Win32State.GameMemoryBlock = VirtualAlloc(
				0, (size_t)Win32State.TotalSize,
				MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
			GameMemory.TransientStorage = ((u8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);
			GameMemory.DebugStorage = ((u8 *)GameMemory.TransientStorage + GameMemory.TransientStorageSize);

			for (int ReplayIndex = 1;
				ReplayIndex < ArrayCount(Win32State.ReplayBuffers);
				++ReplayIndex)
			{
				win32_replay_buffer *ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];

				Win32GetInputFileLocation(&Win32State, false, ReplayIndex, sizeof(ReplayBuffer->FileName), ReplayBuffer->FileName);
		
				ReplayBuffer->FileHandle = 
					CreateFileA(ReplayBuffer->FileName, GENERIC_WRITE|GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);
				
				LARGE_INTEGER MaxSize;
				MaxSize.QuadPart = Win32State.TotalSize;
				ReplayBuffer->MemoryMap = CreateFileMapping(
					ReplayBuffer->FileHandle, 0, PAGE_READWRITE,
					MaxSize.HighPart, MaxSize.LowPart, 0);
				
				DWORD Error = GetLastError();

				ReplayBuffer->MemoryBlock = MapViewOfFile(
					ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, Win32State.TotalSize);
				if (ReplayBuffer->MemoryBlock)
				{

				}
				else
				{
					// TODO: Diagnostic
				}
			}
			
			if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
			{
				// TODO: Monitor XBox controller for being plugged in after the fact!
				u32 XBoxControllerPresent[XUSER_MAX_COUNT] = {};
				for (u32 ControllerIndex = 0;
					ControllerIndex < XUSER_MAX_COUNT;
					++ControllerIndex)
				{
					XBoxControllerPresent[ControllerIndex] = true;
				}

				game_input Input[2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];

				LARGE_INTEGER LastCounter = Win32GetWallClock();
				LARGE_INTEGER FlipWallClock = Win32GetWallClock();

				int DebugTimeMarketIndex = 0;
				win32_debug_time_marker DebugTimeMarkers[30] = {0};

				DWORD AudioLatencyBytes = 0;	
				real32 AudioLatencySeconds = 0;
				bool32 SoundIsValid = false;

				win32_game_code Game = Win32LoadGameCode(
					SourceGameCodeDLLFullPath,
					TempGameCodeDLLFullPath,
					GameCodeLockFullPath);

				ShowWindow(Window, SW_SHOW);
				while (GlobalRunning)
				{
					{DEBUG_DATA_BLOCK("Platform/Controls");
						DEBUG_B32(GlobalPause);
						DEBUG_B32(GlobalRenderingType);
					}
					
					//
					//
					//

					NewInput->dtForFrame = TargetSecondPerFrame;

					//
					//
					//

					BEGIN_BLOCK("Input Processed");
					
					// TODO: Zeroing macro
					// TODO: We can't zero everuthing because the up/down state will
					// be wrong!!!
					game_controller_input *OldKeyboardController = GetController(OldInput, 0);
					game_controller_input *NewKeyboardController = GetController(NewInput, 0);
					*NewKeyboardController = {};
					NewKeyboardController->IsConnected = true;
					for (int ButtonIndex = 0;
						ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
						++ButtonIndex)
					{
						NewKeyboardController->Buttons[ButtonIndex].EndedDown = 
							OldKeyboardController->Buttons[ButtonIndex].EndedDown;
					}
						

					{
						TIMED_BLOCK("Win32 Message Processing");
						Win32ProcessPendingMessages(&Win32State, NewKeyboardController);
					}
					
					if (!GlobalPause)
					{
						{
							TIMED_BLOCK("Mause Posiition");

							POINT MouseP;
							GetCursorPos(&MouseP);
							ScreenToClient(Window, &MouseP);
							NewInput->MouseX = (r32)MouseP.x;
							NewInput->MouseY = (r32)((GlobalBackbuffer.Height - 1) - MouseP.y);
							NewInput->MouseZ = 0; // TODO: Support mousewheel?

							NewInput->ShiftDown = (GetKeyState(VK_SHIFT) & (1 << 15));
							NewInput->AltDown = (GetKeyState(VK_MENU) & (1 << 15));
							NewInput->ControlDown = (GetKeyState(VK_CONTROL) & (1 << 15));
						}

						{
							TIMED_BLOCK("Keyboard Processing");
							DWORD WindButtonID[PlatformMouseBotton_Count] =
							{
								VK_LBUTTON,
								VK_MBUTTON,
								VK_RBUTTON,
								VK_XBUTTON1,
								VK_XBUTTON2,
							};
							for (u32 ButtonIndex = 0;
								ButtonIndex < PlatformMouseBotton_Count;
								++ButtonIndex)
							{
								NewInput->MouseButtons[ButtonIndex] = OldInput->MouseButtons[ButtonIndex];	
								NewInput->MouseButtons[ButtonIndex].HalfTransitionCount = 0;	
								Win32ProcessKeyboardMessage(&NewInput->MouseButtons[ButtonIndex],
									GetKeyState(WindButtonID[ButtonIndex]) & (1 << 15));
							}
						}	
						
						{
							TIMED_BLOCK("XBox Controllers");
							
							//TODO: should we pull this more frequenly
							DWORD MaxControllerCount = XUSER_MAX_COUNT;
							if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
							{
								MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
							}

							for(DWORD ControllerIndex = 0;
								ControllerIndex < MaxControllerCount;
								++ControllerIndex)
							{
								DWORD OurControllerIndex = ControllerIndex + 1;
								game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
								game_controller_input *NewController = GetController(NewInput, OurControllerIndex);
								
								XINPUT_STATE ControllerState;
								if (XBoxControllerPresent[ControllerIndex] &&
									XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
								{
									NewController->IsConnected = true;
									NewController->IsConnected = OldController->IsConnected;
									//NOTE: This controller is plugged in
									XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

									NewController->IsAnalog = true;
									NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, 
										XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
									NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, 
										XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

									if (NewController->StickAverageX != 0 || 
										NewController->StickAverageY != 0)
									{
										NewController->IsAnalog = true;
									}

									if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
									{
										NewController->StickAverageY = 1.0f;
										NewController->IsAnalog = true;
									}
									if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
									{
										NewController->StickAverageY = -1.0f;
										NewController->IsAnalog = true;
									}
									if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
									{
										NewController->StickAverageX = -1.0f;
										NewController->IsAnalog = true;
									}
									if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
									{
										NewController->StickAverageX = 1.0f;
										NewController->IsAnalog = true;
									}

									// TODO: Min/Max macros!!!
									// TODO: Collapse to single function
									real32 Threshold = 0.5f;
									Win32ProcessXinputDigitalButton(
										(NewController->StickAverageX < -Threshold) ? 1 : 0,
										&OldController->MoveLeft, 1, 
										&NewController->MoveLeft);
									Win32ProcessXinputDigitalButton(
										(NewController->StickAverageX < Threshold) ? 1 : 0,
										&OldController->MoveRight, 1, 
										&NewController->MoveRight);
									Win32ProcessXinputDigitalButton(
										(NewController->StickAverageX < -Threshold) ? 1 : 0,
										&OldController->MoveDown, 1, 
										&NewController->MoveDown);
									Win32ProcessXinputDigitalButton(
										(NewController->StickAverageX < Threshold) ? 1 : 0,
										&OldController->MoveUp, 1, 
										&NewController->MoveUp);

									Win32ProcessXinputDigitalButton(
										Pad->wButtons,
										&OldController->ActionDown, XINPUT_GAMEPAD_A, 
										&NewController->ActionDown);
									Win32ProcessXinputDigitalButton(
										Pad->wButtons,
										&OldController->ActionRight, XINPUT_GAMEPAD_B,
										&NewController->ActionRight);
									Win32ProcessXinputDigitalButton(
										Pad->wButtons,
										&OldController->ActionLeft, XINPUT_GAMEPAD_X,
										&NewController->ActionLeft);
									Win32ProcessXinputDigitalButton(
										Pad->wButtons,
										&OldController->ActionUp, XINPUT_GAMEPAD_Y,
										&NewController->ActionUp);
									Win32ProcessXinputDigitalButton(
										Pad->wButtons,
										&OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER,
										&NewController->LeftShoulder);
									Win32ProcessXinputDigitalButton(
										Pad->wButtons,
										&OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER,
										&NewController->RightShoulder);

									Win32ProcessXinputDigitalButton(
										Pad->wButtons,
										&OldController->Start, XINPUT_GAMEPAD_START,
										&NewController->Start);
									Win32ProcessXinputDigitalButton(
										Pad->wButtons,
										&OldController->Back, XINPUT_GAMEPAD_BACK,
										&NewController->Back);
								}
								else
								{
									//NOTE: The controller is not available
									NewController->IsConnected = false;
									XBoxControllerPresent[ControllerIndex] = false;
								}
							}
						}
					}

					END_BLOCK();
					//
					//
					//

					BEGIN_BLOCK("Game Update");

					game_render_commands RenderCommands = RenderCommandsStruct(
						PushBufferSize, PushBuffer, 
						(u32)GlobalBackbuffer.Width,
						(u32)GlobalBackbuffer.Height);

					game_offscreen_buffer Buffer = {};
					Buffer.Memory = GlobalBackbuffer.Memory;
					Buffer.Height = GlobalBackbuffer.Height;
					Buffer.Width = GlobalBackbuffer.Width;
					Buffer.Pitch = GlobalBackbuffer.Pitch;
					if (!GlobalPause)
					{
						if (Win32State.InputRecordingIndex)
						{
							Win32RecordInput(&Win32State, NewInput);
						}

						if (Win32State.InputPlayingIndex)
						{
							game_input Temp = *NewInput;
							Win32PlayBackInput(&Win32State, NewInput);
							for (u32 MouseBottonIndex = 0;
								MouseBottonIndex < PlatformMouseBotton_Count;
								++MouseBottonIndex)
							{
								NewInput->MouseButtons[MouseBottonIndex] = Temp.MouseButtons[MouseBottonIndex];
							}
							NewInput->MouseX = Temp.MouseX;
							NewInput->MouseY = Temp.MouseY;
							NewInput->MouseZ = Temp.MouseZ;
						}

						if (Game.UpdateAndRender)
						{
							Game.UpdateAndRender(&GameMemory, NewInput, &RenderCommands);
							if (NewInput->QuitRequested)
							{
								GlobalRunning = false;
							}
						}
					}

					END_BLOCK();

					//
					//
					//
					
					BEGIN_BLOCK("Audio Updated");
					
					if (!GlobalPause)
					{
						LARGE_INTEGER AudioWallClock = Win32GetWallClock();
						real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

						DWORD PlayCursor;
						DWORD WriteCursor;
						if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
						{
							/* NOTE:
								Here is how sound output computation works.
							
								We define a safety value that is the number
								of samples we think our game update looop
								may vary by (let's say up to 2ms)

								When we wake up to write audio, will look
								and see what the play cursor position is and we
								will forecast ahead where we think the play
								cursor will be on the next farme boundary.

								We will then look to see if the write cursor is
								before that by at least our safety value. If
								it is, the target fill postion is that frame
								boundary plus one frame. This gives us perfect
								audio sync in the case of a card that has low
								enought latency.

								If the write cursor is _after_ that safety
								margin, the assume we can never sync the
								audio perfectly, so we will write one frame's
								worth of audio plus the safety margins's worth
								of quard samples
							*/

							if (!SoundIsValid)
							{
								SoundOutput.RunningSampleIndex = WriteCursor  / SoundOutput.BytesPerSample;
								SoundIsValid = true;
							}

							DWORD ByteToLock = ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % 
								SoundOutput.SecondaryByfferSize);

							DWORD ExpectedSoundBytesPerFrame = 
								(int)((real32)(SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample) / GameUpdateHz);
							real32 SecondsLeftUntilFlip = (TargetSecondPerFrame - FromBeginToAudioSeconds);
							DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip/TargetSecondPerFrame)*(real32)ExpectedSoundBytesPerFrame);
							
							DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;

							DWORD SafeWeriteCursor = WriteCursor;
							if (SafeWeriteCursor < PlayCursor)
							{
								SafeWeriteCursor += SoundOutput.SecondaryByfferSize;
							}
							Assert(SafeWeriteCursor >= PlayCursor)
							SafeWeriteCursor += SoundOutput.SafetyBytes;

							bool32 AudioCardIsLowLatent = (SafeWeriteCursor < ExpectedFrameBoundaryByte);
							
							DWORD TargetCursor = 0;
							if (AudioCardIsLowLatent)
							{
								TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame); 
							}
							else
							{
								TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes);
							}
							TargetCursor = TargetCursor % SoundOutput.SecondaryByfferSize;

							DWORD BytesToWrite = 0;
							if(ByteToLock > TargetCursor)
							{
								BytesToWrite = (SoundOutput.SecondaryByfferSize - ByteToLock);
								BytesToWrite += TargetCursor;
							}
							else
							{
								BytesToWrite = TargetCursor - ByteToLock;
							}

							game_sound_output_buffer SoundBuffer = {};
							SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
							SoundBuffer.SampleCount = Align8(BytesToWrite / SoundOutput.BytesPerSample);
							BytesToWrite = SoundBuffer.SampleCount*SoundOutput.BytesPerSample;
							SoundBuffer.Samples = Samples;
							if (Game.GetSoundSamples)
							{
								Game.GetSoundSamples(&GameMemory, &SoundBuffer);
							}
#if HANDMADE_INTERNAL
							win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarketIndex];
							Marker->OutputPlayCursor = PlayCursor;
							Marker->OutputWriteCursor = WriteCursor;
							Marker->OutputLocation = ByteToLock;
							Marker->OutputByteCount = BytesToWrite;
							Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;

							DWORD UnwrappedWriteCursor = WriteCursor;
							if (UnwrappedWriteCursor < PlayCursor)
							{
								UnwrappedWriteCursor += SoundOutput.SecondaryByfferSize;
							}
							AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;	
							AudioLatencySeconds = 
								(((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) /
								(real32)SoundOutput.SamplesPerSecond);
#if 0
							char TextBuffer[256];
							_snprintf_s(
								TextBuffer, sizeof(TextBuffer), 
								"BTL:%u TC:%u BTW:%u - PC:%u WC:%u DELTA:%u (%fs)\n", 
								ByteToLock, TargetCursor, BytesToWrite,
								PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
							OutputDebugStringA(TextBuffer);
#endif
#endif
							Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
						}
						else
						{
							SoundIsValid = false;
						}
					}

					END_BLOCK();

					//
					//
					//
	
#if HANDMADE_INTERNAL
					BEGIN_BLOCK("Debug Collating")

					FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
					b32 ExecutableNeedsToBeReloaded =
						(CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0);
#if 0
					FILETIME NewEXETime = Win32GetLastWriteTime(Win32EXEFullPath);
					FILETIME OldEXETime = Win32GetLastWriteTime(TempWin32EXEFullPath);
					if (Win32TimeIsValid(NewEXETime))
					{
						b32 Win32NeedsToBeReloaded =
							(CompareFileTime(&NewEXETime, &OldEXETime) != 0);
						// TODO: Compare file contents here
						if (Win32NeedsToBeReloaded)
						{
							Win32FullRestart(TempWin32EXEFullPath, Win32EXEFullPath, DeleteWin32EXEFullPath);
						}
					}
#endif
					GameMemory.ExecutableReloaded = false;

					if (ExecutableNeedsToBeReloaded)
					{
						Win32CompleteAllWork(&HighPriorityQueue);
						Win32CompleteAllWork(&LowPriorityQueue);
						DEBUGSetEventRecording(false);
					}

					if (Game.DEBUGFrameEnd)
					{
						Game.DEBUGFrameEnd(&GameMemory, NewInput, &RenderCommands);
					}
					else
					{
						// NOTE: if for some reason the game didn't load,
						// make sure we clear the debug event array so it doesn't
						// pile up on itself
						GlobalDebugTable_.EventArrayIndex_EventIndex = 0;
					}
	
					if (ExecutableNeedsToBeReloaded)
					{
						Win32UnloadGameCode(&Game);

						for (u32 LoadTryIndex = 0;
							Game.IsValid && LoadTryIndex < 100;
							++LoadTryIndex)
						{
							Game = Win32LoadGameCode(
								SourceGameCodeDLLFullPath,
								TempGameCodeDLLFullPath,
								GameCodeLockFullPath);
							Sleep(100);
						}

						GameMemory.ExecutableReloaded = true;
						DEBUGSetEventRecording(Game.IsValid);
					}


					END_BLOCK();
#endif
			
					//
					//
					//
				
					//TODO: Leave this off until we have actual vblank support?
#if 0
					BEGIN_BLOCK(FrameWaitComplete);

					if (!GlobalPause)
					{
						LARGE_INTEGER WorkCounter = Win32GetWallClock();
						real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

						real32 SecondsElapsedForFrame = WorkSecondsElapsed;
						if (SecondsElapsedForFrame < TargetSecondPerFrame)
						{	
							if (SleepIsGranular)
							{
								DWORD SleepMS = (DWORD)(1000.f * (TargetSecondPerFrame - SecondsElapsedForFrame)); 
								if (SleepMS > 0)
								{
									Sleep(SleepMS);
								}
							}

							real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());

							if(TestSecondsElapsedForFrame < TargetSecondPerFrame)
							{
								//TODO: LOG MISSED SLIP HERE
							}

							while (SecondsElapsedForFrame < TargetSecondPerFrame)
							{
								SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
							}
						}
						else
						{
							// TODO: MISSED FRAME RATE
							// TODO: Logging
						}
					}

					END_BLOCK(FrameWaitComplete);
#endif					
					//
					//
					//

					BEGIN_BLOCK("Frame Display");

					umm NeededSortMemorySize = RenderCommands.PushBufferElementCount * sizeof(sort_entry);
					if (CurrentSortMemorySize < NeededSortMemorySize)
					{
						Win32DeallocateMemory(SortMemory);
						CurrentSortMemorySize = NeededSortMemorySize;
						SortMemory = Win32AllocateMemory(CurrentSortMemorySize);
					}

					// TODO: Collapse this with above!
					umm NeededClipMemorySize = RenderCommands.PushBufferElementCount * sizeof(render_entry_cliprect);
					if (CurrentClipMemorySize < NeededClipMemorySize)
					{
						Win32DeallocateMemory(SortMemory);
						CurrentClipMemorySize = NeededClipMemorySize;
						SortMemory = Win32AllocateMemory(CurrentClipMemorySize);
					}

					win32_window_dimension Dimension = Win32GetWindowDimension(Window);
					HDC DeviceContext = GetDC(Window);
					Win32DisplayBufferInWindow(&HighPriorityQueue, &RenderCommands, DeviceContext,
						Dimension.Width, Dimension.Height, SortMemory, ClipRectMemory);
					ReleaseDC(Window, DeviceContext);

					FlipWallClock = Win32GetWallClock();

					game_input *Temp = NewInput;
					NewInput = OldInput;
					OldInput = Temp;

					END_BLOCK();

					LARGE_INTEGER EndCounter = Win32GetWallClock();
					FRAME_MARKER(Win32GetSecondsElapsed(LastCounter, EndCounter));
					LastCounter = EndCounter;
				}
			}
			else
			{
				//TODO: Loggin
			}
		}
		else
		{
			//TODO: Loggin
		}
	}
	else
	{
		//TODO: Loggin
	}

	return 0;
}