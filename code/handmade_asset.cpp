#if 0
#pragma pack(push, 1)
struct bitmap_header
{
    uint16 FileType;        /* File type, always 4D42h ("BM") */
    uint32 FileSize;        /* Size of the file in bytes */
    uint16 Reserved1;       /* Always 0 */
    uint16 Reserved2;       /* Always 0 */
    uint32 BitmapOffset;    /* Starting position of image data in bytes */
    uint32 Size;            /* Size of this header in bytes */
    int32 Width;            /* Image width in pixels */
    int32 Height;           /* Image height in pixels */
    uint16 Planes;          /* Number of color planes */
    uint16 BitsPerPixel;    /* Number of bits per pixel */
	int32 Compression;
    int32 SizeOfBitmap;		/* Size of bitmap in bytes */
    int32 HorzResolution;	/* Horizontal resolution in pixels per meter */
    int32 VertResolution;	/* Vertical resolution in pixels per meter */
    int32 ColorsUsed;       /* Number of colors in the image */
    int32 ColorsImportant;  /* Minimum number of important colors */

    uint32 RedMask;
    uint32 GreenMask;
    uint32 BlueMask;
};

#define RIFF_CODE(a, b, c, d) (((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))
enum
{
	WAVE_ChunkID_fmt = RIFF_CODE('f', 'm', 't', ' '),
	WAVE_ChunkID_data = RIFF_CODE('d', 'a', 't', 'a'),
	WAVE_ChunkID_RIFF = RIFF_CODE('R', 'I', 'F', 'F'),
	WAVE_ChunkID_WAVE = RIFF_CODE('W', 'A', 'V', 'E')
};

struct WAVE_header 
{
	uint32 RIFFID;
	uint32 Size;
	uint32 WAVEID;
};

struct WAVE_chunk
{
	uint32 ID;
	uint32 Size;
};

struct WAVE_fmt
{
	uint16 wFormatTag;
	uint16 nChannels;
	uint32 nSamplesPerSec;
	uint32 nAvgBytesPerSec;
	uint16 nBlockAlign;
	uint16 wBitsPerSamle;
	uint16 cbSize;
	uint16 wValidBitsPerSample;
	uint32 dwChannelMask;
	uint8 SubFormt[16];
};

#pragma pack(pop)

internal loaded_bitmap
DEBUGLoadBMP(char *FileName, v2 AlignPercentage = V2(0.5f, 0.5f))
{
	loaded_bitmap Result = {};

    debug_read_file_result ReadResult = DEBUGPlatformReadEntireFile(FileName);
	if (ReadResult.ContentsSize != 0)
	{
		bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
		uint32 *Pixel = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
		Result.Memory = Pixel;
		Assert(Result.Memory != 0);
		Result.Width = Header->Width;
		Result.Height = Header->Height;
		Result.AlignPercentage = AlignPercentage;
		Result.WidthOverHeight = SafeRatio0((real32)Result.Width, (real32)Result.Height);
		
		Assert(Result.Height >= 0);
		Assert(Header->Compression == 3);

		// NOTE; Byte order in memory is determined by the Header itself,
		// os we have to read out the masks and convert hte pixels ourselves
		uint32 RedMask = Header->RedMask;
    	uint32 GreenMask = Header->GreenMask;
    	uint32 BlueMask = Header->BlueMask;
		uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

		bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
		bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
		bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
		bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);

		Assert(RedScan.Found);
		Assert(GreenScan.Found);
		Assert(BlueScan.Found);
		Assert(AlphaScan.Found);

		int32 RedShiftDown = (int32)RedScan.Index;
		int32 GreenShiftDown = (int32)GreenScan.Index;
		int32 BlueShiftDown = (int32)BlueScan.Index;
		int32 AlphaShiftDown = (int32)AlphaScan.Index;

		uint32 *SourceDest = Pixel;
		for (int32 Y = 0;
			Y < Header->Width;
			++Y)
		{
			for (int32 X = 0;
				X < Header->Height;
				++X)
			{
				uint32 C = *SourceDest;

				v4 Texel = 
				{
					(real32)((C & RedMask) >> RedShiftDown),
					(real32)((C & GreenMask) >> GreenShiftDown),
					(real32)((C & BlueMask) >> BlueShiftDown),
					(real32)((C & AlphaMask) >> AlphaShiftDown)
				};
				
				Texel = SRGB255ToLinear1(Texel);
#if 1
				Texel.rgb *= Texel.a;
#endif
				Texel = Linear1ToSRGB255(Texel);

				*SourceDest++ = 
					(((uint32)(Texel.a + 0.5f) << 24)|
					((uint32)(Texel.r + 0.5f) << 16) |
					((uint32)(Texel.g + 0.5f) << 8) |
					((uint32)(Texel.b + 0.5f)) << 0);
			}
		}
	}
	Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;
#if 0
	Result.Memory = (uint8 *)Result.Memory + Result.Pitch*(Result.Height - 1);
	Result.Pitch = -Result.Pitch
#endif
	return(Result);
}

struct riff_iterator
{
	uint8 *At;
	uint8 *Stop;
};

inline riff_iterator
ParseChunkAt(void *At, void *Stop)
{
	riff_iterator Iter;

	Iter.At = (uint8 *)At;
	Iter.Stop = (uint8 *)Stop;

	return(Iter);
}

inline riff_iterator
NextChunk(riff_iterator Iter)
{
	WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
	uint32 Size = (Chunk->Size + 1) & ~1;
	Iter.At += sizeof(WAVE_chunk) + Size;

	return(Iter);
}

inline bool32
IsValid(riff_iterator Iter)
{
	bool32 Result = (Iter.At < Iter.Stop);
	return(Result);
}

inline void *
GetChunkData(riff_iterator Iter)
{
	void *Result = (Iter.At + sizeof(WAVE_chunk));
	return(Result);
}

inline uint32
GetType(riff_iterator Iter)
{
	WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
	uint32 Result = Chunk->ID;

	return(Result);
}

inline uint32
GetChunkDataSize(riff_iterator Iter)
{
	WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
	uint32 Result = Chunk->Size;

	return(Result);
}

internal loaded_sound
DEBUGLoadWAV(char *FileName, uint32 SectionFirstSampleIndex, uint32 SectionSampleCount)
{
	loaded_sound Result = {};

	debug_read_file_result ReadResult = DEBUGPlatformReadEntireFile(FileName);
	if (ReadResult.ContentsSize != 0)
	{
		WAVE_header *Header = (WAVE_header *)ReadResult.Contents;
		Assert(Header->RIFFID == WAVE_ChunkID_RIFF);
		Assert(Header->WAVEID == WAVE_ChunkID_WAVE);
		
		uint32 ChannelCount = 0;
		uint32 SampleDataSize = 0;
		int16 *SampleData = 0;
		for (riff_iterator Iter = ParseChunkAt(Header + 1, (uint8 *)(Header + 1) + Header->Size - 4);
			IsValid(Iter);
			Iter = NextChunk(Iter))
		{
			switch (GetType(Iter))
			{
				case WAVE_ChunkID_fmt:
				{
					WAVE_fmt *fmt = (WAVE_fmt *)GetChunkData(Iter);
					Assert(fmt->wFormatTag == 1); // NOTE: Only support PCM
					//Assert(fmt->nSamplesPerSec == 48000);
					Assert(fmt->wBitsPerSamle == 16);
					Assert(fmt->nBlockAlign == (2*fmt->nChannels));
					ChannelCount = fmt->nChannels;
				} break;

				case WAVE_ChunkID_data:
				{
					SampleData = (int16 *)GetChunkData(Iter);
					SampleDataSize = GetChunkDataSize(Iter);
				} break;
			}
		}

		Assert(ChannelCount && SampleData);

		Result.ChannelCount = ChannelCount;
		u32 SampleCount = SampleDataSize / (ChannelCount*sizeof(int16));
		if (ChannelCount == 1)
		{
			Result.Samples[0] = SampleData;
			Result.Samples[1] = 0;
		}
		else if (ChannelCount == 2)
		{
			Result.Samples[0] = SampleData;
			Result.Samples[1] = SampleData + SampleCount;

			for (uint32 SampleIndex = 0;
				SampleIndex < SampleCount;
				++SampleIndex)
			{
				int16 Source = SampleData[2*SampleIndex];
				SampleData[2*SampleIndex] = SampleData[SampleIndex];
				SampleData[SampleIndex] = Source;
			}
		}
		else
		{
			Assert(!"Invalid channel count in WAV file");
		}

		// TODO: Load right channels
		b32 AtEnd = true;
		Result.ChannelCount = 1;
		if (SectionSampleCount)
		{
			Assert((SectionFirstSampleIndex + SectionSampleCount) <= SampleCount);
			AtEnd = (SectionFirstSampleIndex + SectionSampleCount) == SampleCount;
			SampleCount = SectionSampleCount;
			for (uint32 ChannelIndex = 0;
				ChannelIndex < Result.ChannelCount;
				++ChannelIndex)
			{
				Result.Samples[ChannelIndex] += SectionFirstSampleIndex;
			}
		}

		if (AtEnd)
		{	
			// TODO: All sounds have to be padded with their subsequent sound out
			// to 8 samples past their end
			u32 SampleCountAlign8 = Align8(SampleCount);
			for (uint32 ChannelIndex = 0;
				ChannelIndex < Result.ChannelCount;
				++ChannelIndex)
			{
				for (u32 SampleIndex = SampleCount;
					SampleIndex < (SampleCount + 8);
					++SampleIndex)
				{
					Result.Samples[ChannelIndex][SampleIndex] = 0;
				}

			}	
		}

		Result.SampleCount = SampleCount;
	}

	return(Result);
}
#endif

struct load_asset_work
{
	task_with_memory *Task;
	asset_slot *Slot;

	platform_file_handle *Handle;
	u64 Offset;
	u64 Size;
	void *Destination;

	asset_state FinalState;
};

internal
PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork)
{
	load_asset_work *Work = (load_asset_work *)Data;
#if 0
	Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);
#endif

	CompletePreviousWritesBeforeFutureWrites;

	// TODO: Should we actually fill in bogus data here and set to final state anyway
	//if (PlatformNoFileErrors(Work->Handle))
	{
		Work->Slot->State = Work->FinalState;
	}

	EndTaskWidthMemory(Work->Task);
}

#if 0
struct load_bitmap_work
{	
	game_assets *Assets;
	bitmap_id ID;
	task_with_memory *Task;
	loaded_bitmap *Bitmap;

	asset_state FinalState;
};

internal
PLATFORM_WORK_QUEUE_CALLBACK(LoadBitmapWork)
{
	load_bitmap_work *Work = (load_bitmap_work *)Data;

	hha_asset *HHAAsset = &Work->Assets->Assets[Work->ID.Value];
	hha_bitmap *Info = &HHAAsset->Bitmap;
	loaded_bitmap *Bitmap = Work->Bitmap;

	Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
	Bitmap->WidthOverHeight = (r32)Info->Dim[0] / (r32)Info->Dim[1];
	Bitmap->Width = Info->Dim[0];
	Bitmap->Height = Info->Dim[1];
	Bitmap->Pitch = 4*Info->Dim[0];
	Bitmap->Memory = Work->Assets->HHAContents + HHAAsset->DataOffset;

	Work->Assets->Slots[Work->ID.Value].Bitmap = Work->Bitmap;

	CompletePreviousWritesBeforeFutureWrites;

	Work->Assets->Slots[Work->ID.Value].Bitmap = Work->Bitmap;
	Work->Assets->Slots[Work->ID.Value].State = Work->FinalState;

	EndTaskWidthMemory(Work->Task);
}
#endif

internal void
LoadBitmap(game_assets *Assets, bitmap_id ID)
{
	if (ID.Value &&
        (AtomicCompareExchangeUInt32((uint32 *)&Assets->Slots[ID.Value].State, AssetState_Queued, AssetState_Unloaded) ==
		AssetState_Unloaded))
	{
		task_with_memory *Task = BeginTaskWidthMemory(Assets->TranState);
		if (Task)
		{	
			hha_asset *HHAAsset = Assets->Assets + ID.Value;
			hha_bitmap *Info = &HHAAsset->Bitmap;
			loaded_bitmap *Bitmap = PushStruct(&Assets->Arena, loaded_bitmap);

			Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
			Bitmap->WidthOverHeight = (r32)Info->Dim[0] / (r32)Info->Dim[1];
			Bitmap->Width = Info->Dim[0];
			Bitmap->Height = Info->Dim[1];
			Bitmap->Pitch = 4*Info->Dim[0];
			u32 MemorySize = Bitmap->Pitch*Bitmap->Height;
			Bitmap->Memory = PushSize(&Assets->Arena, MemorySize);
			
			load_asset_work *Work = PushStruct(&Task->Arena, load_asset_work);
			Work->Task = Task;
			Work->Slot = Assets->Slots + ID.Value;
			Work->Handle = 0;
			Work->Offset = HHAAsset->DataOffset;
			Work->Size = MemorySize;
			Work->Destination = Bitmap->Memory;
			Work->FinalState = AssetState_Loaded;
			Work->Slot->Bitmap = Bitmap;

			Bitmap->Memory = Assets->HHAContents + HHAAsset->DataOffset;

			Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
		}
		else
		{
			Assets->Slots[ID.Value].State = AssetState_Unloaded;
		}
	}		
}

struct load_sound_work
{	
	game_assets *Assets;
	sound_id ID;
	task_with_memory *Task;
	loaded_sound *Sound;

	asset_state FinalState;
};

internal
PLATFORM_WORK_QUEUE_CALLBACK(LoadSoundWork)
{
	load_sound_work *Work = (load_sound_work *)Data;

	hha_asset *HHAAsset = &Work->Assets->Assets[Work->ID.Value];
	hha_sound *Info = &HHAAsset->Sound;
	loaded_sound *Sound= Work->Sound;

	Sound->SampleCount = Info->SampleCount;
	Sound->ChannelCount = Info->ChannelCount;
	Assert(Sound->ChannelCount < ArrayCount(Sound->Samples));
	u64 SampleDataOffset = HHAAsset->DataOffset;
	for (u32 ChannelIndex = 0;
		ChannelIndex < Sound->ChannelCount;
		++ChannelIndex)
	{		
		Sound->Samples[ChannelIndex] = (int16 *)Work->Assets->HHAContents + SampleDataOffset;
		SampleDataOffset += Sound->SampleCount*sizeof(int16);
	}


	CompletePreviousWritesBeforeFutureWrites;

	Work->Assets->Slots[Work->ID.Value].Sound = Work->Sound;
	Work->Assets->Slots[Work->ID.Value].State = Work->FinalState;

	EndTaskWidthMemory(Work->Task);
}

internal void
LoadSound(game_assets *Assets, sound_id ID)
{
	if (ID.Value &&
        (AtomicCompareExchangeUInt32((uint32 *)&Assets->Slots[ID.Value].State, AssetState_Queued, AssetState_Unloaded) ==
		AssetState_Unloaded))
	{
		task_with_memory *Task = BeginTaskWidthMemory(Assets->TranState);
		if (Task)
		{
			load_sound_work *Work = PushStruct(&Task->Arena, load_sound_work);

			Work->Assets = Assets;
			Work->ID = ID;
			Work->Task = Task;
			Work->Sound = PushStruct(&Assets->Arena, loaded_sound);
			Work->FinalState = AssetState_Loaded;

			Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadSoundWork, Work);
		}
		else
		{
			Assets->Slots[ID.Value].State = AssetState_Unloaded;
		}
	}
}

internal uint32
GetBestMatchAssetFrom(game_assets *Assets, asset_type_id TypeID,
	asset_vector *MatchVector, asset_vector *WeightVector)
{
	uint32 Result = 0;

	real32 BestDiff = Real32Maximum;
	asset_type *Type = Assets->AssetTypes + TypeID;
	for (uint32 AssetIndex = Type->FirstAssetIndex;
		AssetIndex < Type->OnePastLastAssetIndex;
		++AssetIndex)
	{
		hha_asset *Asset = Assets->Assets + AssetIndex;

		real32 TotalWeightedDiff = 0.0f;
		for (uint32 TagIndex = Asset->FirstTagIndex;
			TagIndex < Asset->OnePastLastTagIndex;
			++TagIndex)
		{
			hha_tag *Tag = Assets->Tags + TagIndex;
			
			real32 A = MatchVector->E[Tag->ID];
			real32 B = Tag->Value;
			real32 D0 = AbsoluteValue(A - B);
			real32 D1 = AbsoluteValue((A - Assets->TagRange[Tag->ID] * SignOf(A)) - B);
			real32 Difference = Minimum(D0, D1);

			real32 Weighted = WeightVector->E[Tag->ID]*AbsoluteValue(Difference);
			TotalWeightedDiff += Weighted;
		}
	
		if (BestDiff > TotalWeightedDiff)
		{
			BestDiff = TotalWeightedDiff;
			Result = AssetIndex;
		}
	}

	return(Result);
}

internal uint32
GetRandomSlotFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series)
{
	uint32 Result = 0;

	asset_type *Type = Assets->AssetTypes + TypeID;
    if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
    {
		uint32 Count = Type->OnePastLastAssetIndex - Type->FirstAssetIndex;
		uint32 Choice = RandomChoice(Series, Count);
        Result = Type->FirstAssetIndex + Choice;
    }

    return(Result);
}

internal uint32
GetFirstSlotFrom(game_assets *Assets, asset_type_id TypeID)
{
    uint32 Result = 0;

    asset_type *Type = Assets->AssetTypes + TypeID;
    if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
    {
        Result = Type->FirstAssetIndex;
    }

    return(Result);
}

inline bitmap_id
GetBestMatchBitmapFrom(game_assets *Assets, asset_type_id TypeID,
	asset_vector *MatchVector, asset_vector *WeightVector)
{
	bitmap_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
	return(Result);
}

inline bitmap_id
GetFirstBitmapFrom(game_assets *Assets, asset_type_id TypeID)
{
    bitmap_id Result = {GetFirstSlotFrom(Assets, TypeID)};
    return(Result);
}

inline bitmap_id
GetRandomBitmapFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series)
{
	bitmap_id Result = {GetRandomSlotFrom(Assets, TypeID, Series)};
    return(Result);
}


inline sound_id
GetBestMatchSound(game_assets *Assets, asset_type_id TypeID,
	asset_vector *MatchVector, asset_vector *WeightVector)
{
	sound_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
	return(Result);
}

inline sound_id
GetFirstSoundFrom(game_assets *Assets, asset_type_id TypeID)
{
    sound_id Result = {GetFirstSlotFrom(Assets, TypeID)};
    return(Result);
}

inline sound_id
GetRandomSoundFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series)
{
	sound_id Result = {GetRandomSlotFrom(Assets, TypeID, Series)};
    return(Result);
}

internal game_assets *
AllocateGameAssets(memory_arena *Arena, transient_state *TranState, memory_index Size)
{
    game_assets *Assets = PushStruct(Arena, game_assets);
    SubArena(&Assets->Arena, Arena, Size);
    Assets->TranState = TranState;

	for (uint32 TagType = 0;
		TagType < Tag_Count;
		++TagType)
	{
		Assets->TagRange[TagType] = 1000000.0f;
	}
	Assets->TagRange[Tag_FacingDirection] = Tau32;
#if 0
	Assets->TagCount = 0;
	Assets->AssetCount = 0;

	{
		platform_file_group FileGroup = Platform.GetAllFilesOfTypeBegin("hha");
		Assets->FileCount = FileGroup.FileCount;
		Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
		for (u32 FileIndexs = 0;
			FileIndexs < Assets->FileCount;
			++FileIndexs;)
		{
			asset_file *File = Assets->Files + FileIndex;

			File->TagBase = Assets->TagCount;

			u64 AssetTypeArraySize = File->Header.AssetTypeCount*sizeof(hha_asset_type);

			ZeroStruct(File->Header);
			File->Handle = Platform.OpenFile(FileGroup, FileIndex);
			Platform.ReadDataFromFile(File->Handle, 0, sizeof(File->Header), &File->Header);
			File->AssetTypeArray = (hha_asset_type)PushSize(Arena, AssetTypeArraySize);
			Platform.ReadDataFromFile(File->Handle, File->Header.AssetTypes, 
				AssetTypeArraySize,	&File->AssetTypeArray);

			if (File->Header.MagicValue != HHA_MAGIC_VALUE)
			{
				Platform.FileError(File->Handle, "HHA file has an invalid magic value");
			}
			
			if (File->Header.Version != HHA_VERSION)
			{
				Platform.FileError(File->Handle, "HHA file is of a later version");
			}
			
			if (PlatformNoFileErrors(File->Handle))
			{
				Assets->TagCount += File->Header.TagCount;
				Assets->AssetCount += File->Header.AssetCount;
			}
			else
			{
				// TODO: Eventually, have some way of notifying user of bogus files?
				InvalidCodepath;
			}
		}
		Platform.GetAllFilesOfTypeEnd(FileGroup);
	}

	// NOTE: All metadata space
	Assets->Assets = PushArray(Arena, Assets->AssetCount, hha_asset);
	Assets->AssetCount = PushArray(Arena, Assets->AssetCount, asset_slot);
	Assets->Slots = PushArray(Arena, Assets->TagCount, hha_tag);

	// NOTE: Load tags
	for (u32 FileIndexs = 0;
		FileIndexs < Assets->FileCount;
		++FileIndexs;)
	{
		asset_file *File = Assets->Files + FileIndex;
		if (PlatformNoFileErrors(File->Handle))
		{
			u32 TagArraySize = sizeof(hha_tag)*File->Header.TagCount;
			Platform.ReadDataFromFile(File->Handle, File.Header.Tags, 
				TagArraySize, Assets->Tags + File->TagBase);
		}
	}

	// TODO: Excersize for the reader - how would you do this in a way
	// that scaled gracefully to hundreds of asset pack files? (or more)
	u32 AssetCount = 0;
	for (u32 DestTypeID = 0;
		DestTypeID < Asset_Count;
		++DestTypeID)
	{
		asset_type *DestType = Assets->AssetTypes + DestTypeID;
		DestType->FirstAssetIndex = AssetCount;

		for (u32 FileIndexs = 0;
				FileIndexs < Assets->FileCount;
				++FileIndexs;)
		{
			asset_file *File = Assets->Files + FileIndex;
			if (PlatformNoFileErrors(File->Handle))
			{
				for (u32 SourceIndex = 0;
					SourceIndex < File->Header.AssetTypeCount
					++SourceIndex)
				{
					hha_asset_type *SourceType = File->AssetTypeArray + SourceIndex;

					if (SourceType->TypeID == DestTypeID)
					{
						u32 AssetCountForType = (SourceType->OnePastLastAssetIndex - 
							SourceType->FirstAssetIndex);
						Platform.ReadDataFromFile(File->Handle, 
							File->Header.Assets + SourceType->FirstAssetIndex*sizeof(hha_asset)
							AssetCountForType*sizeof(hha_asset),
							Assets->Assets + AssetCount);
						for (u32 AssetIndex = AssetCount;
							AssetIndex < (AssetCount + AssetCountForType);
							++AssetIndex)
						{
							hha_asset *Asset = Assets->Assets + AssetIndex;
							Asset->FirstTagIndex += File->TagBase;
							Asset->OnepastLastTagIndex += File->TagBase;
						}
						AssetCount += AssetCountForType;
						Assert(AssetCount < Assets->AssetCount);
					}
				}
			}
		}

		DestType->OnePastLastAssetIndex = AssetCount;
	}

	Assert(AssetCount == Assets->AssetCount);
#endif

#if 1
	debug_read_file_result ReadResult = Platform.DEBUGReadEntireFile("test.hha");
	if (ReadResult.ContentsSize != 0)
	{
		hha_header *Header = (hha_header *)ReadResult.Contents;

		Assets->AssetCount = Header->AssetCount;
		Assets->Assets = (hha_asset *)((u8 *)ReadResult.Contents + Header->Assets);
		Assets->Slots = PushArray(Arena, Assets->AssetCount, asset_slot);

		Assets->TagCount = Header->TagCount;
		Assets->Tags = (hha_tag *)((u8 *)ReadResult.Contents + Header->Tags);

		hha_asset_type *HHAAssetTypes = (hha_asset_type *)((u8 *)ReadResult.Contents + Header->AssetTypes);

		for (u32 Index = 0;
			Index < Header->AssetTypeCount;
			++Index)
		{
			hha_asset_type *Source = HHAAssetTypes + Index;

			if (Source->TypeID < Asset_Count)
			{
				asset_type *Dest = Assets->AssetTypes + Source->TypeID;
				// TODO: Support merging!

				Assert(Dest->FirstAssetIndex == 0);
				Assert(Dest->OnePastLastAssetIndex == 0);
				Dest->FirstAssetIndex = Source->FirstAssetIndex;
				Dest->OnePastLastAssetIndex = Source->OnePastLastAssetIndex;
			}
		}

		Assets->HHAContents = (u8 *)ReadResult.Contents;
	}
#endif

#if 0
	Assets->DEBUGUsedAssetCount = 1;

	BeginAssetType(Assets, Asset_Shadow);
	AddBitmapAsset(Assets, "test/test_hero_shadow.bmp", V2(0.5f, 0.156682829f));
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Tree);
	AddBitmapAsset(Assets, "test/tree.bmp", V2(0.493827164f, 0.295652181f));
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Sword);
	AddBitmapAsset(Assets, "test/rock03.bmp", V2(0.5f, 0.65625f));
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Grass);
	AddBitmapAsset(Assets, "test/grass00.bmp");
	AddBitmapAsset(Assets, "test/grass00.bmp");
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Tuft);
    AddBitmapAsset(Assets, "test/tuft.bmp");
    AddBitmapAsset(Assets, "test/tuft.bmp");
    AddBitmapAsset(Assets, "test/tuft.bmp");
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Stone);
    AddBitmapAsset(Assets, "test/ground00.bmp");
    AddBitmapAsset(Assets, "test/ground01.bmp");
    AddBitmapAsset(Assets, "test/ground00.bmp");
    AddBitmapAsset(Assets, "test/ground01.bmp");
	EndAssetType(Assets);

	real32 AngleRight = 0.0f*Tau32;
	real32 AngleBack = 0.25f*Tau32;
	real32 AngleLeft = 0.5f*Tau32;
	real32 AngleFront = 0.75f*Tau32;

	v2 HeroAlign = {0.5f, 0.1566802029f};

	BeginAssetType(Assets, Asset_Head);
	AddBitmapAsset(Assets, "test/test_hero_right_head.bmp", HeroAlign);
	AddTag(Assets, Tag_FacingDirection, AngleRight);
	AddBitmapAsset(Assets, "test/test_hero_back_head.bmp", HeroAlign);
	AddTag(Assets, Tag_FacingDirection, AngleBack);
	AddBitmapAsset(Assets, "test/test_hero_left_head.bmp", HeroAlign);
	AddTag(Assets, Tag_FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, "test/test_hero_front_head.bmp", HeroAlign);
	AddTag(Assets, Tag_FacingDirection, AngleFront);
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Cape);
	AddBitmapAsset(Assets, "test/test_hero_right_cape.bmp", HeroAlign);
	AddTag(Assets, Tag_FacingDirection, AngleRight);
	AddBitmapAsset(Assets, "test/test_hero_back_cape.bmp", HeroAlign);
	AddTag(Assets, Tag_FacingDirection, AngleBack);
	AddBitmapAsset(Assets, "test/test_hero_left_cape.bmp", HeroAlign);
	AddTag(Assets, Tag_FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, "test/test_hero_front_cape.bmp", HeroAlign);
	AddTag(Assets, Tag_FacingDirection, AngleFront);
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Torso);
	AddBitmapAsset(Assets, "test/test_hero_right_torso.bmp", HeroAlign);
	AddTag(Assets, Tag_FacingDirection, AngleRight);
	AddBitmapAsset(Assets, "test/test_hero_back_torso.bmp", HeroAlign);
	AddTag(Assets, Tag_FacingDirection, AngleBack);
	AddBitmapAsset(Assets, "test/test_hero_left_torso.bmp", HeroAlign);
	AddTag(Assets, Tag_FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, "test/test_hero_front_torso.bmp", HeroAlign);
	AddTag(Assets, Tag_FacingDirection, AngleFront);
	EndAssetType(Assets);

	//
	//
	//

	BeginAssetType(Assets, Asset_Bloop);
	AddSoundAsset(Assets, "test/bloop0.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Crack);
	AddSoundAsset(Assets, "test/drop0.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Drop);
	AddSoundAsset(Assets, "test/drop0.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Glide);
	AddSoundAsset(Assets, "test/bloop0.wav");
	EndAssetType(Assets);

	u32 OneMusicChunk = 10*44100;
	u32 TotalMusicSampleCount = 58*44100;
	BeginAssetType(Assets, Asset_Music);
	sound_id LastMusic = {0};
	for (u32 FirstSampleIndex = 0;
		FirstSampleIndex < TotalMusicSampleCount;
		FirstSampleIndex += OneMusicChunk)
	{
		u32 SampleCount = TotalMusicSampleCount - FirstSampleIndex;
		if (SampleCount > OneMusicChunk)
		{
			SampleCount = OneMusicChunk;
		}
		sound_id ThisMusic = AddSoundAsset(Assets, "test/music0.wav", FirstSampleIndex, SampleCount);
		if (IsValid(LastMusic))
		{
			Assets->Assets[LastMusic.Value].Sound.NextIDToPlay = ThisMusic;
		}
		LastMusic = ThisMusic;
	}
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Puhp);
	AddSoundAsset(Assets, "test/drop0.wav");
	EndAssetType(Assets);
#endif

    return(Assets);
}