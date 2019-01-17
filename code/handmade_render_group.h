#if !defined(HANDMADE_RENDER_GROUP_H)

/* NOTE:
	1) Everywhere outside the render, Y _always_ goes upward, X to the right

	2) All bitmaps including the render target are assumed to be bottom-up
	(meaning that the first row pointer points to the bottom-most row
	 when viewed on screen)

	3) Unless otherwise specified, all inputs to the render are in world
	coordinate ("meters"), NOT pixels. Anything that is in pixel values
	will be explicitly marked as such
	
	4) Z is a specila coordinate vecouse it is vroken up into discrete slices
	and the render actually understands these slices. Z slices are what
	control the _scaling_ of things, whereas Z offsets inside a slice are
	what control Y offsetting.

	5) All color values specifeid to the render as V4's are in
	NON-premultplied alpha.

	// TODO: ZHANDLING
*/

struct loaded_bitmap
{
	v2 Align;

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
	v4 Color;
};

struct render_entry_rectangle
{
	render_entity_basis EntityBasis;
	v4 Color;
	v2 Dim;
};

// TODO: This is dumb, this should gust be just be part of
// the renderer pushbuffer - add correction of coordinates
// in there and be done with it.
struct render_group
{
	real32 GlobalAlpha;
    render_basis *DefaultBasis;
    real32 MetersToPixels;

    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8 *PushBufferBase;
};

// NOTE: Render API

#if 0
internal inline void PushBitmap(render_group *Group, loaded_bitmap *Bitmap, v2 Offset, real32 OffsetZ,
	v4 Color = V4(1, 1, 1, 1));
internal inline void PushRect(render_group *Group, v2 Offset, real32 OffsetZ, v2 Dim, v4 Color);
internal inline void PushRectOutline(render_group *Group, v2 Offset, real32 OffsetZ, v2 Dim, v4 Color);
inline void Clear(render_group *Group, v4 Color);
#endif

#define HANDMADE_RENDER_GROUP_H
#endif