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
	s16 Pitch;
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
	RenderGroupEntryType_render_entry_cordinate_system
};

struct render_group_entry_header
{
	render_group_entry_type Type;
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
	v2 Size;
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

struct render_transform
{
	bool32 Orthographic;

	// NOTE: Camera parameters
	real32 MetersToPixels; // NOTE: This translates meters _on the monitor_ into pixels _on the monitor_
	v2 ScreenCenter;

	real32 FocalLength;
	real32 DistanceAboveTarget;

	v3 OffsetP;
	real32 Scale;
};

struct render_group
{
	struct game_assets *Assets;
	real32 GlobalAlpha;

	u32 GenerationID;

	v2 MonitroHalfDimInMeters;

    render_transform Transform;

    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8 *PushBufferBase;

	uint32 MissingResourceCount;
	b32 RendersInBackground;

	b32 InsideRender;
};

void DrawRectangleQuickly(loaded_bitmap *Buffer,
	v2 Origin, v2 XAxis, v2 YAxis, v4 Color,
	loaded_bitmap *Texture,	real32 PixelsToMeters,
	rectangle2i ClipRect, bool32 Even);

#define HANDMADE_RENDER_GROUP_H
#endif