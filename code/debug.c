/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

static void Debug_Text_Line(text_context *Text, char *Format, ...)
{
   Advance_Text_Line(Text->Font, Text->Size, 1.0f/Text->Renderer->Pixels_Per_Meter, &Text->Y);

   char Data[128];
   string String = {0};
   String.Data = (u8 *)Data;

   va_list Arguments;
   va_start(Arguments, Format);
   String.Length = vsnprintf(Data, sizeof(Data), Format, Arguments);
   va_end(Arguments);

   Push_Text(Text->Renderer, Text->Font, Text->Size, Text->X, Text->Y, String);
}

static void Display_Debug_Overlay(game_state *Game_State, game_input *Input, renderer *Renderer, float Frame_Seconds)
{
   vec2 Screen_Dim = {Renderer->Screen_Width_Meters, Renderer->Screen_Height_Meters};
   vec2 Screen_Center = Mul2(Screen_Dim, 0.5f);

   float Start_X = -Screen_Center.X + 0.5f;
   float Start_Y = -Screen_Center.Y;

   text_context Text = Begin_Text(Renderer, Start_X, Start_Y, &Game_State->Varia_Font, Text_Size_Large);
   Debug_Text_Line(&Text, "Dungeon Simulator");

   Text.Font = &Game_State->Fixed_Font;
   Text.Size = Text_Size_Medium;
   Debug_Text_Line(&Text, "Frame Time: %3.3fms", Frame_Seconds*1000.0f);
   Debug_Text_Line(&Text, "Mouse: {%0.2f, %0.2f}", Input->Binormal_Mouse_X, Input->Binormal_Mouse_Y);
#if 0
   Debug_Text_Line(&Text, "Permanent: %zuMB", (Game_State->Permanent.End - Game_State->Permanent.Begin) / Megabytes(1));
   Debug_Text_Line(&Text, "Map: %zuMB", (Game_State->Map.Arena.End - Game_State->Map.Arena.Begin) / Megabytes(1));
   Debug_Text_Line(&Text, "Scratch: %zuMB", (Game_State->Scratch.End - Game_State->Scratch.Begin) / Megabytes(1));
   if(Game_State->Selected_Debug_Entity_ID)
   {
      entity *Selected = Get_Entity(Game_State, Game_State->Selected_Debug_Entity_ID);
      string Name = Entity_Type_Names[Selected->Type];
      Debug_Text_Line(&Text, "[SELECTED] %.*s: {X:%d, Y:%d, Z:%d, W:%d, H:%d}",
                             (int)Name.Length, Name.Data,
                             Selected->Position.X,
                             Selected->Position.Y,
                             Selected->Position.Z,
                             Selected->Width,
                             Selected->Height);
   }

   Debug_Text_Line(&Text, "");
   Debug_Text_Line(&Text, "Cycle Counts:");
#endif
   Text.Size = Text_Size_Small;
   for(int Profile_Index = 0; Profile_Index < Array_Count(Debug_Profiler.Profiles); ++Profile_Index)
   {
      debug_profile *Profile = Debug_Profiler.Profiles + Profile_Index;
      if(Profile->Name)
      {
         Debug_Text_Line(&Text, "% 20s: % 10ld avg over %d hit(s)", Profile->Name, Profile->Elapsed/Profile->Hits, Profile->Hits);
      }
   }
   Zero_Size(&Debug_Profiler, sizeof(Debug_Profiler));

#if 0
   Text.Size = Text_Size_Medium;
   Debug_Text_Line(&Text, "");
   Debug_Text_Line(&Text, "Entities: (%d Total)", Game_State->Entity_Count);

   Text.Size = Text_Size_Small;
   entity *Camera = Get_Entity(Game_State, Game_State->Camera_ID);
   map_chunk *Chunk = Get_Map_Chunk(&Game_State->Map, Camera->Position.X, Camera->Position.Y, Camera->Position.Z);
   if(Chunk)
   {
      for(map_chunk_entities *Entities = Chunk->Entities; Entities; Entities = Entities->Next)
      {
         for(int Index = 0; Index < Entities->Index_Count; ++Index)
         {
            int Entity_Index = Entities->Indices[Index];
            entity *Entity = Game_State->Entities + Entity_Index;
            if(Is_Active(Entity))
            {
               switch(Entity->Type)
               {
                  case Entity_Type_Null:
                  case Entity_Type_Wall:
                  case Entity_Type_Floor: {
                  } break;

                  default: {
                     string Name = Entity_Type_Names[Entity->Type];
                     Debug_Text_Line(&Text, "[%d] %.*s: {%d, %d, %d}", Entity_Index, (int)Name.Length, Name.Data,
                               Entity->Position.X, Entity->Position.Y, Entity->Position.Z);
                  } break;
               }
            }
         }
      }
   }

   Text.Size = Text_Size_Medium;
   Debug_Text_Line(&Text, "");
   Debug_Text_Line(&Text, "Audio Tracks:");

   Text.Size = Text_Size_Small;
   int Track_Index = 0;
   for(audio_track *Track = Game_State->Audio_Tracks; Track; Track = Track->Next)
   {
      Assert(Array_Count(Track->Volume) == 2);
      Debug_Text_Line(&Text, "Track %d (%s): Volume: [%0.1f %0.1f], Samples Left: %d", Track_Index++,
                      (Track->Playback == Audio_Playback_Loop) ? "loop" : "once",
                      Track->Volume[0], Track->Volume[1],
                      Track->Sound->Sample_Count - Track->Sample_Index
                  );
   }
#endif
   int Screen_Half_Width = Renderer->Screen_Width_Meters * 0.5f;
   int Screen_Half_Height = Renderer->Screen_Height_Meters * 0.5f;

   int Gui_Dim = 1;
   int Gui_X = Screen_Half_Width - 2*Gui_Dim*GAME_CONTROLLER_COUNT;
   int Gui_Y = Gui_Dim - Screen_Half_Height;

   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      game_controller *Controller = Input->Controllers + Controller_Index;

      if(Controller->Connected)
      {
         Push_Rectangle(Renderer, Render_Layer_UI, Gui_X, Gui_Y, Gui_Dim, Gui_Dim, Vec4(0, 1, 0, 1));
      }
      else
      {
         Push_Rectangle(Renderer, Render_Layer_UI, Gui_X, Gui_Y, Gui_Dim, Gui_Dim, Vec4(0, 0.25, 0, 1));
         Push_Outline(Renderer, Render_Layer_UI, Gui_X, Gui_Y, Gui_Dim, Gui_Dim, 0.2f, Vec4(0, 1, 0, 1));
      }
      Gui_X += (2 * Gui_Dim);
   }
}
