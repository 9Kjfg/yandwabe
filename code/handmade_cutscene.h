#if !defined(HANDMADE_CUTSCENE_H)

enum scene_layer_flags
{
    SceneLayerFlag_AtInfinity = 0x1,
    SceneLayerFlag_CounterCameraX = 0x2,
    SceneLayerFlag_CounterCameraY = 0x4,
    SceneLayerFlag_Transient = 0x8,
    SceneLayerFlag_Floaty = 0x16,
};

struct scene_layer
{
    v3 P;
    r32 Height;
    b32 Flags;
    v2 Param;
};

struct layered_scene
{
    asset_type_id AssetType;
    u32 ShotIndex;
    u32 LayerCount;
    scene_layer *Layers;

    v3 CameraStart;
    v3 CameraEnd;
};

#define HANDMADE_CUTSCENE_H
#endif