

#define CUTSCENE_WARMUP_SECONDS 2.0f

internal void
RenderLayerdScene(game_assets *Assets, render_group *RenderGroup, loaded_bitmap *DrawBuffer,
    layered_scene *Scene, r32 tNormal)
{
    r32 WidthOfMonitor = 0.635; // NOTE: Horizontal measurement of monitor in meters
	r32 MetersToPixels = (r32)DrawBuffer->Width*WidthOfMonitor;
    r32 FocalLength = 0.6;

    v3 CameraStart = Scene->CameraStart;
    v3 CameraEnd = Scene->CameraEnd;
    v3 CameraOffset = Lerp(CameraStart, tNormal, CameraEnd);
    if (RenderGroup)
    {
	    Perspective(RenderGroup, DrawBuffer->Width, DrawBuffer->Height, MetersToPixels, FocalLength, 0);
    }

    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
    WeightVector.E[Tag_ShotIndex] = 10.0f;
    WeightVector.E[Tag_LayerIndex] = 1.0f;

    MatchVector.E[Tag_ShotIndex] = (r32)Scene->ShotIndex;

    if (Scene->LayerCount == 0)
    {
        Clear(RenderGroup, V4(0.0f, 0.0f, 0.0f, 0.0f));
    }

    for (u32 LayerIndex = 1;
        LayerIndex <= Scene->LayerCount;
        ++LayerIndex)
    {
        scene_layer Layer = Scene->Layers[LayerIndex - 1];
        b32 Active = true;
        if (Layer.Flags & SceneLayerFlag_Transient)
        {
            Active = ((tNormal >= Layer.Param.x) && (tNormal < Layer.Param.y));
        }


        if (Active)
        {
            MatchVector.E[Tag_LayerIndex] = (r32)LayerIndex;
            bitmap_id LayerImage = GetBestMatchBitmapFrom(Assets, Scene->AssetType, &MatchVector, &WeightVector);

            if (RenderGroup)
            {
                v3 P = Layer.P;
                if (Layer.Flags & SceneLayerFlag_AtInfinity)
                {
                    P.z += CameraOffset.z;
                }

                if (Layer.Flags & SceneLayerFlag_Floaty)
                {
                    P.y += Layer.Param.x * Sin(Layer.Param.y*tNormal);
                }

                if (Layer.Flags & SceneLayerFlag_CounterCameraX)
                {
                    RenderGroup->Transform.OffsetP.x = P.x + CameraOffset.x;
                }
                else
                {
                    RenderGroup->Transform.OffsetP.x = P.x - CameraOffset.x;
                }

                if (Layer.Flags & SceneLayerFlag_CounterCameraY)
                {
                    RenderGroup->Transform.OffsetP.y = P.y + CameraOffset.y;
                }
                else
                {
                    RenderGroup->Transform.OffsetP.y = P.y - CameraOffset.y;
                }

                RenderGroup->Transform.OffsetP.z = P.z - CameraOffset.z;

                RenderGroup->Transform.OffsetP = P - CameraOffset;
                RenderGroup->Transform.OffsetP = P - CameraOffset;
                PushBitmap(RenderGroup, LayerImage, Layer.Height, V3(0, 0, 0));
            }
            else
            {
                PrefetchBitmap(Assets, LayerImage);
            }
        }
    }
}

global_variable scene_layer IntroLayers1[] = 
{
    {{0, 0, -200.0f}, 300.0f, SceneLayerFlag_AtInfinity},
    {{0, 0, -200.0f}, 300.0f},
    {{0, 10.0f, -70.0f}, 80.0f},
    {{0, 0, 0}, 14.0f},
    {{0, 0, 0}, 14.0f},
};

global_variable scene_layer IntroLayers2[] = 
{
    {{0, 0, -200.0f}, 300.0f, SceneLayerFlag_AtInfinity},
    {{0, 0, -200.0f}, 300.0f},
    {{0, 10.0f, -70.0f}, 80.0f},
    {{0, 0, -20.0f}, 14.0f},
    {{0, 0, -20.0f}, 14.0f},
};

#define INTRO_SHOT(Index) Asset_OpeningCutscene, Index, ArrayCount(IntroLayers##Index), IntroLayers##Index
layered_scene IntroCutscene[]
{
    {Asset_None, 0, 0, 0, CUTSCENE_WARMUP_SECONDS},
    {INTRO_SHOT(1), 20.0f, {0, 0, 10.0f}, {-4.0f, -2.0f, 5.0f}},
    {INTRO_SHOT(2), 20.0f, {0, 0, 10.0f}, {-4.0f, -2.0f, 5.0f}},
};

internal b32
RenderCutsceneAtTime(game_assets *Assets, render_group *RenderGroup, loaded_bitmap *DrawBuffer,
    playing_cutscene *CutScene, r32 tCutScene)
{
    b32 CutsceneComplete = false;

    r32 tBase = 0.0f;
    for (u32 ShotIndex = 0;
        ShotIndex < CutScene->SceneCount;
        ++ShotIndex)
    {
        layered_scene *Scene = CutScene->Scenes + ShotIndex;
        r32 tStart = tBase;
        r32 tEnd = tStart + Scene->Duration;

        if ((tCutScene >= tStart) && (tCutScene < tEnd))
        {
            r32 tNormal = Clamp01MapToRange(tStart, tCutScene, tEnd);
            RenderLayerdScene(Assets, RenderGroup, DrawBuffer, &IntroCutscene[ShotIndex], tNormal);
            CutsceneComplete = true;
        }

        tBase = tEnd;
    }

    return(CutsceneComplete);
}

internal b32
RenderCutscene(game_assets *Assets, render_group *RenderGroup, loaded_bitmap *DrawBuffer,
    playing_cutscene *CutScene)
{
    RenderCutsceneAtTime(Assets, 0, DrawBuffer, CutScene, CutScene->t + CUTSCENE_WARMUP_SECONDS);
    b32 CutsceneComplete = RenderCutsceneAtTime(Assets, RenderGroup, DrawBuffer, CutScene, CutScene->t);
    if (!CutsceneComplete)
    {
        CutScene->t = 0.0f;
    }

    return(CutsceneComplete);
}

internal void
AdvanceCutscene(playing_cutscene *Cutscene, r32 dt)
{
    Cutscene->t += dt;
}

internal playing_cutscene
MakeIntroCutScene(void)
{
    playing_cutscene Result = {};

    Result.SceneCount = ArrayCount(IntroCutscene);
    Result.Scenes = IntroCutscene;

    return(Result);
}