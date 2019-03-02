#if !defined(TEST_ASSET_BUILDER_H)

#include <stdio.h>
#include <stdlib.h>
#include "handmade_platform.h"
#include "handmade_asset_type_id.h"

struct bitmap_id
{
    uint32 Value;
};

struct sound_id
{
    uint32 Value;
};

#include "handmade_file_format.h"

struct asset_bitmap_info
{
	char *FileName;
	r32 AlignPercentage[2];
};

struct asset_sound_info
{
	char *FileName;
	uint32 FirstSampleIndex;
	uint32 SampleCount;
	sound_id NextIDToPlay;
};

struct asset
{
	u64 DataOffset;
	u32 FirstTagIndex;
	u32 OnePastLastTagIndex;

	union
	{
		asset_bitmap_info Bitmap;
		asset_sound_info Sound;
	};
};

#define VERY_LARGE_NUMBER 4096

struct game_assets
{
	uint32 TagCount;
    hha_tag Tags[VERY_LARGE_NUMBER];

    u32 AssetTypeCount;
	hha_asset_type AssetTypes[Asset_Count];
	
    uint32 AssetCount;
	asset Assets[VERY_LARGE_NUMBER];

	hha_asset_type *DEBUGAssetType;
	asset *DEBUGAsset;
};

#define TEST_ASSET_BUILDER_H
#endif