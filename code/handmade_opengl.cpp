
inline void
OpenGLRectangle(v4 MinP, v4 MaxP, v4 Color)
{
    glBegin(GL_TRIANGLES);

    glColor4(Color.r, Color.g, Color.b, Color.a);

    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(MinP.x, MinP.y);

    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(MaxP.x, MinP.y);

    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(MaxP.x, MaxP.y);
    
    // NOTE: Upper triangle
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(MinP.x, MinP.y);

    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(MaxP.x, MaxP.y);

    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(MinP.x, MaxP.y);

    glEnd();
}

internal void
RenderGroupToOutput(render_group *RenderGroup, loaded_bitmap *OutputTarget,	rectangle2i ClipRect)
{
	TIMED_FUNCTION();

    glViewport(0, 0, OutputTarget->WindowWidth, OutputTarget->WindowHeight);

	u32 SortEntryCount = RenderGroup->PushBufferElementCount;
	tile_sort_entry *SortEntries = (tile_sort_entry *)(RenderGroup->PushBufferBase + RenderGroup->SortEntryAt);

	tile_sort_entry *Entry = SortEntries;
	for (u32 SortEntryIndex = 0;
		SortEntryIndex < SortEntryCount;
		++SortEntryIndex, ++Entry)
	{
		render_group_entry_header *Header = (render_group_entry_header *)
            (RenderGroup->PushBufferBase + Entry->PushBufferOffset);
		
		void *Data = (uint8 *)Header + sizeof(*Header);
        switch (Header->Type)
        {
            case RenderGroupEntryType_render_entry_clear:
            {
                render_entry_clear *Entry = (render_entry_clear *)Data;

                glClearColor(Entry->Color.r, Entry->Color.g, Entry->Color.b, Entry->Color.a);
                glClear(GL_COLOR_BUFFER_BIT);
            } break;

            case RenderGroupEntryType_render_entry_bitmap:
            {
                render_entry_bitmap *Entry = (render_entry_bitmap *)Data;
				Assert(Entry->Bitmap);

                v2 XAxis = {1.0f, 0.0f};
                v2 YAxis = {0.0f, 1.0f};
                v2 MinP = Entry->P;
                v2 MaxP = MinP + Entry->Size.x*XAxis + Entry->Size.y*YAxis;

                OpenGLRectangle(MinP, MaxP, Color);
            } break;

            case RenderGroupEntryType_render_entry_rectangle:
            {
                render_entry_rectangle *Entry = (render_entry_rectangle *)Data;
                OpenGLRectangle(Entry->P, Entry->P + Entry->Dim, Entry->Color);                
            } break;
			
            case RenderGroupEntryType_render_entry_cordinate_system:
            {
                render_entry_cordinate_system *Entry = (render_entry_cordinate_system *)Data;
            } break;

			InvalidDefaultCase;
		}
	}
}