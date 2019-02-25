
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

#include <windows.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

global_variable bool GLobalRunning;
global_variable bool GLobalPause;
global_variable win32_offscreen_buffer GlobalBackBaffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerCountFrequency;
global_variable bool32 DEBUGGlobalShowCursor;
global_variable WINDOWPLACEMENT GlobalWindowPosition = {sizeof(GlobalWindowPosition)};

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

			Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
		}

	}

	if (!Result.IsValid)
	{
		Result.UpdateAndRender = 0;
		Result.GetSoundSamples = 0;
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
	win32_offscreen_buffer *Buffer,
	HDC DeviceContext, int WindowWidth, int WindowHeight)
{
	// TODO: Centering / black bars?
	if ((WindowWidth >= Buffer->Width*2) &&
		(WindowHeight >= Buffer->Height*2))
	{
		StretchDIBits(
			DeviceContext,
			0, 0, 2*Buffer->Width, 2*Buffer->Height,
			0, 0, Buffer->Width, Buffer->Height,
			Buffer->Memory, 
			&Buffer->Info,
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
			OffsetX, OffsetY, Buffer->Width, Buffer->Height,
			0, 0, Buffer->Width, Buffer->Height,
			Buffer->Memory, 
			&Buffer->Info,
			DIB_RGB_COLORS, SRCCOPY);
	}
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

internal int
StringLength(char *String)
{
	int Count = 0;

	while (*String++)
	{
		++Count;
	}
	return(Count);
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
							GLobalPause = !GLobalPause;
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
						GLobalRunning = false;
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

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	real32 Result = (((real32)(End.QuadPart - Start.QuadPart)) / (real32)GlobalPerCountFrequency);

	return(Result);
}

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
struct platform_work_queue_entry
{
	platform_work_queue_callback *Callback;
	void *Data;
};

struct platform_work_queue
{
	uint32 volatile CompletionGoal;
	uint32 volatile CompletionCount; 
	
	uint32 volatile NextEntryToWrite;
	uint32 volatile NextEntryToRead;
	HANDLE SemaphoreHandle;
	
	platform_work_queue_entry Entries[256];
};

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
	platform_work_queue *Queue = (platform_work_queue *)lpParameter;

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
Win32MakeQueue(platform_work_queue *Queue, uint32 ThreadCount)
{
	uint32 InitialCount = 0;

	Queue->CompletionGoal = 0;
	Queue->CompletionCount = 0; 
	
	Queue->NextEntryToWrite = 0;
	Queue->NextEntryToRead = 0;

	Queue->SemaphoreHandle = CreateSemaphoreEx(0, InitialCount, ThreadCount, 0, 0, SEMAPHORE_ALL_ACCESS);

	for (uint32 ThreadIndex = 0;
		ThreadIndex < ThreadCount;
		++ThreadIndex)
	{
		DWORD ThreadID;
		HANDLE ThreadHandle = CreateThread(0, 0, ThreadProc, Queue, 0, &ThreadID);
		CloseHandle(ThreadHandle);	
	}
}

int CALLBACK 
WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR commandLine,
	int ShowCode)
{
	win32_state Win32State = {};

	platform_work_queue HighPriorityQueue = {};
	Win32MakeQueue(&HighPriorityQueue, 2);

	platform_work_queue LowPriorityQueue = {};
	Win32MakeQueue(&LowPriorityQueue, 2);

	LARGE_INTEGER PerCountFrequencyResult;
	QueryPerformanceFrequency(&PerCountFrequencyResult);
	GlobalPerCountFrequency = PerCountFrequencyResult.QuadPart;

	Win32GetEXEFileName(&Win32State);

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
	Win32ResizeDIBSection(&GlobalBackBaffer, 960, 540);
	
	WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
	WindowClass.lpszClassName = "HandMadeHeroWindow";

	if (RegisterClass(&WindowClass))
	{
		HWND Window = 
			CreateWindowEx(
				0,
				WindowClass.lpszClassName,
				"Handmade Hero",
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
			real32 GameUpdateHz = (real32)(MonitorRefreshHz / 2);
			real32 TargetSecondPerFrame = 1.0f / GameUpdateHz;

			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.BytesPerSample = sizeof(int16)*2;
			SoundOutput.SecondaryByfferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
			SoundOutput.SafetyBytes = (int)((real32)SoundOutput.SamplesPerSecond*(real32)SoundOutput.BytesPerSample / GameUpdateHz)/3.0f;
			Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryByfferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			GLobalRunning = true;
#if 0
			while (GLobalRunning)
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
			int16 *Samples = (int16 *)VirtualAlloc(
				0, SoundOutput.SecondaryByfferSize, 
				MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
			LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
			LPVOID BaseAddress = 0;
#endif
			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(15);
			GameMemory.TransientStorageSize = Megabytes(50);
			GameMemory.HighPriorityQueue = &HighPriorityQueue;
			GameMemory.LowPriorityQueue = &LowPriorityQueue;
			GameMemory.PlatformAddEntry = Win32AddEntry;
			GameMemory.PlatformCompleteAllWork = Win32CompleteAllWork;
			GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
			GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
			GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

			// TODO: Handle memory footprints (USING SYSTEM METRICS)
			//
			// TODO: Use MEM_LARGE_PAGES and call adjust token
			// privileges when not on Windows Xp
			//
			// TODO: TransientStorage needs to be broken up 
			// into game transient and cache transient, and only
			// the former need be saved for state playback
			Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;							
			Win32State.GameMemoryBlock = VirtualAlloc(
				0, (size_t)Win32State.TotalSize,
				MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
			GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

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
				game_input Input[2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];

				LARGE_INTEGER LastCounter = Win32GetWallClock();
				LARGE_INTEGER FlipWallClock = Win32GetWallClock();

				int DebugTimeMarketIntex = 0;
				win32_debug_time_marker DebugTimeMarkers[30] = {0};

				bool32 SoundIsValid = false;
				DWORD AudioLatencyBytes = 0;	
				real32 AudioLatencySeconds = 0;

				win32_game_code Game = Win32LoadGameCode(
					SourceGameCodeDLLFullPath,
					TempGameCodeDLLFullPath,
					GameCodeLockFullPath);

				int64 LastCycleCount = __rdtsc();
				while (GLobalRunning)
				{
					NewInput->dtForFrame = TargetSecondPerFrame;
					
					NewInput->ExecutableReloaded = false;
					FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
					if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime))
					{
						Win32CompleteAllWork(&HighPriorityQueue);
						Win32CompleteAllWork(&LowPriorityQueue);

						Win32UnloadGameCode(&Game);
						Game = Win32LoadGameCode(
							SourceGameCodeDLLFullPath,
							TempGameCodeDLLFullPath,
							GameCodeLockFullPath);
						
						NewInput->ExecutableReloaded = true;
					}

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

					Win32ProcessPendingMessages(&Win32State, NewKeyboardController);
					
					if (!GLobalPause)
					{
						POINT MouseP;
						GetCursorPos(&MouseP);
						ScreenToClient(Window, &MouseP);
						NewInput->MouseX = MouseP.x;
						NewInput->MouseY = MouseP.y;
						NewInput->MouseZ = 0; // TODO: Support mousewheel?

						Win32ProcessKeyboardMessage(&NewInput->MouseButton[0],
							GetKeyState(VK_LBUTTON) & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButton[1],
							GetKeyState(VK_MBUTTON) & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButton[2],
							GetKeyState(VK_RBUTTON) & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButton[3],
							GetKeyState(VK_XBUTTON1) & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButton[4],
							GetKeyState(VK_XBUTTON2) & (1 << 15));

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
							}
						}

						game_offscreen_buffer Buffer = {};
						Buffer.Memory = GlobalBackBaffer.Memory;
						Buffer.Height = GlobalBackBaffer.Height;
						Buffer.Width = GlobalBackBaffer.Width;
						Buffer.Pitch = GlobalBackBaffer.Pitch;
						
						if (Win32State.InputRecordingIndex)
						{
							Win32RecordInput(&Win32State, NewInput);
						}

						if (Win32State.InputPlayingIndex)
						{
							Win32PlayBackInput(&Win32State, NewInput);
						}

						if (Game.UpdateAndRender)
						{
							Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);
							HandleDebugCycleCounters(&GameMemory);
						}

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
							SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
							SoundBuffer.Samples = Samples;
							if (Game.GetSoundSamples)
							{
								Game.GetSoundSamples(&GameMemory, &SoundBuffer);
							}
#if HANDMADE_INTERNAL
							win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarketIntex];
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

						LARGE_INTEGER EndCounter = Win32GetWallClock();
						real32 MSPerFrame = 1000.f*Win32GetSecondsElapsed(LastCounter, EndCounter);
						LastCounter = EndCounter;

						win32_window_dimension Dimension = Win32GetWindowDimension(Window);
#if HANDMADE_INTERNAL
						// TODO: Note, current is wrong on the zero'th index
						// Win32DebugSyncDisplay(
						// 	&GlobalBackBaffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers, 
						// 	DebugTimeMarketIntex - 1,&SoundOutput, TargetSecondPerFrame);
#endif
						HDC DeviceContext = GetDC(Window);
						Win32DisplayBufferInWindow(
							&GlobalBackBaffer, DeviceContext,
							Dimension.Width, Dimension.Height);
						ReleaseDC(Window, DeviceContext);

						FlipWallClock = Win32GetWallClock();
#if HANDMADE_INTERNAL
						{
							DWORD PlayCursor2;
							DWORD WriteCursor2;
							if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor2, &WriteCursor2) == DS_OK)
							{
								Assert(DebugTimeMarketIntex < ArrayCount(DebugTimeMarkers));
								win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarketIntex];
								Marker->FlipPlayCursor = PlayCursor2;
								Marker->FlipWriteCursor = WriteCursor2;
							}
						}
#endif

						game_input *Temp = NewInput;
						NewInput = OldInput;
						OldInput = Temp;
#if 1
						uint64 EndCycleCount = __rdtsc();
						uint64 CycleElapsed = EndCycleCount - LastCycleCount;
						LastCycleCount = EndCycleCount;

						real32 FPS = 0;
						real32 MCPF = ((real32)CycleElapsed / (1000.0f * 1000.0f));

						char FPSBuffer[256];
						_snprintf_s(FPSBuffer, sizeof(FPSBuffer), 
							"%.02fms/f, %.02ff/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF);
						OutputDebugStringA(FPSBuffer);
#endif

#if HANDMADE_INTERNAL
						++DebugTimeMarketIntex;
						if (DebugTimeMarketIntex == ArrayCount(DebugTimeMarkers))
						{
							DebugTimeMarketIntex = 0;
						}
#endif
					}
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