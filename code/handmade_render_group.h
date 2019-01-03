#if !defined(HANDMADE_RENDER_GROUP_H)

struct render_basis
{
    v3 P;
};

struct render_entity_basis
{
	render_basis *Basis;
	v2 Offset;
	real32 OffsetZ;
	real32 EntityZC;
};

// NOTE: render_group_entry is a "compact efficient disriminated uion"
// TODO: Remove the header
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
	render_group_entry_header Header;
	v4 Color;
};

struct render_entry_cordinate_system
{
	render_group_entry_header Header;
	v2 Origin;
	v2 XAxis;
	v2 YAxis;
	v4 Color;
	loaded_bitmap *Texture;
};

struct render_entry_bitmap
{
	render_group_entry_header Header;
	render_entity_basis EntityBasis;
	loaded_bitmap *Bitmap;
	real32 R, G, B, A;
};

struct render_entry_rectangle
{
	render_group_entry_header Header;
	render_entity_basis EntityBasis;
	real32 R, G, B, A;
	v2 Dim;
};

// TODO: This is dumb, this should gust be just be part of
// the renderer pushbuffer - add correction of coordinates
// in there and be done with it.
struct render_group
{
    render_basis *DefaultBasis;
    real32 MetersToPixels;

    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8 *PushBufferBase;
};


#define HANDMADE_RENDER_GROUP_H
#endif