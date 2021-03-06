#if !defined(HANDMADE_RENDER_GROUP_H)

/* NOTE:
	1) Everywhere outside the render, Y _always_ goes upward, X to the right

	2) All bitmaps including the render target are assumed to be bottom-up
	(meaning that the first row pointer points to the bottom-most row
	 when viewed on screen)

	3) It is mandatory that  all inputs to the render are in world
	coordinate ("meters"), NOT pixels. If for same reason something
	absolutely has to be specified in pixels, that will be explicitly
	marked in the API, but this should occur exceedingly sparingly
	
	4) Z is a specila coordinate vecouse it is vroken up into discrete slices
	and the render actually understands these slices. Z slices are what
	control the _scaling_ of things, whereas Z offsets inside a slice are
	what control Y offsetting.

	5) All color values specifeid to the render as V4's are in
	NON-premultiplied alpha.

	// TODO: ZHANDLING
*/

struct loaded_bitmap
{
	void *Memory;
	v2 AlignPercentage;
	r32 WidthOverHeight;
	s16 Width;
	s16 Height;
	// TODO: Get rid of pitch
	s32 Pitch;
	void *TextureHandle;
};

struct environment_map
{
	loaded_bitmap LOD[4];
	real32 Pz;
};

// NOTE: render_group_entry is a "compact efficient disriminated uion"
enum render_group_entry_type
{
	RenderGroupEntryType_render_entry_clear,
	RenderGroupEntryType_render_entry_bitmap,
	RenderGroupEntryType_render_entry_rectangle,
	RenderGroupEntryType_render_entry_cliprect,
	RenderGroupEntryType_render_entry_cordinate_system,
};

struct render_group_entry_header // TODO: don't store type here, sore is sort index
{
	u16 Type;
	u16 ClipRectIndex;
};

struct render_entry_cliprect
{
	render_entry_cliprect *Next;
	rectangle2i Rect;
	u32 RenderTarget;
};

struct render_entry_clear
{
	v4 Color;
};

struct render_entry_bitmap
{
	loaded_bitmap *Bitmap;
	
	v4 Color;
	v2 P;
//	v2 Size;
	// NOTE: X and Y axes are _already scaled_ by the half-dimension
	v2 XAxis;
	v2 YAxis;
};

struct render_entry_rectangle
{
	v4 Color;
	v2 P;
	v2 Dim;
};

// NOTE: This is only for test
// {
struct render_entry_cordinate_system
{
	v2 Origin;
	v2 XAxis;
	v2 YAxis;
	v4 Color;
	loaded_bitmap *Texture;
	loaded_bitmap *NormalMap;

	real32 PixelsToMeters; // TODO: Need to store this for lighting

	environment_map *Top;
	environment_map *Middle;
	environment_map *Bottom;
};
// }

struct object_transform
{
	b32 Upright;
	v3 OffsetP;
	r32 Scale;
	r32 SortBias;
};

struct camera_transform
{
	b32 Orthographic;

	// NOTE: Camera parameters
	r32 MetersToPixels; // NOTE: This translates meters _on the monitor_ into pixels _on the monitor_
	v2 ScreenCenter;

	r32 FocalLength;
	r32 DistanceAboveTarget;
};

struct render_group
{
	struct game_assets *Assets;
	real32 GlobalAlpha;

	v2 MonitroHalfDimInMeters;

    camera_transform CameraTransform;

	uint32 MissingResourceCount;
	b32 RendersInBackground;

	u32 CurrentClipRectIndex;

	u32 GenerationID;
	game_render_commands *Commands;
};

struct entity_basis_p_result
{
	v2 P;
	r32 Scale;
	b32 Valid;
	r32 SortKey;
};

struct used_bitmap_dim
{
	entity_basis_p_result Basis;
	v2 Size;
	v2 Align;
	v3 P;
};

void DrawRectangleQuickly(loaded_bitmap *Buffer,
	v2 Origin, v2 XAxis, v2 YAxis, v4 Color,
	loaded_bitmap *Texture,	real32 PixelsToMeters,
	rectangle2i ClipRect);

inline object_transform
DefaultUprightTransform(void)
{
	object_transform Result = {};

	Result.Upright = true;
	Result.Scale = 1.0f;

	return(Result);
}

inline object_transform
DefaultFlatTransform(void)
{
	object_transform Result = {};

	Result.Scale = 1.0f;

	return(Result);
}

#define HANDMADE_RENDER_GROUP_H
#endif