#include <stdio.h>
#include <stdlib.h>
#include "handmade_platform.h"
#include "handmade_asset_type_id.h"

FILE *Out = 0;

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
	u32 NextIDToPlay;
};

struct asset_tag
{
	uint32 ID;
	real32 Value;
};

struct asset
{
	u64 DataOffset;
	u32 FirstTagIndex;
	u32 OnePastLastTagIndex;
};

struct asset_type
{
	u32 FirstAssetIndex;
	u32 OnePastLastAssetIndex;
};

#define VERY_LARGE_NUMBER 4096

uint32 BitmapCount;
uint32 SoundCount;
uint32 TagCount;
uint32 AssetCount;
asset_bitmap_info BitmapInfos[VERY_LARGE_NUMBER];
asset_sound_info SoundInfos[VERY_LARGE_NUMBER];
asset_tag Tags[VERY_LARGE_NUMBER];
asset Assets[VERY_LARGE_NUMBER];
asset_type AssetTypes[Asset_Count];

u32 DEBUGUsedBitmapCount;
u32 DEBUGUsedSoundCount;
u32 DEBUGUsedAssetCount;
u32 DEBUGUsedTagCount;
asset_type *DEBUGAssetType;
asset *DEBUGAsset;

internal void
BeginAssetType(asset_type_id TypeID)
{
	Assert(DEBUGAssetType == 0);

	DEBUGAssetType = AssetTypes + TypeID;
	DEBUGAssetType->FirstAssetIndex = DEBUGUsedAssetCount;
	DEBUGAssetType->OnePastLastAssetIndex = DEBUGAssetType->FirstAssetIndex;
}

internal void
AddBitmapAsset(char *FileName, real32 AlignPercentageX = 0.5f, real32 AlignPercentageY = 0.5f)
{
	Assert(DEBUGAssetType);
	Assert(DEBUGAssetType->OnePastLastAssetIndex < AssetCount);

	asset *Asset = DEBUGAssetType->OnePastLastAssetIndex++;
	Asset->FirstTagIndex = DEBUGUsedTagCount;
	Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
	Asset->SlotID = DEBUGAddBitmapInfo(Assets, FileName, AlignPercentage).Value;

    /*
    internal bitmap_id
    DEBUGAddBitmapInfo(char *FileName, v2 AlignPercentage)
    {
        Assert(Assets->DEBUGUsedBitmapCount < Assets->BitmapCount);
        bitmap_id ID = {Assets->DEBUGUsedBitmapCount++};

        asset_bitmap_info *Info = Assets->BitmapInfos + ID.Value;
        Info->FileName = PushString(&Assets->Arena, FileName);
        Info->AlignPercentage = AlignPercentage;

        return(ID);
    }
    */

	Assets->DEBUGAsset = Asset;
}

internal asset *
AddSoundAsset(char *FileName, u32 FirstSampleIndex = 0, u32 SampleCount = 0)
{
	Assert(Assets->DEBUGAssetType);
	Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < Assets->AssetCount);

	asset *Asset = Assets->Assets + Assets->DEBUGAssetType->OnePastLastAssetIndex++;
	Asset->FirstTagIndex = Assets->DEBUGUsedTagCount;
	Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
	Asset->SlotID = DEBUGAddSoundInfo(Assets, FileName, FirstSampleIndex, SampleCount).Value;

	Assets->DEBUGAsset = Asset;

    /*
    internal sound_id
    DEBUGAddSoundInfo(char *FileName, u32 FirstSampleIndex = 0, u32 SampleCount = 0)
    {
        Assert(Assets->DEBUGUsedSoundCount < Assets->SoundCount);
        sound_id ID = {Assets->DEBUGUsedSoundCount++};

        asset_sound_info *Info = Assets->SoundInfos + ID.Value;
        Info->FileName = PushString(&Assets->Arena, FileName);
        Info->FirstSampleIndex = FirstSampleIndex;
        Info->SampleCount = SampleCount;
        Info->NextIDToPlay.Value = 0;

        return(ID);
    }
    */

	return(Asset);
}

internal void
AddTag(game_assets *Assets, asset_tag_id ID, real32 Value)
{
	Assert(Assets->DEBUGAsset);

	++Assets->DEBUGAsset->OnePastLastTagIndex;
	asset_tag *Tag = Assets->Tags + Assets->DEBUGUsedTagCount++;

	Tag->ID = ID;
	Tag->Value = Value;
}

internal void
EndAssetType(game_assets *Assets)
{
	Assert(Assets->DEBUGAssetType);
	Assets->DEBUGUsedAssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
	Assets->DEBUGAssetType = 0;
}

int
main(int ArgCoutn, char **Args)
{
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
	asset *LastMusic = 0;
	for (u32 FirstSampleIndex = 0;
		FirstSampleIndex < TotalMusicSampleCount;
		FirstSampleIndex += OneMusicChunk)
	{
		u32 SampleCount = TotalMusicSampleCount - FirstSampleIndex;
		if (SampleCount > OneMusicChunk)
		{
			SampleCount = OneMusicChunk;
		}
		asset *ThisMusic = AddSoundAsset(Assets, "test/music0.wav", FirstSampleIndex, SampleCount);
		if (LastMusic)
		{
			Assets->SoundInfos[LastMusic->SlotID].NextIDToPlay.Value = ThisMusic->SlotID;
		}
		LastMusic = ThisMusic;
	}
	EndAssetType(Assets);

	BeginAssetType(Assets, Asset_Puhp);
	AddSoundAsset(Assets, "test/drop0.wav");
	EndAssetType(Assets);

    Out = fopen("test.hha", "wb");
    
    if (Out)
    {

        fclose(Out);
    }

}