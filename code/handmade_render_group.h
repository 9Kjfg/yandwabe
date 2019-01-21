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
	v2 AlignPercentage;
	real32 WidthOverHeight;

	int32 Width;
	int32 Height;
	int32 Pitch;
	void *Memory;
};

struct environment_map
{
	loaded_bitmap LOD[4];
	real32 Pz;
};

struct render_basis
{
    v3 P;
};

struct render_entity_basis
{
	render_basis *Basis;
	v3 Offset;
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

	environment_map *Top;
	environment_map *Middle;
	environment_map *Bottom;
};
// }

struct render_entry_bitmap
{;
	render_entity_basis EntityBasis;
	loaded_bitmap *Bitmap;
	v2 Size;
	v4 Color;
};

struct render_entry_rectangle
{
	render_entity_basis EntityBasis;
	v4 Color;
	v2 Dim;
};

struct render_group_camera
{
	// NOTE: Camera parameters
	real32 FocalLength;
	real32 DistanceAboveTarget;
};

struct render_group
{
	render_group_camera GameCamera;
	render_group_camera RenderCamera;

	real32 MetersToPixels; // NOTE: This translates meters _on the monitor_ into pixels _on the monitor_
	v2 MonitroHalfDimInMeters;
	real32 GlobalAlpha;

    render_basis *DefaultBasis;

    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8 *PushBufferBase;
};

#define HANDMADE_RENDER_GROUP_H
#endif