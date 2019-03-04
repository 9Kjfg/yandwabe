#if !defined(TEST_ASSET_BUILDER_H)

#include <stdio.h>
#include <stdlib.h>
#include "handmade_platform.h"
#include "handmade_asset_type_id.h"
#include "handmade_file_format.h"
#include "handmade_intrinsics.h"
#include "handmade_math.h"

enum asset_type
{
	AssetType_Sound,
	AssetType_Bitmap
};

struct asset_source
{
	asset_type Type;
	char *Filename;
	u32 FirstSampleIndex;
};

#define VERY_LARGE_NUMBER 4096

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