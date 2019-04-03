#include "test_asset_builder.h"

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

struct entire_file
{
	u32 ContentsSize;
	void *Contents;
};

entire_file
ReadEntireFile(char *FileName)
{
	entire_file Result = {};

	FILE *In = fopen(FileName, "rb");
	if (In)
	{
		fseek(In, 0, SEEK_END);
		Result.ContentsSize = ftell(In);
		fseek(In, 0, SEEK_SET);

		Result.Contents = malloc(Result.ContentsSize);
		fread(Result.Contents, Result.ContentsSize, 1, In);
		fclose(In);
	}
	else
	{
		printf("ERROR: Cannot open file %s.\n", FileName);
	}

	return(Result);
}

internal loaded_bitmap
LoadBMP(char *FileName)
{
	loaded_bitmap Result = {};

    entire_file ReadResult = ReadEntireFile(FileName);
	if (ReadResult.ContentsSize != 0)
	{
		Result.Free = ReadResult.Contents;

		bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
		uint32 *Pixel = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
		Result.Memory = Pixel;
		Assert(Result.Memory != 0);
		Result.Width = Header->Width;
		Result.Height = Header->Height;
		
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

internal loaded_font *
LoadFont(char *FileName, char *FontName, int PixelHeight)
{

	loaded_font *Font = (loaded_font *)malloc(sizeof(loaded_font));

	AddFontResourceExA(FileName, FR_PRIVATE, 0);
	Font->Win32Handle = CreateFontA(
		PixelHeight, 0, 0, 0,
		FW_NORMAL,// NOTE: Weight
		FALSE, // NOTE: Italic
		FALSE, // NOTE: Underline
		FALSE, // NOTE: Strikeout
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY,
		DEFAULT_PITCH|FF_DONTCARE,
		FontName);

	SelectObject(GlobalFontDeviceContext, Font->Win32Handle);
	GetTextMetrics(GlobalFontDeviceContext, &Font->TextMetric);
	
	Font->MinCodePoint = INT_MAX;
	Font->MaxCodePoint = 0;
	
	// NOTE: 5000 characters should be more than enough for _anybody_
	Font->MaxGlyphCount = 5000;
	Font->GlyphCount = 0;

	u32 GlyphIndexFromCodePointSize = ONE_PAST_MAX_FONT_CODEPOINT_COUNT*sizeof(u32);
	Font->GlyphIndexFromCodePoint = (u32 *)malloc(GlyphIndexFromCodePointSize);
	memset(Font->GlyphIndexFromCodePoint, 0, GlyphIndexFromCodePointSize);

	Font->Glyphs = (hha_font_glyph *)malloc(sizeof(hha_font_glyph)*Font->MaxGlyphCount);
	size_t HorizontalAdvanceSize = sizeof(r32)*Font->MaxGlyphCount*Font->MaxGlyphCount;
	Font->HorizontalAdvance = (r32 *)malloc(HorizontalAdvanceSize);
	memset(Font->HorizontalAdvance, 0, HorizontalAdvanceSize);

	Font->OnePastHighestCodepoint = 0;

	// NOTE: Reserve space for the null glyph
	Font->GlyphCount = 1;
	Font->Glyphs[0].UnicodeCodePoint = 0;
	Font->Glyphs[0].BitmapID.Value = 0;

	return(Font);
}

internal void
FinalizeFontKerning(loaded_font *Font)
{
	SelectObject(GlobalFontDeviceContext, Font->Win32Handle);

	DWORD KerningPairCount = GetKerningPairs(GlobalFontDeviceContext, 0, 0);
	KERNINGPAIR *KerningPairs = (KERNINGPAIR *)malloc(KerningPairCount*sizeof(KERNINGPAIR));
	GetKerningPairsW(GlobalFontDeviceContext, KerningPairCount, KerningPairs);
	for (DWORD KerningPairIndex = 0;
		KerningPairIndex < KerningPairCount;	
		++KerningPairIndex)
	{
		KERNINGPAIR *Pair = KerningPairs + KerningPairIndex;
		if ((Pair->wFirst < ONE_PAST_MAX_FONT_CODEPOINT_COUNT) &&
			(Pair->wSecond < ONE_PAST_MAX_FONT_CODEPOINT_COUNT))
		{
			u32 First = Font->GlyphIndexFromCodePoint[Pair->wFirst];
			u32 Second = Font->GlyphIndexFromCodePoint[Pair->wSecond];
			if ((First != 0) && (Second != 0))
			{
				Font->HorizontalAdvance[First*Font->MaxGlyphCount + Second] += (r32)Pair->iKernAmount;
			}
		}
	}

	free(KerningPairs);
}

internal void
FreeFont(loaded_font *Font)
{
	if (Font)
	{
		DeleteObject(Font->Win32Handle);
		free(Font->Glyphs);
		free(Font->HorizontalAdvance);
		free(Font->GlyphIndexFromCodePoint);
		free(Font);
	}
}

internal void
InitializeFontDC(void)
{
	GlobalFontDeviceContext = CreateCompatibleDC(GetDC(0));

	BITMAPINFO Info = {};
	Info.bmiHeader.biSize = sizeof(Info.bmiHeader);
	Info.bmiHeader.biWidth = MAX_FONT_WIDTH;
	Info.bmiHeader.biHeight = MAX_FONT_HEIGHT;
	Info.bmiHeader.biPlanes = 1;
	Info.bmiHeader.biBitCount = 32;
	Info.bmiHeader.biCompression = BI_RGB;
	Info.bmiHeader.biSizeImage = 0;
	Info.bmiHeader.biXPelsPerMeter = 0;
	Info.bmiHeader.biYPelsPerMeter = 0;
	Info.bmiHeader.biClrUsed = 0;
	Info.bmiHeader.biClrImportant = 0;
	HBITMAP Bitmap = CreateDIBSection(GlobalFontDeviceContext, &Info, DIB_RGB_COLORS, &GlobalFontBits, 0, 0);
	SelectObject(GlobalFontDeviceContext, Bitmap);
	SetBkColor(GlobalFontDeviceContext, RGB(0, 0, 0));
}

internal loaded_bitmap
LoadGlyphBitmap(loaded_font *Font, u32 CodePoint, hha_asset *Asset)
{
	loaded_bitmap Result = {};

	u32 GlyphIndex = Font->GlyphIndexFromCodePoint[CodePoint];

#if USE_FONTS_FROM_WINDOWS

	SelectObject(GlobalFontDeviceContext, Font->Win32Handle);

	memset(GlobalFontBits, 0x00, MAX_FONT_WIDTH*MAX_FONT_HEIGHT*sizeof(u32));

	wchar_t CheesePoint = (wchar_t)CodePoint;

	SIZE Size;
	GetTextExtentPoint32W(GlobalFontDeviceContext, &CheesePoint, 1, &Size);

	int PreStepX = 128;

	int BoundWidth = Size.cx + 2*PreStepX;
	if (BoundWidth > MAX_FONT_WIDTH)
	{
		BoundWidth = MAX_FONT_WIDTH;
	}
	int BoundHeight = Size.cy;
	if (BoundHeight > MAX_FONT_HEIGHT)
	{
		BoundHeight = MAX_FONT_HEIGHT;
	}

	//PatBlt(DeviceContext, 0, 0, Width, Height, BLACKNESS);
	SetTextColor(GlobalFontDeviceContext, RGB(255, 255, 255));
	TextOutW(GlobalFontDeviceContext, PreStepX, 0, &CheesePoint, 1);

	s32 MinX = 10000;
	s32 MinY = 10000;
	s32 MaxX = -10000;
	s32 MaxY = -10000;

	u32 *Row = (u32 *)GlobalFontBits + (MAX_FONT_HEIGHT - 1)*MAX_FONT_WIDTH;
	
	for (s32 Y = 0;
		Y < BoundHeight;
		++Y)
	{
		u32 *Pixel = Row;
		for (s32 X = 0;
			X < BoundWidth;
			++X)
		{
			if (*Pixel != 0)
			{
				if (MinX > X)
				{
					MinX = X;
				}
				if (MinY > Y)
				{
					MinY = Y;
				}
				if (MaxX < X)
				{
					MaxX = X;
				}
				if (MaxY < Y)
				{
					MaxY = Y;
				}
			}
			++Pixel;
		}
		Row -= MAX_FONT_WIDTH;
	}

	r32 KerningChange = 0.0f;
	if (MinX <= MaxX)
	{
		int Width = (MaxX - MinX) + 1;
		int Height = (MaxY - MinY) + 1;

		Result.Width = Width + 2;
		Result.Height = Height + 2;
		Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;
		Result.Memory = malloc(Result.Height*Result.Pitch);
		Result.Free = Result.Memory;
		
		memset(Result.Memory, 0, Result.Height*Result.Pitch);

		u8 *DestRow = (u8 *)Result.Memory + (Result.Height - 1 - 1)*Result.Pitch;
		u32 *SourceRow = (u32 *)GlobalFontBits + (MAX_FONT_HEIGHT - 1 - MinY)*MAX_FONT_WIDTH;
		for (s32 Y = MinY;
			Y <= MaxY;
			++Y)
		{
			u32 *Source = (u32 *)SourceRow + MinX;
			u32 *Dest = (u32 *)DestRow + 1;
			for (s32 X = MinX;
				X <= MaxX;
				++X)
			{
#if 0
				COLORREF Pixel = GetPixel(DeviceContext, X, Y);
				Assert(Pixel == *Source);
#else
				u32 Pixel = *Source;
#endif
				r32 Gray = (r32)(Pixel & 0xFF);
				v4 Texel = {255.0f, 255.0f, 255.0f, Gray};
				Texel = SRGB255ToLinear1(Texel);
				Texel.rgb *= Texel.a;
				Texel = Linear1ToSRGB255(Texel);

				*Dest++ = 
					(((uint32)(Texel.a + 0.5f) << 24)|
					((uint32)(Texel.r + 0.5f) << 16) |
					((uint32)(Texel.g + 0.5f) << 8) |
					((uint32)(Texel.b + 0.5f)) << 0);

				++Source;
			}

			DestRow -= Result.Pitch;
			SourceRow -= MAX_FONT_WIDTH;
		}
		Asset->Bitmap.AlignPercentage[0] = (1.0f) / (r32)Result.Width;
		Asset->Bitmap.AlignPercentage[1] = (1.0f + (MaxY - (BoundHeight - Font->TextMetric.tmDescent))) / (r32)Result.Height;
		
		KerningChange = (r32)(MinX - PreStepX);
	}

#if 0
	ABC ThisABC;
	GetCharABCWidthsW(GlobalFontDeviceContext, CodePoint, CodePoint, &ThisABC);
	r32 CharAdvance = (r32)(ThisABC.abcA + ThisABC.abcB + ThisABC.abcC);
#else
	INT ThisWidth;
	GetCharWidth32W(GlobalFontDeviceContext, CodePoint, CodePoint, &ThisWidth);
	r32 CharAdvance = (r32)ThisWidth;
#endif

	for (u32 OtherGlyphIndex = 0;
		OtherGlyphIndex < Font->MaxGlyphCount;
		++OtherGlyphIndex)
	{
		Font->HorizontalAdvance[GlyphIndex*Font->MaxGlyphCount + OtherGlyphIndex] += (CharAdvance - KerningChange);
		if (OtherGlyphIndex != 0)
		{
			Font->HorizontalAdvance[OtherGlyphIndex*Font->MaxGlyphCount + GlyphIndex] += KerningChange;
		}
	}

#else

	entire_file TTFFile = ReadEntireFile(FileName);

	if (TTFFile.ContentsSize != 0)
	{
		stbtt_fontinfo Font;
		stbtt_InitFont(&Font, (u8 *)TTFFile.Contents, stbtt_GetFontOffsetForIndex((u8 *)TTFFile.Contents, 0));
	
		int Width, Height, XOffset, YOffset;
		u8 *MonoBitmap = stbtt_GetCodepointBitmap(&Font, 0, stbtt_ScaleForPixelHeight(&Font, 120.0f),
			CodePoint, &Width, &Height, &XOffset, &YOffset);
		
		Result.Width = Width;
		Result.Height = Height;
		Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;
		Result.Memory = malloc(Result.Pitch*Height);
		Result.Free = Result.Memory;
	
		u8 *Source = MonoBitmap;
		u8 *DestRow = (u8 *)Result.Memory + (Height - 1)*Result.Pitch;
		for (s32 Y = 0;
			Y < Height;
			++Y)
		{
			u32 *Dest = (u32 *)DestRow;
			for (s32 X = 0;
				X < Width;
				++X)
			{
				u8 Alpha = *Source++;
				*Dest++ = 
					((Alpha << 24) |
					(Alpha << 16) |
					(Alpha << 8) |
					(Alpha << 0));
			}
			DestRow -= Result.Pitch;
		}

		stbtt_FreeBitmap(MonoBitmap, 0);
		free(TTFFile.Contents);
	}
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

struct loaded_sound
{
	uint32 SampleCount; // NOTE: This is the sample count divided by 8
	uint32 ChannelCount;
	int16 *Samples[2];

	void *Free;
};

internal loaded_sound
LoadWAV(char *FileName, uint32 SectionFirstSampleIndex, uint32 SectionSampleCount)
{
	loaded_sound Result = {};

	entire_file ReadResult = ReadEntireFile(FileName);
	if (ReadResult.ContentsSize != 0)
	{
		Result.Free = ReadResult.Contents;

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

internal void
BeginAssetType(game_assets *Assets, asset_type_id TypeID)
{
	Assert(Assets->DEBUGAssetType == 0);
	Assets->DEBUGAssetType = Assets->AssetTypes + TypeID;
	Assets->DEBUGAssetType->TypeID = TypeID;
	Assets->DEBUGAssetType->FirstAssetIndex = Assets->AssetCount;
	Assets->DEBUGAssetType->OnePastLastAssetIndex = Assets->DEBUGAssetType->FirstAssetIndex;
}

struct added_asset
{
	u32 ID;
	hha_asset *HHA;
	asset_source *Source;
};

internal added_asset
AddAsset(game_assets *Assets)
{
	Assert(Assets->DEBUGAssetType);
	Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < ArrayCount(Assets->Assets));

	u32 Index = Assets->DEBUGAssetType->OnePastLastAssetIndex++;
	asset_source *Source = Assets->AssetSources + Index;
	hha_asset *HHA = Assets->Assets + Index;
	HHA->FirstTagIndex = Assets->TagCount;
	HHA->OnePastLastTagIndex = HHA->FirstTagIndex;

	Assets->AssetIndex = Index;

	added_asset Result;
	Result.ID = Index;
	Result.HHA = HHA;
	Result.Source = Source;
	return(Result);
}

internal bitmap_id
AddBitmapAsset(game_assets *Assets, char *FileName, r32 AlignPercentageX = 0.5f, r32 AlignPercentageY = 0.5f)
{
	added_asset Asset = AddAsset(Assets);
	Asset.HHA->Bitmap.AlignPercentage[0] = AlignPercentageX;
	Asset.HHA->Bitmap.AlignPercentage[1] = AlignPercentageY;
	Asset.Source->Type = AssetType_Bitmap;
	Asset.Source->Bitmap.FileName = FileName;

	bitmap_id Result = {Asset.ID};
	return(Result);
}

internal bitmap_id
AddCharacterAsset(game_assets *Assets, loaded_font *Font, u32 CodePoint)
{
	added_asset Asset = AddAsset(Assets);
	Asset.HHA->Bitmap.AlignPercentage[0] = 0.0f; // NOTE: Set later be extraction
	Asset.HHA->Bitmap.AlignPercentage[1] = 0.0f; // NOTE: Set later be extraction
	Asset.Source->Type = AssetType_FontGlyph;
	Asset.Source->Glyph.Codepoint = CodePoint;
	Asset.Source->Glyph.Font = Font;

	bitmap_id Result = {Asset.ID};

	Assert(Font->GlyphCount < Font->MaxGlyphCount);
	u32 GlyphIndex = Font->GlyphCount++;
	hha_font_glyph *Glyph = Font->Glyphs + GlyphIndex;
	Glyph->UnicodeCodePoint = CodePoint;
	Glyph->BitmapID = Result;
	Font->GlyphIndexFromCodePoint[CodePoint] = GlyphIndex;

	if (Font->OnePastHighestCodepoint <= CodePoint)
	{
		Font->OnePastHighestCodepoint = CodePoint + 1;
	}

	return(Result);
}

internal sound_id
AddSoundAsset(game_assets *Assets, char *FileName, u32 FirstSampleIndex = 0, u32 SampleCount = 0)
{
	added_asset Asset = AddAsset(Assets);
	Asset.HHA->Sound.SampleCount = SampleCount;
	Asset.HHA->Sound.Chain = HHASoundChain_None;
	Asset.Source->Type = AssetType_Sound;
	Asset.Source->Sound.FileName = FileName;
	Asset.Source->Sound.FirstSampleIndex = FirstSampleIndex;

	sound_id Result = {Asset.ID};
	return(Result);
}

internal font_id
AddFontAsset(game_assets *Assets, loaded_font *Font)
{
	added_asset Asset = AddAsset(Assets);
	Asset.HHA->Font.OnePastHighestCodepoint = Font->OnePastHighestCodepoint;
	Asset.HHA->Font.GlyphCount = Font->GlyphCount;
	Asset.HHA->Font.AscenderHeight = (r32)Font->TextMetric.tmAscent;
	Asset.HHA->Font.DescenderHeight = (r32)Font->TextMetric.tmDescent;
	Asset.HHA->Font.ExternalLeading = (r32)Font->TextMetric.tmExternalLeading;
	Asset.Source->Type = AssetType_Font;
	Asset.Source->Font.Font = Font;

	font_id Result = {Asset.ID};
	return(Result);
}

internal void
AddTag(game_assets *Assets, asset_tag_id ID, real32 Value)
{
	Assert(Assets->AssetIndex);

	hha_asset *HHA = Assets->Assets + Assets->AssetIndex;
	++HHA->OnePastLastTagIndex;
	hha_tag *Tag = Assets->Tags + Assets->TagCount++;

	Tag->ID = ID;
	Tag->Value = Value;
}

internal void
EndAssetType(game_assets *Assets)
{
	Assert(Assets->DEBUGAssetType);
	Assets->AssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
	Assets->DEBUGAssetType = 0;
	Assets->AssetIndex = 0;
}

internal void
WriteHHA(game_assets *Assets, char *FileName)
{
	FILE *Out = fopen(FileName, "wb");
    if (Out)
    {
		hha_header Header = {};
		Header.MagicValue = HHA_MAGIC_VALUE;
		Header.Version = HHA_VERSION;
		Header.TagCount = Assets->TagCount;
		Header.AssetTypeCount = Asset_Count; // TODO: Do we really want to do this? Sparsenes!
		Header.AssetCount = Assets->AssetCount;
		
		u32 TagArraySize = Header.TagCount*sizeof(hha_tag);
		u32 AssetTypeArraySize = Header.AssetTypeCount*sizeof(hha_asset_type);
		u32 AssetArraySize = Header.AssetCount*sizeof(hha_asset);
		
		Header.Tags = sizeof(Header);
		Header.AssetTypes = Header.Tags + TagArraySize;
		Header.Assets = Header.AssetTypes + AssetTypeArraySize;

		fwrite(&Header, sizeof(Header), 1, Out);
		fwrite(Assets->Tags, TagArraySize, 1, Out);
		fwrite(Assets->AssetTypes, AssetTypeArraySize, 1, Out);
		fseek(Out, AssetArraySize, SEEK_CUR);
		for (u32 AssetIndex = 1;
			AssetIndex < Header.AssetCount;
			++AssetIndex)
		{
			asset_source *Source = Assets->AssetSources + AssetIndex;
			hha_asset *Dest = Assets->Assets + AssetIndex;
			
			Dest->DataOffset = ftell(Out);

			if (Source->Type == AssetType_Sound)
			{
				loaded_sound WAV = LoadWAV(Source->Sound.FileName, Source->Sound.FirstSampleIndex, Dest->Sound.SampleCount);
				
				Dest->Sound.SampleCount = WAV.SampleCount;
				Dest->Sound.ChannelCount = WAV.ChannelCount;
				
				for (u32 ChannelIndex = 0;
					ChannelIndex < WAV.ChannelCount;
					++ChannelIndex)
				{
					fwrite(WAV.Samples[ChannelIndex], Dest->Sound.SampleCount*sizeof(s16), 1, Out);
				}

				free(WAV.Free);
			}
			else if (Source->Type == AssetType_Font) 
			{
				loaded_font *Font = Source->Font.Font;
				
				FinalizeFontKerning(Font);
				
				u32 GlyphsSize = Font->GlyphCount*sizeof(hha_font_glyph);
				fwrite(Font->Glyphs, GlyphsSize, 1, Out);

				u8 *HorizontalAdvance = (u8 *)Font->HorizontalAdvance;
				for (u32 GlyphIndex = 0;
					GlyphIndex < Font->GlyphCount;
					++GlyphIndex)
				{
					u32 HorizontalAdvanceSliceSize = sizeof(r32)*Font->GlyphCount;
					fwrite(HorizontalAdvance, HorizontalAdvanceSliceSize, 1, Out);
					HorizontalAdvance += sizeof(r32)*Font->MaxGlyphCount;
				}
			}
			else
			{
				loaded_bitmap Bitmap;
				if (Source->Type == AssetType_FontGlyph)
				{
					Bitmap = LoadGlyphBitmap(Source->Glyph.Font, Source->Glyph.Codepoint, Dest);
				}
				else
				{
					Assert(Source->Type == AssetType_Bitmap);
					Bitmap = LoadBMP(Source->Bitmap.FileName);
				}

				Dest->Bitmap.Dim[0] = Bitmap.Width;
				Dest->Bitmap.Dim[1] = Bitmap.Height;

				Assert((Bitmap.Width*4) == Bitmap.Pitch);
				fwrite(Bitmap.Memory, Bitmap.Width*Bitmap.Height*4, 1, Out);

				free(Bitmap.Free);
			}
		}
		fseek(Out, (u32)Header.Assets, SEEK_SET);
		fwrite(Assets->Assets, AssetArraySize, 1, Out);

        fclose(Out);
    }
	else
	{
		printf("ERROR: Couldn't open file :\n");
	}
}

internal void
Initialize(game_assets *Assets)
{
	Assets->TagCount = 1;
	Assets->AssetCount = 1;
	Assets->DEBUGAssetType = 0;
	Assets->AssetIndex = 0;

	Assets->AssetTypeCount = Asset_Count;
	memset(Assets->AssetTypes, 0, sizeof(Assets->AssetTypes));
}

internal void
WriteFonts(void)
{
	game_assets Assets_;
	game_assets *Assets = &Assets_;
	Initialize(Assets);

	loaded_font *Fonts[] = 
	{
		LoadFont("C:/Windows/Fonts/arial.ttf", "Arial", 50),
		LoadFont("C:/Windows/Fonts/LiberationMono-Regular.ttf", "Liberation Mono", 20)
	};

	BeginAssetType(Assets, Asset_FontGlyph);
	for (u32 FontIndex = 0;
		FontIndex < ArrayCount(Fonts);
		++FontIndex)
	{
		loaded_font *Font = Fonts[FontIndex];

		AddCharacterAsset(Assets, Font, ' ');
		for (u32 Character = '!';
			Character <= '~';
			++Character)
		{
			AddCharacterAsset(Assets, Font, Character);
		}

		// NOTE: Kanji Owl
		AddCharacterAsset(Assets, Font, 0x5c0f);
		AddCharacterAsset(Assets, Font, 0x8033);
		AddCharacterAsset(Assets, Font, 0x6728);
		AddCharacterAsset(Assets, Font, 0x514e);
	}
	EndAssetType(Assets);

	// TODO: This is kinda janky, because it means you have to get this
	// order right always
	BeginAssetType(Assets, Asset_Font);
	AddFontAsset(Assets, Fonts[0]);
	AddTag(Assets, Tag_FontType, FontType_Default);
	AddFontAsset(Assets, Fonts[1]);
	AddTag(Assets, Tag_FontType, FontType_Debug);
	EndAssetType(Assets);

	WriteHHA(Assets, "testfonts.hha");
}

internal void
WriteHero(void)
{
	game_assets Assets_;
	game_assets *Assets = &Assets_;
	Initialize(Assets);

	real32 AngleRight = 0.0f*Tau32;
	real32 AngleBack = 0.25f*Tau32;
	real32 AngleLeft = 0.5f*Tau32;
	real32 AngleFront = 0.75f*Tau32;

	r32 HeroAlign[2] = {0.5f, 0.1566802029f};

	BeginAssetType(Assets, Asset_Head);
	AddBitmapAsset(Assets, "test/test_hero_right_head.bmp", HeroAlign[0], HeroAlign[1]);
	AddTag(Assets, Tag_FacingDirection, AngleRight);
	AddBitmapAsset(Assets, "test/test_hero_back_head.bmp", HeroAlign[0], HeroAlign[1]);
	AddTag(Assets, Tag_FacingDirection, AngleBack);
	AddBitmapAsset(Assets, "test/test_hero_left_head.bmp", HeroAlign[0], HeroAlign[1]);
	AddTag(Assets, Tag_FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, "test/test_hero_front_head.bmp", HeroAlign[0], HeroAlign[1]);
	AddTag(Assets, Tag_FacingDirection, AngleFront);
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Cape);
	AddBitmapAsset(Assets, "test/test_hero_right_cape.bmp", HeroAlign[0], HeroAlign[1]);
	AddTag(Assets, Tag_FacingDirection, AngleRight);
	AddBitmapAsset(Assets, "test/test_hero_back_cape.bmp", HeroAlign[0], HeroAlign[1]);
	AddTag(Assets, Tag_FacingDirection, AngleBack);
	AddBitmapAsset(Assets, "test/test_hero_left_cape.bmp", HeroAlign[0], HeroAlign[1]);
	AddTag(Assets, Tag_FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, "test/test_hero_front_cape.bmp", HeroAlign[0], HeroAlign[1]);
	AddTag(Assets, Tag_FacingDirection, AngleFront);
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Torso);
	AddBitmapAsset(Assets, "test/test_hero_right_torso.bmp", HeroAlign[0], HeroAlign[1]);
	AddTag(Assets, Tag_FacingDirection, AngleRight);
	AddBitmapAsset(Assets, "test/test_hero_back_torso.bmp", HeroAlign[0], HeroAlign[1]);
	AddTag(Assets, Tag_FacingDirection, AngleBack);
	AddBitmapAsset(Assets, "test/test_hero_left_torso.bmp", HeroAlign[0], HeroAlign[1]);
	AddTag(Assets, Tag_FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, "test/test_hero_front_torso.bmp", HeroAlign[0], HeroAlign[1]);
	AddTag(Assets, Tag_FacingDirection, AngleFront);
	EndAssetType(Assets);

	WriteHHA(Assets, "test1.hha");
}

internal void
WriteNonHero(void)
{
	game_assets Assets_;
	game_assets *Assets = &Assets_;
	Initialize(Assets);

	BeginAssetType(Assets, Asset_Shadow);
	AddBitmapAsset(Assets, "test/test_hero_shadow.bmp", 0.5f, 0.156682829f);
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Tree);
	AddBitmapAsset(Assets, "test/tree.bmp", 0.493827164f, 0.295652181f);
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Sword);
	AddBitmapAsset(Assets, "test/rock03.bmp", 0.5f, 0.65625f);
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

	WriteHHA(Assets, "test2.hha");
}

internal void
WriteSounds(void)
{
	game_assets Assets_;
	game_assets *Assets = &Assets_;
	Initialize(Assets);

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
		if ((FirstSampleIndex + OneMusicChunk) < TotalMusicSampleCount)
		{
			Assets->Assets[ThisMusic.Value].Sound.Chain = HHASoundChain_Advance;
		}
	}
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Puhp);
	AddSoundAsset(Assets, "test/drop0.wav");
	EndAssetType(Assets);

	WriteHHA(Assets, "test3.hha");
}

int
main(int ArgCoutn, char **Args)
{
	InitializeFontDC();
	
	WriteFonts();
	WriteHero();
	WriteNonHero();
	WriteSounds();

	return(0);
}