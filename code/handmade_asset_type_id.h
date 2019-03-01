#if !defined(HANDMADE_ASSET_TYPE_ID)

enum asset_type_id
{
    Asset_None,
	
	//
	//NOTE: Bitmaps!
	//
	
	Asset_Shadow,
	Asset_Tree,
	Asset_Sword,
	//Asset_Stairwell,
	Asset_Rock,

	Asset_Grass,
	Asset_Tuft,
	Asset_Stone,

	Asset_Head,
	Asset_Cape,
	Asset_Torso,

	//
	//NOTE: Sounds!
	//

	Asset_Bloop,
	Asset_Crack,
	Asset_Drop,
	Asset_Glide,
	Asset_Music,
	Asset_Puhp,

	Asset_Count
};

#define HANDMADE_ASSET_TYPE_ID
#endif