

#define CUTSCENE_WARMUP_SECONDS 2.0f

internal void
RenderLayerdScene(game_assets *Assets, render_group *RenderGroup, loaded_bitmap *DrawBuffer,
    layered_scene *Scene, r32 tNormal)
{
    r32 WidthOfMonitor = 0.635; // NOTE: Horizontal measurement of monitor in meters
	r32 MetersToPixels = (r32)DrawBuffer->Width*WidthOfMonitor;
    r32 FocalLength = 0.6;

    r32 SceneFadeValue = 1.0f;
    if (tNormal < Scene->tFadeIn)
    {
        SceneFadeValue = Clamp01MapToRange(0.0f, tNormal, Scene->tFadeIn);
    }

    v4 Color = {SceneFadeValue, SceneFadeValue, SceneFadeValue, 1.0f};

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

                PushBitmap(RenderGroup, LayerImage, Layer.Height, V3(0, 0, 0), Color);
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
    {{0, 0, -200.0f}, 300.0f, SceneLayerFlag_AtInfinity, 0.1f},
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
    game_mode_cutscene *CutScene, r32 tCutScene)
{
    b32 CutsceneStillRunning = false;

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
            CutsceneStillRunning = true;
        }

        tBase = tEnd;
    }

    return(CutsceneStillRunning);
}

internal b32
CheckForMetaInput(game_state *GameState, transient_state *TranState, game_input *Input)
{
    b32 Result = false;
    for (int ControllerIndex = 0;
		ControllerIndex < ArrayCount(Input->Controllers);
		++ControllerIndex)
	{
		game_controller_input *Controller = GetController(Input, ControllerIndex);
		controlled_hero *ConHero = GameState->ControlledHeroes + ControllerIndex;
        if (WasPressed(Controller->Back))
        {
            Input->QuitRequested = true;
            break;
        }
        else if (WasPressed(Controller->Start))
        {
            PlayWorld(GameState, TranState);
            Result = true;
            break;
        }
    }

    return(Result);
}

internal b32
UpdateAndRenderCutScene(game_state *GameState, transient_state *TranState, render_group *RenderGroup, loaded_bitmap *DrawBuffer,
    game_input *Input, game_mode_cutscene *CutScene)
{
    game_assets *Assets = TranState->Assets;

    b32 Result = CheckForMetaInput(GameState, TranState, Input);
    if (!Result)
    {
        RenderCutsceneAtTime(Assets, 0, DrawBuffer, CutScene, CutScene->t + CUTSCENE_WARMUP_SECONDS);
        b32 CutsceneStillRunning = RenderCutsceneAtTime(Assets, RenderGroup, DrawBuffer, CutScene, CutScene->t);
        if (CutsceneStillRunning)
        {
            CutScene->t += Input->dtForFrame;
        }
        else
        {
            PlayTitleScreen(GameState, TranState);
        }
    }

    return(Result);
}

internal b32
UpdateAndRenderTitleScreen(game_state *GameState, transient_state *TranState, render_group *RenderGroup, loaded_bitmap *DrawBuffer,
    game_input *Input, game_mode_title_screen *TitleScreen)
{
    game_assets *Assets = TranState->Assets;

    b32 Result = CheckForMetaInput(GameState, TranState, Input);
    if (!Result)
    {
        Clear(RenderGroup, V4(1.0f, 0.25f, 0.25f, 0.0f));
        if (TitleScreen->t > 10.0f)
        {
            PlayIntroCutScene(GameState, TranState);
        }
        else
        {
            TitleScreen->t += Input->dtForFrame;
        }
    }

    return(Result);
}

internal void
PlayIntroCutScene(game_state *GameState, transient_state *TranState)
{
    SetGameMode(GameState, TranState, GameMode_CutScene);

    game_mode_cutscene *Result = PushStruct(&GameState->ModeArena, game_mode_cutscene);

    Result->SceneCount = ArrayCount(IntroCutscene);
    Result->Scenes = IntroCutscene;
    Result->t = 0;

    GameState->CutScene = Result;
}

internal void
PlayTitleScreen(game_state *GameState, transient_state *TranState)
{
    SetGameMode(GameState, TranState, GameMode_TitleScreen);

    game_mode_title_screen *Result = PushStruct(&GameState->ModeArena, game_mode_title_screen);

    Result->t = 0;

    GameState->TitleScreen = Result;
}