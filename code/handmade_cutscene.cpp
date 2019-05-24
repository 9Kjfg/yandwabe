
internal void
RenderCutscene(game_assets *Assets, render_group *RenderGroup, loaded_bitmap *DrawBuffer,
    r32 tCutScene)
{
    // TODO: Unify this stuff
    r32 WidthOfMonitor = 0.635; // NOTE: Horizontal measurement of monitor in meters
	r32 MetersToPixels = (r32)DrawBuffer->Width*WidthOfMonitor;

    r32 FocalLength = 0.6;

    r32 tStart = 0.0f;
    r32 tEnd = 5.0f;

    r32 tNormal = Clamp01MapToRange(tStart, tCutScene, tEnd);

    v3 CameraStart = {0, 0, 0};
    v3 CameraEnd = {-4.0f, -2.0f, -1.0f};
    v3 CameraOffset = Lerp(CameraStart, tNormal, CameraEnd);
	r32 DistanceAboveGround = 10.0f - tNormal*5.0f;
	Perspective(RenderGroup, DrawBuffer->Width, DrawBuffer->Height, MetersToPixels, FocalLength, DistanceAboveGround);

    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
    WeightVector.E[Tag_ShotIndex] = 10.0f;
    WeightVector.E[Tag_LayerIndex] = 1.0f;

    int ShotIndex = 1;
    MatchVector.E[Tag_ShotIndex] = (r32)ShotIndex;

    v4 LayerPlacement[] = 
    {
        {0, 0, DistanceAboveGround - 200.0f, 300.0f}, // NOTE: Sky background
        {0, 0, -200.0f, 300.0f}, // NOTE: Weird sky light
        {0, 10.0f, -70.0f, 80.0f},
        {0, 0, 0, 14.0f},
        {0, 0, 0, 14.0f},
    };

    for (u32 LayerIndex = 1;
        LayerIndex <= 3;
        ++LayerIndex)
    {
        v4 Placement = LayerPlacement[LayerIndex - 1];
        RenderGroup->Transform.OffsetP = Placement.xyz - CameraOffset;
        MatchVector.E[Tag_LayerIndex] = (r32)LayerIndex;
        bitmap_id LayerImage = GetBestMatchBitmapFrom(Assets, Asset_OpeningCutscene, &MatchVector, &WeightVector);
        PushBitmap(RenderGroup, LayerImage, Placement.w, V3(0, 0, 0));
    }
}