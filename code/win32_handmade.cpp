
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
			BufferDecsription.dwFlags = 0;
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
	LPARAM LParam
)
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
					} 
					else if (VKCode == 'A')
					{
					}
					else if (VKCode == 'S')
					{
					}
					else if (VKCode == 'D')
					{
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
						Win32ProcessKeyboardMessage(&KeyboardController->Up, IsDown);
					}
					else if (VKCode == VK_LEFT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Left, IsDown);
					}
					else if (VKCode == VK_DOWN)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Down, IsDown);
					}
					else if (VKCode == VK_RIGHT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Right, IsDown);
					}
					else if (VKCode == VK_ESCAPE)
					{
						GLobalRunning = false;
					}
					else if (VKCode == VK_SPACE)
					{
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

int CALLBACK 
WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR commandLine,
	int ShowCode
)
{
	LARGE_INTEGER PerCountFrequencyResult;
	QueryPerformanceFrequency(&PerCountFrequencyResult);
	int64 PerCountFrequency = PerCountFrequencyResult.QuadPart;

	Win32LoadXInput();
	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackBaffer, 1280, 720);
	
	WindowClass.style = CS_HREDRAW|CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "HandMadeHeroWindow";

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

			// NOTE: Graphics test
			int XOffset = 0;
			int YOffset = 0;

			win32_sound_output SoundOutput = {};

			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.BytesPerSample = sizeof(int16)*2;
			SoundOutput.SecondaryByfferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
			SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
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

			LARGE_INTEGER LastCounter;
			QueryPerformanceCounter(&LastCounter);
			int64 LastCycleCount = __rdtsc();

			if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
			{
				game_input Input[2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];

				while (GLobalRunning)
				{
					game_controller_input *KeyboardController = &NewInput->Controllers[0];
					// TODO: Zeroing macro
					game_controller_input ZeroController = {};
					*KeyboardController = ZeroController;

					Win32ProcessPendingMessages(KeyboardController);

					//TODO: should we pull this more frequenly
					DWORD MaxControllerCount = XUSER_MAX_COUNT;
					if (MaxControllerCount > ArrayCounter(NewInput->Controllers))
					{
						MaxControllerCount = ArrayCounter(NewInput->Controllers);
					}
					for(DWORD ControllerIndex = 0;
						ControllerIndex < MaxControllerCount;
						++ControllerIndex)
					{
						game_controller_input *OldController = &OldInput->Controllers[ControllerIndex];
						game_controller_input *NewController = &NewInput->Controllers[ControllerIndex];
						XINPUT_STATE ControllerState;
						if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
						{
							//NOTE: This controller is plugged in
							XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

							bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
							bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
							bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
							bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

							NewController->IsAnalog = true;
							NewController->StartX = OldController->EndX;
							NewController->StartY = OldController->EndY;

							// TODO: Min/Max macros!!!
							// TODO: Collapse to single function
							real32 X;
							if (Pad->sThumbLX < 0)
							{
								X = (real32)Pad->sThumbLX / 32768.0f;
							}
							else
							{
								X = (real32)Pad->sThumbLX / 32767.0f;
							}
							NewController->MinX = NewController->MaxX = NewController->EndX = X;

							real32 Y;
							if (Pad->sThumbLX < 0)
							{
								Y = (real32)Pad->sThumbLY / 32768.0f;
							}
							else
							{
								Y = (real32)Pad->sThumbLY / 32767.0f;
							}
							NewController->MinY = NewController->MaxY = NewController->EndY = Y;

							Win32ProcessXinputDigitalButton(Pad->wButtons,
															&OldController->Down, XINPUT_GAMEPAD_A,
															&NewController->Down);
							Win32ProcessXinputDigitalButton(Pad->wButtons,
															&OldController->Right, XINPUT_GAMEPAD_B,
															&NewController->Right);
							Win32ProcessXinputDigitalButton(Pad->wButtons,
															&OldController->Left, XINPUT_GAMEPAD_X,
															&NewController->Left);
							Win32ProcessXinputDigitalButton(Pad->wButtons,
															&OldController->Up, XINPUT_GAMEPAD_Y,
															&NewController->Up);
							Win32ProcessXinputDigitalButton(Pad->wButtons,
															&OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER,
															&NewController->LeftShoulder);
							Win32ProcessXinputDigitalButton(Pad->wButtons,
															&OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER,
															&NewController->RightShoulder);
							
							//bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
							//bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						}
						else
						{
							//NOTE: The controller is not available
						}
					}

					DWORD PlayCursor = 0;
					DWORD WriteCursor = 0;
					DWORD ByteToLock = 0;
					DWORD TargetCursor = 0;
					DWORD BytesToWrite = 0;
					bool32 SoundIsValid = false;
					// TODO: Tighten up sound logic so that we know where we shold be
					// writing to and can anticipate the time spent in the game update
					if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
					{
						ByteToLock = (SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % 
							SoundOutput.SecondaryByfferSize;
							
						TargetCursor = 
							((PlayCursor + 
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

						SoundIsValid = true;
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

					// NOTE: DirectSound output test
					if (SoundIsValid)
					{

						Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
					}

					win32_window_dimension Dimension = Win32GetWindowDimension(Window);
					Win32DisplayBufferInWindow(
						&GlobalBackBaffer, DeviceContext,
						Dimension.Width, Dimension.Height);

					int64 EndCycleCount = __rdtsc();

					LARGE_INTEGER EndCounter;
					QueryPerformanceCounter(&EndCounter);

					int64 CycleElapsed = EndCycleCount - LastCycleCount;
					int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
					real32 MSPerFrame = (real32)((1000*(real32)CounterElapsed) / (real32)PerCountFrequency);
					real32 FPS = (real32)PerCountFrequency / (real32)CounterElapsed;
					real32 MCPF = ((real32)CycleElapsed / (1000 * 1000));
	#if 0
					char Buffer[256];
					sprintf(Buffer, "%.02fms/f, %.02ff/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF);
					OutputDebugStringA(Buffer);
	#endif
					LastCounter = EndCounter;
					LastCycleCount = EndCycleCount;

					game_input *Temp = NewInput;
					NewInput = OldInput;
					OldInput = Temp;
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