
/*
	- Save game locations
	- Getting a handle to out own executable file
	- Assert loading path
	- Threading (launch a thread)
	- Raw Iput (support for multiple keyboards)
	- Sleep/timeBeginPeriod
	- ClipCursor (for mutimonitor support)
	- FullScreen Support
	- WM_SETCURSOR (control cursor visibility)
	- QuerryCancelAutoplay
	- WM_ACTIVATEAPP (for when we are not the active application)
	- Blit speed improvements (BitBlt)
	- Hardware acceleration (OpenGl or Direct3D or BOTH)
	- GetKeyboardLayout (for French keyboards, internation WASD support)
*/

// TODO:implement sine ourselves
#include <math.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32_t bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

#include "handmade.h"
#include "handmade.cpp"

#include <windows.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

global_variable bool GLobalRunning;
global_variable win32_offscreen_buffer GlobalBackBaffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerCountFrequency;

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

internal debug_read_file_result
DEBUGPlatformReadEntireFile(char *Filename)
{
	debug_read_file_result Result = {};

	HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize;
		if (GetFileSizeEx(FileHandle, &FileSize))
		{
			uint32 FileSize32 = SafeTruncateUint64(FileSize.QuadPart);
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

internal void 
DEBUGPlatformFreeFileMemory(void *Memory)
{
	if (Memory)
	{
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

internal bool32
DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory)
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
			BufferDecsription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
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
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = (Buffer->Width * Buffer->Height) * BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	
	Buffer->Pitch = Width*BytesPerPixel;
}


internal void
Win32DisplayBufferInWindow(
	win32_offscreen_buffer *Buffer,
	HDC DeviceContext, int WindowWidth, int WindowHeight)
{
	//TODO: Aspect ratio corection
	StretchDIBits(
		DeviceContext,
		0, 0, WindowWidth, WindowHeight,
		0, 0, Buffer->Width, Buffer->Height,
		Buffer->Memory, 
		&Buffer->Info,
		DIB_RGB_COLORS, SRCCOPY
	);
}

LRESULT CALLBACK 
Win32MainWindowCallback(
	HWND Window,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam)
{
	LRESULT Result = 0;

	switch(Message)
	{
		case WM_SIZE:
		{
		} break;
		case WM_CLOSE:
		{
			GLobalRunning = false;
		} break;
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;
		case WM_DESTROY:
		{
			GLobalRunning = false;
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
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Height = Paint.rcPaint.bottom - Y;
			int Width = Paint.rcPaint.right - X;

			win32_window_dimension Dimension = Win32GetWindowDimension(Window);

			Win32DisplayBufferInWindow(
				&GlobalBackBaffer, DeviceContext,
				Dimension.Width, Dimension.Height);
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
	Assert(NewState->EndedDown != IsDown);
	NewState->EndedDown = IsDown;
	++NewState->HalfTransitionCount;
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

internal void
Win32ProcessPendingMessages(game_controller_input *KeyboardController)
{
	MSG Message;
	while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
	{
		switch(Message.message)
		{
			case WM_QUIT:
			{
				GLobalRunning = false;
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
						Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
					}
					else if (VKCode == VK_SPACE)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
					}
				}

				bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
				if ((VKCode == VK_F4) && AltKeyWasDown)
				{
					GLobalRunning = false;
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

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	real32 Result = (((real32)(End.QuadPart - Start.QuadPart)) / (real32)GlobalPerCountFrequency);

	return(Result);
}

internal void
Win32DebugDrawVertical(
	win32_offscreen_buffer *GlobalBackBaffer, 
	int X, int Top, int Bottom, uint32 Color)
{
	uint8 *Pixel = 
		(uint8 *)GlobalBackBaffer->Memory + 
		X*GlobalBackBaffer->BytesPerPixel +
		Top*GlobalBackBaffer->Pitch;

	for (int Y = Top;
		Y < Bottom;
		++Y)
	{
		*(uint32 *)Pixel = Color;
		Pixel +=GlobalBackBaffer->Pitch;
	}
}

internal void
Win32DrawSoundBufferMarker(
	win32_offscreen_buffer *GlobalBackBaffer, 
	win32_sound_output *SoundOutput,
	real32 C, int PadX, int Top, int Bottom,
	DWORD Value, uint32 Color)
{
	Assert(Value < SoundOutput->SecondaryByfferSize);
	real32 XReal32 = (C * (real32)Value);
	int X = PadX  + (int)XReal32;
	Win32DebugDrawVertical(GlobalBackBaffer, X, Top, Bottom, Color);
}

internal void
Win32DebugSyncDisplay(
	win32_offscreen_buffer *BackBaffer, 
	int MarkerCount, win32_debug_time_marker *Markers,
	win32_sound_output *SoundOutput, real32 TargetSecondPerFrame)
{
	int PadX = 16;
	int PadY = 16;

	int Top = PadY;
	int Bottom = BackBaffer->Height - PadY;

	real32 C = (real32)(BackBaffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryByfferSize;
	for (int MarkerIndex = 0;
		MarkerIndex < MarkerCount;
		++MarkerIndex)
	{
		win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
		Win32DrawSoundBufferMarker(BackBaffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->PlayCursor, 0xFFFFFFFF);
		Win32DrawSoundBufferMarker(BackBaffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->WriteCursor, 0xFFFF0000);
	}
}

int CALLBACK 
WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR commandLine,
	int ShowCode)
{
	LARGE_INTEGER PerCountFrequencyResult;
	QueryPerformanceFrequency(&PerCountFrequencyResult);
	GlobalPerCountFrequency = PerCountFrequencyResult.QuadPart;

	UINT DesiredShedulerMS = 1;
	bool32 SleepIsGranular = (timeBeginPeriod(DesiredShedulerMS) == TIMERR_NOERROR);

	Win32LoadXInput();
	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackBaffer, 1280, 720);
	
	WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "HandMadeHeroWindow";

	// TODO: How do we reliably query on this on Windows
#define FramesOfAudioLatence 4
#define MonitorRefreshHz  60
#define GameUpdateHz (MonitorRefreshHz / 2)
	real32 TargetSecondPerFrame = 1.0f / (real32)GameUpdateHz;

	if (RegisterClass(&WindowClass))
	{
		HWND Window = 
			CreateWindowEx(
				0,
				WindowClass.lpszClassName,
				"HandmadeHero",
				WS_OVERLAPPEDWINDOW|WS_VISIBLE,
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
			HDC DeviceContext = GetDC(Window);
			win32_sound_output SoundOutput = {};

			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.BytesPerSample = sizeof(int16)*2;
			SoundOutput.SecondaryByfferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
			SoundOutput.LatencySampleCount = FramesOfAudioLatence*(SoundOutput.SamplesPerSecond / GameUpdateHz);
			Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryByfferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			GLobalRunning = true;

			// TODO: Pool with bitmap init
			int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryByfferSize, MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
			LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
			LPVOID BaseAddress = 0;
#endif
			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(64);
			GameMemory.TransientStorageSize = Megabytes(64);

			// TODO: Handle memory footprints
			uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
											//BaseAddress
			GameMemory.PermanentStorage = VirtualAlloc(0, (size_t)TotalSize, MEM_COMMIT, PAGE_READWRITE);
			GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

			LARGE_INTEGER LastCounter = Win32GetWallClock();
;
			if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
			{
				game_input Input[2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];

				int DebugTimeMarketIntex = 0;
				win32_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {0};

				DWORD LastPlayCursor = 0;
				bool32 SoundIsValid = false;

				int64 LastCycleCount = __rdtsc();
				while (GLobalRunning)
				{
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

					Win32ProcessPendingMessages(NewKeyboardController);

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
						if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
						{
							NewController->IsConnected = true;
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
						}
					}

					// NOTE: Compute how mush sound to write and where
					DWORD ByteToLock = 0;
					DWORD TargetCursor = 0;
					DWORD BytesToWrite = 0;
					if (SoundIsValid)
					{
						ByteToLock = ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % 
							SoundOutput.SecondaryByfferSize);
							
						TargetCursor = 
							((LastPlayCursor + 
							(SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample))) % 
							SoundOutput.SecondaryByfferSize; 

						if(ByteToLock > TargetCursor)
						{
							BytesToWrite = (SoundOutput.SecondaryByfferSize - ByteToLock);
							BytesToWrite += TargetCursor;
						}
						else
						{
							BytesToWrite = TargetCursor - ByteToLock;
						}
					}
					
					game_sound_output_buffer SoundBuffer = {};
					SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
					SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
					SoundBuffer.Samples = Samples;

					game_offscreen_buffer Buffer = {};
					Buffer.Memory = GlobalBackBaffer.Memory;
					Buffer.Height = GlobalBackBaffer.Height;
					Buffer.Width = GlobalBackBaffer.Width;
					Buffer.Pitch = GlobalBackBaffer.Pitch;
					GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);

					if (SoundIsValid)
					{
#if HANDMADE_INTERNAL
						DWORD PlayCursor;
						DWORD WriteCursor;
						GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
						char TextBuffer[256];
						_snprintf_s(
							TextBuffer, sizeof(TextBuffer), 
							"LPC:%u BTL:%u TC:%u BTW:%u - PC:%u WC:%u\n", 
							LastPlayCursor, ByteToLock, TargetCursor, BytesToWrite,
							PlayCursor, WriteCursor);
						OutputDebugStringA(TextBuffer);
#endif
						Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
					}

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

						//Assert(TestSecondsElapsedForFrame < TargetSecondPerFrame);
						
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

					LARGE_INTEGER EndCounter = Win32GetWallClock();
					real32 MSPerFrame = 1000.f*Win32GetSecondsElapsed(LastCounter, EndCounter);
					LastCounter = EndCounter;

					win32_window_dimension Dimension = Win32GetWindowDimension(Window);
#if HANDMADE_INTERNAL
					Win32DebugSyncDisplay(&GlobalBackBaffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers, &SoundOutput, TargetSecondPerFrame);
#endif
					Win32DisplayBufferInWindow(
						&GlobalBackBaffer, DeviceContext,
						Dimension.Width, Dimension.Height);

					DWORD PlayCursor2;
					DWORD WriteCursor2;
					if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor2, &WriteCursor2) == DS_OK)
					{
						LastPlayCursor = PlayCursor2;
						if (!SoundIsValid)
						{
							SoundOutput.RunningSampleIndex = WriteCursor2  / SoundOutput.BytesPerSample;
							SoundIsValid = true;
						}
					}
					else
					{
						SoundIsValid = false;
					}

#if HANDMADE_INTERNAL
					{
						win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarketIntex++];
						if (DebugTimeMarketIntex > ArrayCount(DebugTimeMarkers))
						{
							DebugTimeMarketIntex = 0;
						}
						Marker->PlayCursor = PlayCursor2;
						Marker->WriteCursor = WriteCursor2;
					}
#endif

					game_input *Temp = NewInput;
					NewInput = OldInput;
					OldInput = Temp;

					uint64 EndCycleCount = __rdtsc();
					uint64 CycleElapsed = EndCycleCount - LastCycleCount;
					LastCycleCount = EndCycleCount;

					real32 FPS = 0;
					real32 MCPF = ((real32)CycleElapsed / (1000.0f * 1000.0f));
#if 1
					char FPSBuffer[256];
					_snprintf_s(FPSBuffer, sizeof(FPSBuffer), 
						"%.02fms/f, %.02ff/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF);
					OutputDebugStringA(FPSBuffer);
#endif
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