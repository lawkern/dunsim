/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

static void Advance_Text_Line(text_font *Font, text_size Size, int *Y)
{
   float Scale = Font->Glyphs[Size].Scale;

   float Line_Advance = Scale * (Font->Ascent - Font->Descent + Font->Line_Gap);
   *Y += Line_Advance;
}

static int Get_Text_Width(text_font *Font, text_size Size, string Text)
{
   int Result = 0;

   float Scale = Font->Glyphs[Size].Scale;
   for(size Index = 0; Index < Text.Length; ++Index)
   {
      if(Index != Text.Length-1)
      {
         int C0 = Text.Data[Index + 0];
         int C1 = Text.Data[Index + 1];

         Result += (Scale * Font->Distances[(C0 * GLYPH_COUNT) + C1]);
      }
   }

   return(Result);
}

typedef struct {
   renderer *Renderer;
   int X;
   int Y;

   text_font *Font;
   text_size Size;
} text_context;

static text_context Begin_Text(renderer *Renderer, int X, int Y, text_font *Font, text_size Size)
{
   text_context Result = {0};
   Result.Renderer = Renderer;
   Result.X = X;
   Result.Y = Y;
   Result.Font = Font;
   Result.Size = Size;

   return(Result);
}

static void Debug_Text_Line(text_context *Text, char *Format, ...)
{
   Advance_Text_Line(Text->Font, Text->Size, &Text->Y);

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
   int Start_X = Renderer->Pixels_Per_Meter / 2;
   int Start_Y = 0;

   text_context Text = Begin_Text(Renderer, Start_X, Start_Y, &Game_State->Varia_Font, Text_Size_Large);
   Debug_Text_Line(&Text, "Dungeon Simulator");

   Text.Font = &Game_State->Fixed_Font;
   Text.Size = Text_Size_Medium;
   Debug_Text_Line(&Text, "Frame Time: %3.3fms", Frame_Seconds*1000.0f);
   Debug_Text_Line(&Text, "Permanent: %zuMB", (Game_State->Permanent.End - Game_State->Permanent.Begin) / Megabytes(1));
   Debug_Text_Line(&Text, "Map: %zuMB", (Game_State->Map.Arena.End - Game_State->Map.Arena.Begin) / Megabytes(1));
   Debug_Text_Line(&Text, "Scratch: %zuMB", (Game_State->Scratch.End - Game_State->Scratch.Begin) / Megabytes(1));
   Debug_Text_Line(&Text, "Mouse: {%0.2f, %0.2f}", Input->Mouse_X, Input->Mouse_Y);
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
   Debug_Text_Line(&Text, "Total Entities: %d", Game_State->Entity_Count);

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

   Debug_Text_Line(&Text, "");
   Debug_Text_Line(&Text, "Audio Tracks:");

   int Track_Index = 0;
   for(audio_track *Track = Game_State->Audio_Tracks; Track; Track = Track->Next)
   {
      Debug_Text_Line(&Text, "[%d] %d samples left %s", Track_Index++, Track->Sound->Sample_Count - Track->Sample_Index,
                      (Track->Playback == Audio_Playback_Loop) ? "(looping)" : "");
   }

   int Gui_Dim = Renderer->Pixels_Per_Meter;
   int Gui_X = Renderer->Backbuffer.Width - 2*Gui_Dim*GAME_CONTROLLER_COUNT;
   int Gui_Y = Gui_Dim;
   int Border_Pixels = Maximum(1, Renderer->Pixels_Per_Meter / 16);

   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      game_controller *Controller = Input->Controllers + Controller_Index;

      if(Controller->Connected)
      {
         Push_Rectangle(Renderer, Render_Layer_UI, Gui_X, Gui_Y, Gui_Dim, Gui_Dim, 0x00FF00FF);
      }
      else
      {
         Push_Rectangle(Renderer, Render_Layer_UI, Gui_X, Gui_Y, Gui_Dim, Gui_Dim, 0x004400FF);
         Push_Outline(Renderer, Render_Layer_UI, Gui_X, Gui_Y, Gui_Dim, Gui_Dim, 2*Border_Pixels, 0x00FF00FF);
      }
      Gui_X += (2 * Gui_Dim);
   }
}
