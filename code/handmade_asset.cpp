
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
#pragma pack(pop)

inline v2
TopDownAlign(loaded_bitmap *Bitmap, v2 Align)
{
	Align.y = (real32)(Bitmap->Height -1) - Align.y;
	
	Align.x = SafeRatio0(Align.x, (real32)Bitmap->Width);
	Align.y = SafeRatio0(Align.y, (real32)Bitmap->Height);

	return(Align);
}

internal void
SetToDownAlign(hero_bitmaps *Bitmap, v2 Align)
{
	Align = TopDownAlign(&Bitmap->Head, Align);

	Bitmap->Head.AlignPercentage = Align;
	Bitmap->Cape.AlignPercentage = Align;
	Bitmap->Torso.AlignPercentage = Align;
}

internal loaded_bitmap
DEBUGLoadBMP(char *FileName, int32 AlignX, int32 TopDownAlignY)
{
	loaded_bitmap Result = {};

    debug_read_file_result ReadResult = DEBUGPlatformReadEntireFile(FileName);
	if (ReadResult.ContentsSize != 0)
	{
		bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
		uint32 *Pixel = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
		Result.Memory = Pixel;
		Result.Width = Header->Width;
		Result.Height = Header->Height;
		Result.AlignPercentage = TopDownAlign(&Result, V2i(AlignX, TopDownAlignY));
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

internal loaded_bitmap
DEBUGLoadBMP(char *FileName)
{
	loaded_bitmap Result = DEBUGLoadBMP(FileName, 0, 0);
	Result.AlignPercentage = V2(0.5f, 0.5f);
	return(Result);
}

struct load_bitmap_work
{	
	game_assets *Assets;
	char *FileName;
	bitmap_id ID;
	task_with_memory *Task;
	loaded_bitmap *Bitmap;

	bool32 HasAlignmet;
	int32 AlignX;
	int32 TopDownAlignY;

	asset_state FinalState;
};

internal
PLATFORM_WORK_QUEUE_CALLBACK(LoadBitmapWork)
{
	load_bitmap_work *Work = (load_bitmap_work *)Data;

	// TODO: Get rid of this thread thing when i load throught when i load thorught a queue instead of the debug call

	if (Work->HasAlignmet)
	{
		*Work->Bitmap = DEBUGLoadBMP(Work->FileName, Work->AlignX, Work->TopDownAlignY);
	}
	else
	{
		*Work->Bitmap = DEBUGLoadBMP(Work->FileName);
	}

	CompletePreviousWritesBeforeFutureWrites;

	Work->Assets->Bitmaps[Work->ID.Value].Bitmap = Work->Bitmap;
	Work->Assets->Bitmaps[Work->ID.Value].State = Work->FinalState;

	EndTaskWidthMemory(Work->Task);
}

internal void
LoadBitmap(game_assets *Assets, bitmap_id ID)
{
	if (ID.Value &&
        (AtomicCompareExchangeUInt32((uint32 *)&Assets->Bitmaps[ID.Value].State, AssetState_Unloaded, AssetState_Queued) ==
		AssetState_Unloaded))
	{
		task_with_memory *Task = BeginTaskWidthMemory(Assets->TranState);
		if (Task)
		{
			load_bitmap_work *Work = PushStruct(&Task->Arena, load_bitmap_work);

			Work->Assets = Assets;
			Work->ID = ID;
			Work->FileName = "";
			Work->Task = Task;
			Work->Bitmap = PushStruct(&Assets->Arena, loaded_bitmap);
			Work->HasAlignmet = false;
			Work->FinalState = AssetState_Loaded;

			 0;
			switch (ID.Value)
			{
				case Asset_Backdrop:
				{
					Work->FileName = "test/test_background.bmp";
				} break;

				case Asset_Shadow:
				{
					Work->FileName = "test/test_hero_shadow.bmp";
					Work->HasAlignmet = true;
					Work->AlignX = 72;
					Work->TopDownAlignY = 182;
				} break;
				
				case Asset_Tree:
				{
					Work->FileName = "test/tree.bmp";
					Work->HasAlignmet = true;
					Work->AlignX = 40;
					Work->TopDownAlignY = 50;
				} break;

				case Asset_Stairwell:
				{
					Work->FileName = "test/rock03.bmp";
				} break;

				case Asset_Sword:
				{
					Work->FileName = "test/rock03.bmp";
					Work->HasAlignmet = true;
					Work->AlignX = 29;
					Work->TopDownAlignY = 19;
				} break;
			}

			PlatformAddEntry(Assets->TranState->LowPriorityQueue, LoadBitmapWork, Work);
		}
	}		
}


internal void
LoadSound(game_assets *Assets, uint32 ID)
{

}

internal bitmap_id
GetFirstBitmapID(game_assets *Assets, asset_type_id TypeID)
{
    bitmap_id Result = {};

    asset_type *Type = Assets->AssetTypes + TypeID;
    if (Type->FirstAssetIndex != Type->OnePastLastTagIndex)
    {
        asset *Asset = Assets->Assets + Type->FirstAssetIndex;
        Result.Value = Asset->SlotID;
    }

    return(Result);
}

internal game_assets *
AllocateGameAssets(memory_arena *Arena, transient_state *TranState, memory_index Size)
{
    game_assets *Assets = PushStruct(Arena, game_assets);
    SubArena(&Assets->Arena, Arena, Size);
    Assets->TranState = TranState;

    Assets->BitmapCount = Asset_Count;
    Assets->Bitmaps = PushArray(Arena, Assets->BitmapCount, asset_slot);

    Assets->SoundCount = 1;
    Assets->Sounds = PushArray(Arena, Assets->SoundCount, asset_slot);

    Assets->TagCount = 0;
    Assets->Tags = 0;

    Assets->AssetCount = Assets->BitmapCount;
    Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);

    for (uint32 AssetID = 0;
        AssetID < Asset_Count;
        ++AssetID)
    {
        asset_type *Type = Assets->AssetTypes + AssetID;
        Type->FirstAssetIndex = AssetID;
        Type->OnePastLastTagIndex = AssetID + 1;

        asset *Asset = Assets->Assets + Type->FirstAssetIndex;
        Asset->FirstTagIndex = 0;
        Asset->OnePastLastTagIndex = 0;
        Asset->SlotID = Type->FirstAssetIndex;
    }

    Assets->Grass[0] = DEBUGLoadBMP("test/grass00.bmp");
    Assets->Grass[1] = DEBUGLoadBMP("test/grass00.bmp");

    Assets->Tuft[0] = DEBUGLoadBMP("test/tuft.bmp");
    Assets->Tuft[1] = DEBUGLoadBMP("test/tuft.bmp");
    Assets->Tuft[2] = DEBUGLoadBMP("test/tuft.bmp");

    Assets->Stone[0] = DEBUGLoadBMP("test/ground00.bmp");
    Assets->Stone[1] = DEBUGLoadBMP("test/ground01.bmp");
    Assets->Stone[2] = DEBUGLoadBMP("test/ground00.bmp");
    Assets->Stone[3] = DEBUGLoadBMP("test/ground01.bmp");

    hero_bitmaps *Bitmap;

    Bitmap = Assets->HeroBitmaps;
    Bitmap->Head = DEBUGLoadBMP("test/test_hero_right_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP("test/test_hero_right_cape.bmp");
    Bitmap->Torso =	DEBUGLoadBMP("test/test_hero_right_torso.bmp");
    SetToDownAlign(Bitmap, V2(72, 182));
    ++Bitmap;

    Bitmap->Head = DEBUGLoadBMP("test/test_hero_back_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP("test/test_hero_back_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP("test/test_hero_back_torso.bmp");
    SetToDownAlign(Bitmap, V2(72, 182));
    ++Bitmap;

    Bitmap->Head = DEBUGLoadBMP("test/test_hero_left_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP("test/test_hero_left_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP("test/test_hero_left_torso.bmp");
    SetToDownAlign(Bitmap, V2(72, 182));
    ++Bitmap;

    Bitmap->Head = DEBUGLoadBMP("test/test_hero_front_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP("test/test_hero_front_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP("test/test_hero_front_torso.bmp");
    SetToDownAlign(Bitmap, V2(72, 182));

    return(Assets);
}