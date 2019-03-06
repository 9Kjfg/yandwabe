
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

	Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);

	CompletePreviousWritesBeforeFutureWrites;

	// TODO: Should we actually fill in bogus data here and set to final state anyway
	if (PlatformNoFileErrors(Work->Handle))
	{
		Work->Slot->State = Work->FinalState;
	}

	EndTaskWidthMemory(Work->Task);
}


internal platform_file_handle *
GetFileHandleFor(game_assets *Assets, u32 FileIndex)
{
	Assert(FileIndex < Assets->FileCount);
	platform_file_handle *Result = Assets->Files[FileIndex].Handle;

	return(Result);
}

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
			asset *Asset = Assets->Assets + ID.Value;
			hha_bitmap *Info = &Asset->HHA.Bitmap;
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
			Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
			Work->Offset = Asset->HHA.DataOffset;
			Work->Size = MemorySize;
			Work->Destination = Bitmap->Memory;
			Work->FinalState = AssetState_Loaded;
			Work->Slot->Bitmap = Bitmap;

			Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
		}
		else
		{
			Assets->Slots[ID.Value].State = AssetState_Unloaded;
		}
	}		
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
			asset *Asset = Assets->Assets + ID.Value;
			hha_sound *Info = &Asset->HHA.Sound;
			
			loaded_sound *Sound = PushStruct(&Assets->Arena, loaded_sound);
			Sound->SampleCount = Info->SampleCount;
			Sound->ChannelCount = Info->ChannelCount;
			u32 ChannelSize = Sound->SampleCount*sizeof(int16);
			u32 MemorySize = Sound->ChannelCount*ChannelSize;
			
			void *Memory = PushSize(&Assets->Arena, MemorySize);

			int16 *SoundAt = (int16 *)Memory;
			for (u32 ChannelIndex = 0;
				ChannelIndex < Sound->ChannelCount;
				++ChannelIndex)
			{
				Sound->Samples[ChannelIndex] = SoundAt;
				SoundAt += ChannelSize;
			}

			load_asset_work *Work = PushStruct(&Task->Arena, load_asset_work);
			Work->Task = Task;
			Work->Slot = Assets->Slots + ID.Value;
			Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
			Work->Offset = Asset->HHA.DataOffset;
			Work->Size = MemorySize;
			Work->Destination = Memory;
			Work->FinalState = AssetState_Loaded;
			Work->Slot->Sound = Sound;

			Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
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
		asset *Asset = Assets->Assets + AssetIndex;

		real32 TotalWeightedDiff = 0.0f;
		for (uint32 TagIndex = Asset->HHA.FirstTagIndex;
			TagIndex < Asset->HHA.OnePastLastTagIndex;
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

	Assets->TagCount = 0;
	Assets->AssetCount = 0;

	{
		platform_file_group FileGroup = Platform.GetAllFilesOfTypeBegin("hha");
		Assets->FileCount = FileGroup.FileCount;
		Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
		for (u32 FileIndex = 0;
			FileIndex < Assets->FileCount;
			++FileIndex)
		{
			asset_file *File = Assets->Files + FileIndex;

			File->TagBase = Assets->TagCount;

			ZeroStruct(File->Header);
			File->Handle = Platform.OpenFile(FileGroup, FileIndex);
			Platform.ReadDataFromFile(File->Handle, 0, sizeof(File->Header), &File->Header);
			
			u32 AssetTypeArraySize = File->Header.AssetTypeCount*sizeof(hha_asset_type);
			File->AssetTypeArray = (hha_asset_type *)PushSize(Arena, AssetTypeArraySize);
			Platform.ReadDataFromFile(File->Handle, File->Header.AssetTypes, 
				AssetTypeArraySize,	File->AssetTypeArray);

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
				InvalidCodePath;
			}
		}
		Platform.GetAllFilesOfTypeEnd(FileGroup);
	}

	// NOTE: All metadata space
	Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);
	Assets->Slots = PushArray(Arena, Assets->AssetCount, asset_slot);
	Assets->Tags = PushArray(Arena, Assets->TagCount, hha_tag);

	// NOTE: Load tags
	for (u32 FileIndex = 0;
		FileIndex < Assets->FileCount;
		++FileIndex)
	{
		asset_file *File = Assets->Files + FileIndex;
		if (PlatformNoFileErrors(File->Handle))
		{
			u32 TagArraySize = sizeof(hha_tag)*File->Header.TagCount;
			Platform.ReadDataFromFile(File->Handle, File->Header.Tags, 
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

		for (u32 FileIndex = 0;
			FileIndex < Assets->FileCount;
			++FileIndex)
		{
			asset_file *File = Assets->Files + FileIndex;
			if (PlatformNoFileErrors(File->Handle))
			{
				for (u32 SourceIndex = 0;
					SourceIndex < File->Header.AssetTypeCount;
					++SourceIndex)
				{
					hha_asset_type *SourceType = File->AssetTypeArray + SourceIndex;

					if (SourceType->TypeID == DestTypeID)
					{
						u32 AssetCountForType = 
							(SourceType->OnePastLastAssetIndex - SourceType->FirstAssetIndex);

						temporary_memory TempMem = BeginTemporaryMemory(&TranState->TranArena);
						hha_asset *HHAAssetArray = PushArray(&TranState->TranArena, AssetCountForType, hha_asset);

						Platform.ReadDataFromFile(File->Handle, 
							File->Header.Assets + SourceType->FirstAssetIndex*sizeof(hha_asset),
							AssetCountForType*sizeof(hha_asset), HHAAssetArray);

						for (u32 AssetIndex = 0;
							AssetIndex < AssetCountForType;
							++AssetIndex)
						{
							hha_asset *HHAAsset = HHAAssetArray + AssetIndex;

							Assert(AssetCount < Assets->AssetCount);
							asset *Asset = Assets->Assets + AssetCount++;
						
							Asset->FileIndex = FileIndex;
							Asset->HHA = *HHAAsset;
							Asset->HHA.FirstTagIndex += File->TagBase;
							Asset->HHA.OnePastLastTagIndex += File->TagBase;
						}

						EndTemporaryMemory(TempMem);
					}
				}
			}
		}

		DestType->OnePastLastAssetIndex = AssetCount;
	}

	//Assert(AssetCount == Assets->AssetCount);

#if 0
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