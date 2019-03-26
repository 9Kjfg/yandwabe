#if !defined(TEST_ASSET_BUILDER_H)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "handmade_platform.h"
#include "handmade_file_format.h"
#include "handmade_intrinsics.h"
#include "handmade_math.h"

enum asset_type
{
	AssetType_Sound,
	AssetType_Bitmap,
	AssetType_Font,
	AssetType_FontGlyph
};

struct loaded_font;
struct asset_source_font
{
	loaded_font *Font;
};

struct asest_source_font_glyph
{
	loaded_font *Font;
	u32 Codepoint;
};

struct asset_source_bitmap
{
	char *FileName;
};

struct asset_source_sound
{
	char *FileName;
	u32 FirstSampleIndex;
};

struct asset_source
{
	asset_type Type;
	union
	{
		asset_source_bitmap Bitmap;
		asset_source_sound Sound;
		asset_source_font Font;
		asest_source_font_glyph Glyph;
	};
};

// Are there larger number tha 4096? Do we gave evidence
// in the natural work of things that can exist in quantities _larger_ than 4096
#define VERY_LARGE_NUMBER 4096 // NOTE: 4096 should be enough for anybody

struct game_assets
{
	uint32 TagCount;
    hha_tag Tags[VERY_LARGE_NUMBER];

    u32 AssetTypeCount;
	hha_asset_type AssetTypes[Asset_Count];
	
    uint32 AssetCount;
	asset_source AssetSources[VERY_LARGE_NUMBER];
	hha_asset Assets[VERY_LARGE_NUMBER];

	hha_asset_type *DEBUGAssetType;
	u32 AssetIndex;
};

#define TEST_ASSET_BUILDER_H
#endif