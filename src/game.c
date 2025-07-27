/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include "game.h"

#include "intrinsics.c"
#include "math.c"
#include "random.c"
#include "assets.c"
#include "map.c"
#include "entity.c"
#include "render.c"

typedef enum {
   Audio_Playback_Once,
   Audio_Playback_Loop,
} audio_playback;

typedef struct audio_track audio_track;
struct audio_track
{
   game_sound *Sound;
   audio_track *Next;

   int Sample_Index;
   audio_playback Playback;
};

typedef struct {
   arena Permanent;
   arena Scratch;
   random_entropy Entropy;

   text_font Varia_Font;
   text_font Fixed_Font;

   int Active_Textbox_Index;
   string Textbox_Dialogue[4];

   texture Upstairs;
   texture Downstairs;

   game_sound Background_Music;
   game_sound Clap;

   map Map;

   int Entity_Count;
   entity Entities[1024*1024];

   int Selected_Debug_Entity_ID;
   int Camera_ID;
   int Player_IDs[GAME_CONTROLLER_COUNT];

   audio_track *Audio_Tracks;
   audio_track *Free_Audio_Tracks;
} game_state;

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

static void Push_Text(renderer *Renderer, text_font *Font, text_size Size, int X, int Y, string Text)
{
   if(Font->Loaded)
   {
      text_glyphs *Glyphs = Font->Glyphs + Size;
      float Scale = Glyphs->Scale;

      for(size Index = 0; Index < Text.Length; ++Index)
      {
         int Codepoint = Text.Data[Index];

         texture Glyph = Glyphs->Bitmaps[Codepoint];
         Push_Texture(Renderer, Render_Layer_UI, Glyph, X, Y);

         if(Index != Text.Length-1)
         {
            int Next_Codepoint = Text.Data[Index + 1];
            int Pair_Index = (Codepoint * GLYPH_COUNT) + Next_Codepoint;
            X += (Scale * Font->Distances[Pair_Index]);
         }
      }
   }
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

static void Text_Line(text_context *Text, char *Format, ...)
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

static void Display_Textbox(game_state *Game_State, renderer *Renderer, string Text)
{
   text_font *Font = &Game_State->Varia_Font;
   if(Font->Loaded)
   {
      text_size Size = Text_Size_Medium;
      text_glyphs *Glyphs = Font->Glyphs + Size;

      float Scale = Glyphs->Scale;
      float Line_Advance = Scale * (Font->Ascent - Font->Descent + Font->Line_Gap);

      int Margin = 10;
      int Padding = 25;

      int Line_Count = 1;
      int Max_Line_Count = 6;

      int Box_Width = Renderer->Backbuffer.Width - 2*Margin;
      int Box_Height = Max_Line_Count*Line_Advance + 2*Padding;
      int Box_X = Margin;
      int Box_Y = Renderer->Backbuffer.Height - Margin - Box_Height;
      Push_Rectangle(Renderer, Render_Layer_UI, Box_X, Box_Y, Box_Width, Box_Height, 0x000000FF);

      int Text_X = Margin + Padding;
      int Text_Y = Box_Y + Padding + Scale*Font->Ascent;

      cut Words = {0};
      Words.After = Text;
      while(Words.After.Length)
      {
         Words = Cut(Words.After, ' ');
         string Word = Words.Before;

         int Width = Get_Text_Width(Font, Size, Word);
         if((Box_X + Box_Width - Padding) < (Text_X + Width))
         {
            Text_X = Margin + Padding;
            Advance_Text_Line(Font, Size, &Text_Y);
            Line_Count++;
         }

         for(int Index = 0; Index < Word.Length; ++Index)
         {
            int Codepoint = Word.Data[Index];
            if(Codepoint == '\n')
            {
               Text_X = Margin + Padding;
               Advance_Text_Line(Font, Size, &Text_Y);
               Line_Count++;
            }
            else
            {
               texture Glyph = Glyphs->Bitmaps[Codepoint];
               Push_Texture(Renderer, Render_Layer_UI, Glyph, Text_X, Text_Y);

               int Next_Codepoint = (Index != Word.Length-1) ? Word.Data[Index + 1] : ' ';
               int Pair_Index = (Codepoint * GLYPH_COUNT) + Next_Codepoint;
               Text_X += (Scale * Font->Distances[Pair_Index]);
            }
         }

         if(Words.After.Length)
         {
            int Next_Codepoint = Words.After.Data[0];
            Text_X += (Scale * Font->Distances[(' ' * GLYPH_COUNT) + Next_Codepoint]);
         }
      }

      Assert(Line_Count <= Max_Line_Count);
   }
}

static bool Can_Move(game_state *Game_State, entity *Entity, int Delta_X, int Delta_Y)
{
   bool Result = true;

   int3 Old_P = Entity->Position;
   int3 New_P = Entity->Position;
   New_P.X += Delta_X;
   New_P.Y += Delta_Y;

   // TODO: Handle diagonal cross-chunk boundaries.
   map_chunk *Old_Chunk = Query_Map_Chunk(&Game_State->Map, Old_P.X, Old_P.Y, Old_P.Z);
   map_chunk *New_Chunk = Query_Map_Chunk(&Game_State->Map, New_P.X, New_P.Y, New_P.Z);

   if(!(Old_Chunk && New_Chunk))
   {
      Result = false;
   }
   else
   {
      rectangle Entity_Rect = To_Rectangle(New_P.X, New_P.Y, Entity->Width, Entity->Height);

      int Chunk_Count = (Old_Chunk == New_Chunk) ? 1 : 2;
      map_chunk *Chunks[] = {Old_Chunk, New_Chunk};

      for(int Chunk_Index = 0; Result && (Chunk_Index < Chunk_Count); ++Chunk_Index)
      {
         map_chunk *Chunk = Chunks[Chunk_Index];
         for(map_chunk_entities *Entities = Chunk->Entities; Entities; Entities = Entities->Next)
         {
            for(int Index = 0; Index < Entities->Index_Count; ++Index)
            {
               entity *Test = Game_State->Entities + Entities->Indices[Index];
               if(Test != Entity && Has_Collision(Test))
               {
                  rectangle Test_Rect = To_Rectangle(Test->Position.X, Test->Position.Y, Test->Width, Test->Height);
                  if(Rectangles_Intersect(Entity_Rect, Test_Rect))
                  {
                     Result = false;
                     break;
                  }
               }
            }
         }
      }
   }

   return(Result);
}

static void Update_Entity_Chunk(int Entity_Index, map_chunk *Old, map_chunk *New, arena *Arena)
{
   if(Old)
   {
      // TODO: We can probably do better than scanning the blocks linearly.
      bool Removed = false;
      for(map_chunk_entities *Entities = Old->Entities; !Removed && Entities; Entities = Entities->Next)
      {
         for(int Index = 0; Index < Entities->Index_Count; ++Index)
         {
            if(Entities->Indices[Index] == Entity_Index)
            {
               Entities->Indices[Index] = Entities->Indices[Entities->Index_Count - 1];
               Entities->Index_Count--;

               Removed = true;
               break;
            }
         }
      }
      Assert(Removed);
   }

   map_chunk_entities *Entities = New->Entities;
   if(!Entities || Entities->Index_Count == Array_Count(Entities->Indices))
   {
      map_chunk_entities *New_Entities = Allocate(Arena, map_chunk_entities, 1);
      New_Entities->Next = Entities;
      New->Entities = Entities = New_Entities;
   }
   Entities->Indices[Entities->Index_Count++] = Entity_Index;
}

static int Create_Entity(game_state *Game_State, entity_type Type, int Width, int Height, int X, int Y, int Z, u32 Flags)
{
   Assert(Game_State->Entity_Count != Array_Count(Game_State->Entities));
   int ID = Game_State->Entity_Count++;

   entity *Entity = Game_State->Entities + ID;
   Entity->Type = Type;
   Entity->Width = Width;
   Entity->Height = Height;
   Entity->Position.X = X;
   Entity->Position.Y = Y;
   Entity->Position.Z = Z;
   Entity->Flags = Flags;

   map *Map = &Game_State->Map;
   map_chunk *Chunk = Insert_Map_Chunk(Map, X, Y, Z);
   Update_Entity_Chunk(ID, 0, Chunk, &Map->Arena);

   return(ID);
}

static entity *Get_Entity(game_state *Game_State, int ID)
{
   Assert(ID > 0);
   Assert(ID < Array_Count(Game_State->Entities));

   entity *Result = Game_State->Entities + ID;
   return(Result);
}

static bool Move(game_state *Game_State, entity *Entity, int Delta_X, int Delta_Y)
{
   bool Ok = false;

   animation *Animation = &Entity->Animation;
   int3 *Position = &Entity->Position;

   if(Delta_X && Delta_Y)
   {
      // TODO: This includes redundant checks just to support sliding along
      // walls when moving into them diagonally. This could be much simpler.
      if(Animation->Direction == Direction_Right || Animation->Direction == Direction_Left)
      {
         Ok = Can_Move(Game_State, Entity, 0, Delta_Y);
         if(Ok)
         {
            Delta_X = 0;
         }
         else
         {
            Ok = Can_Move(Game_State, Entity, Delta_X, 0);
            Delta_Y = 0;
         }
      }
      else
      {
         Ok = Can_Move(Game_State, Entity, Delta_X, 0);
         if(Ok)
         {
            Delta_Y = 0;
         }
         else
         {
            Ok = Can_Move(Game_State, Entity, 0, Delta_Y);
            Delta_X = 0;
         }
      }
   }
   else
   {
      Ok = Can_Move(Game_State, Entity, Delta_X, Delta_Y);
   }

   if     (Delta_X > 0) Animation->Direction = Direction_Right;
   else if(Delta_X < 0) Animation->Direction = Direction_Left;
   else if(Delta_Y > 0) Animation->Direction = Direction_Down;
   else if(Delta_Y < 0) Animation->Direction = Direction_Up;
   else                 Animation->Direction = Direction_None;

   if(Ok && (Delta_X || Delta_Y))
   {
      Animation->Offset_X = (float)Delta_X;
      Animation->Offset_Y = (float)Delta_Y;

      map_chunk *Old_Chunk = Query_Map_Chunk(&Game_State->Map, Position->X, Position->Y, Position->Z);
      Position->X += Delta_X;
      Position->Y += Delta_Y;
      map_chunk *New_Chunk = Query_Map_Chunk(&Game_State->Map, Position->X, Position->Y, Position->Z);

      if(Old_Chunk != New_Chunk)
      {
         int ID = Entity - Game_State->Entities;
         Update_Entity_Chunk(ID, Old_Chunk, New_Chunk, &Game_State->Map.Arena);
      }
   }

   return(Ok);
}

static void Advance_Animation(animation *Animation, float Frame_Seconds, float Pixels_Per_Second)
{
   float Delta = Pixels_Per_Second * Frame_Seconds;

   if(Animation->Offset_X > 0)
   {
      Animation->Offset_X -= Delta;
      if(Animation->Offset_X < 0) Animation->Offset_X = 0;
   }
   else if(Animation->Offset_X < 0)
   {
      Animation->Offset_X += Delta;
      if(Animation->Offset_X > 0) Animation->Offset_X = 0;
   }

   if(Animation->Offset_Y > 0)
   {
      Animation->Offset_Y -= Delta;
      if(Animation->Offset_Y < 0) Animation->Offset_Y = 0;
   }
   else if(Animation->Offset_Y < 0)
   {
      Animation->Offset_Y += Delta;
      if(Animation->Offset_Y > 0) Animation->Offset_Y = 0;
   }
}

static void Play_Sound(game_state *Game_State, game_sound *Sound, audio_playback Playback)
{
   audio_track *Track;
   if(Game_State->Free_Audio_Tracks)
   {
      Track = Game_State->Free_Audio_Tracks;
      Game_State->Free_Audio_Tracks = Game_State->Free_Audio_Tracks->Next;
   }
   else
   {
      Track = Allocate(&Game_State->Permanent, audio_track, 1);
   }

   Track->Sound = Sound;
   Track->Next = Game_State->Audio_Tracks;
   Track->Sample_Index = 0;
   Track->Playback = Playback;

   Game_State->Audio_Tracks = Track;
}

UPDATE(Update)
{
   game_state *Game_State = (game_state *)Memory.Base;

   arena *Permanent = &Game_State->Permanent;
   arena *Scratch = &Game_State->Scratch;

   random_entropy *Entropy = &Game_State->Entropy;
   map *Map = &Game_State->Map;

   int Backbuffer_Width = Renderer->Backbuffer.Width;
   int Backbuffer_Height = Renderer->Backbuffer.Height;

   int Tile_Count_X = 40;
   int Tile_Pixels = Backbuffer_Width / Tile_Count_X;
   int Tile_Count_Y = Backbuffer_Height / Tile_Pixels;

   if(!Permanent->Begin)
   {
      // Initialize memory.
      Permanent->Begin = Memory.Base + sizeof(*Game_State);
      Permanent->End = Permanent->Begin + Megabytes(64);

      Map->Arena.Begin = Permanent->End;
      Map->Arena.End = Map->Arena.Begin + Megabytes(16);

      Scratch->Begin = Map->Arena.End;
      Scratch->End = Memory.Base + Memory.Size;

      Assert(Scratch->Begin < Scratch->End);

      // Initialize renderer.
      Renderer->Backbuffer.Memory = Allocate(Permanent, u32, Backbuffer_Width*Backbuffer_Height);
      for(int Queue_Index = 0; Queue_Index < Array_Count(Renderer->Queues); ++Queue_Index)
      {
         Renderer->Queues[Queue_Index] = Allocate(Permanent, render_queue, 1);
      }

      // Initialize audio.

      string Test = S("This is a test file.");
      Write_Entire_File(Test.Data, Test.Length, "data/test.txt");

      Game_State->Entropy = Random_Seed(0x13);

      int3 Origin = {8, 8, 0};
      Game_State->Entity_Count++; // Skip null entity.
      for(int Player_Index = 0; Player_Index < GAME_CONTROLLER_COUNT; ++Player_Index)
      {
         u32 Flags = Entity_Flag_Visible|Entity_Flag_Collides;
         Game_State->Player_IDs[Player_Index] = Create_Entity(Game_State, Entity_Type_Player, 2, 2, Origin.X, Origin.Y, Origin.Z, Flags);
      }
      Game_State->Camera_ID = Create_Entity(Game_State, Entity_Type_Camera, 0, 0, Origin.X, Origin.Y, Origin.Z, Entity_Flag_Active);

      u32 Dragon_Flags = Entity_Flag_Active|Entity_Flag_Visible|Entity_Flag_Collides;
      Create_Entity(Game_State, Entity_Type_Dragon, 4, 4, 6, -8, Origin.Z, Dragon_Flags);

      int Chunk_X = 0;
      int Chunk_Y = 0;
      int Chunk_Z = 0;
      movement_direction Direction_Towards_Previous_Room = Direction_None;
      for(int Chunk_Index = 0; Chunk_Index < 10; ++Chunk_Index)
      {
         int X = Chunk_X * MAP_CHUNK_DIM;
         int Y = Chunk_Y * MAP_CHUNK_DIM;
         int Z = Chunk_Z;

         map_chunk *Chunk = Insert_Map_Chunk(Map, X, Y, Z);
         for(int Offset_Y = 0; Offset_Y < MAP_CHUNK_DIM; ++Offset_Y)
         {
            for(int Offset_X = 0; Offset_X < MAP_CHUNK_DIM; ++Offset_X)
            {
               switch(Debug_Map_Chunk[Offset_Y][Offset_X])
               {
                  case 1: { // Floor
                     u32 Flags = Entity_Flag_Active|Entity_Flag_Visible;
                     Create_Entity(Game_State, Entity_Type_Floor, 1, 1, X+Offset_X, Y+Offset_Y, Z, Flags);
                  } break;

                  case 2: { // Wall
                     u32 Flags = Entity_Flag_Active|Entity_Flag_Visible|Entity_Flag_Collides;
                     Create_Entity(Game_State, Entity_Type_Wall, 1, 1, X+Offset_X, Y+Offset_Y, Z, Flags);
                  } break;

                  case 3: { // Stairs
                     u32 Flags = Entity_Flag_Active|Entity_Flag_Visible;
                     Create_Entity(Game_State, Entity_Type_Stairs, 2, 2, X+Offset_X, Y+Offset_Y, Z, Flags);
                  } break;
               }
            }
         }

         movement_direction Direction = Direction_Towards_Previous_Room;
         while(Direction == Direction_Towards_Previous_Room)
         {
            Direction = Random_Range(Entropy, Direction_None, Direction_Count-1);
         }

         switch(Direction)
         {
            case Direction_Up: {
               Chunk_Y -= 1;
               Direction_Towards_Previous_Room = Direction_Down;
            } break;

            case Direction_Down: {
               Chunk_Y += 1;
               Direction_Towards_Previous_Room = Direction_Up;
            } break;

            case Direction_Left: {
               Chunk_X -= 1;
               Direction_Towards_Previous_Room = Direction_Right;
            } break;

            case Direction_Right: {
               Chunk_X += 1;
               Direction_Towards_Previous_Room = Direction_Left;
            } break;

            case Direction_None: {
               Chunk_Z = (Chunk_Z) ? 0 : 1;
            } break;

            default: {
               Assert(0);
            } break;
         }
      }

      Load_Font(&Game_State->Varia_Font, Permanent, *Scratch, "data/Inter.ttf", Tile_Pixels);
      Load_Font(&Game_State->Fixed_Font, Permanent, *Scratch, "data/JetBrainsMono.ttf", Tile_Pixels);
      if(!Game_State->Varia_Font.Loaded)
      {
         Log("During development, make sure to run the program from the project root folder.");
      }

      Game_State->Upstairs = Load_Image(Permanent, "data/upstairs.png");
      Game_State->Downstairs = Load_Image(Permanent, "data/downstairs.png");

      Game_State->Background_Music = Load_Wave(Permanent, *Scratch, "data/bgm.wav");
      Game_State->Clap = Load_Wave(Permanent, *Scratch, "data/clap.wav");
      Play_Sound(Game_State, &Game_State->Background_Music, Audio_Playback_Loop);

      Game_State->Textbox_Dialogue[1] = S(
         "THE FIRST WORLD\n"
         "Within the earth there was fire. "
         "Mankind succumbed to greed and touched the forbidden sun. "
         "The enslaved prayed, and the sun god appeared. "
         "The earth god raged, and with its serpent of hellfire, shrouded the world in death and darkness. "
         "And they will never meet."
         );

      Game_State->Textbox_Dialogue[2] = S(
         "THE SECOND WORLD\n"
         "Within the void there was breath. "
         "The forest god tamed demons and the sun spread the fires of war. "
         "Those of the half-moon dreamed. "
         "Those of the moon dreamed. "
         "Man killed the sun and became god, and the sea god stormed. "
         "And they will never meet."
         );

      Game_State->Textbox_Dialogue[3] = S(
         "THE THIRD WORLD\n"
         "Within the chaos there was emptiness. "
         "The inconvenient remnants recall the promised day and hear the voice of the half-moon. "
         "The sun god dances and laughs, guiding the world to its end. "
         "The sun returns and brings a new morning. "
         "And they will surely meet."
         );
   }

   int Player_Delta_Xs[GAME_CONTROLLER_COUNT] = {0};
   int Player_Delta_Ys[GAME_CONTROLLER_COUNT] = {0};
   int Camera_Delta_X = 0;
   int Camera_Delta_Y = 0;

   // General Input Handling.
   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      game_controller *Controller = Input->Controllers + Controller_Index;
      if(Controller->Connected)
      {
         if(Was_Pressed(Controller->Action_Left))
         {
            Play_Sound(Game_State, &Game_State->Clap, Audio_Playback_Once);
         }

         entity *Player = Get_Entity(Game_State, Game_State->Player_IDs[Controller_Index]);
         Controller->Connected ? Activate_Entity(Player) : Deactivate_Entity(Player);

         if(Was_Pressed(Controller->Action_Up))
         {
            Game_State->Active_Textbox_Index++;
            if(Game_State->Active_Textbox_Index == Array_Count(Game_State->Textbox_Dialogue))
            {
               Game_State->Active_Textbox_Index = 0;
            }
         }

         bool Moving_Camera = Is_Held(Controller->Shoulder_Right);
         if(Moving_Camera)
         {
            int Delta = 4;
            if(Was_Pressed(Controller->Move_Up))    Camera_Delta_Y -= Delta;
            if(Was_Pressed(Controller->Move_Down))  Camera_Delta_Y += Delta;
            if(Was_Pressed(Controller->Move_Left))  Camera_Delta_X -= Delta;
            if(Was_Pressed(Controller->Move_Right)) Camera_Delta_X += Delta;
         }
         else
         {
            int Delta = (Is_Held(Controller->Action_Down)) ? 2 : 1;
            if(Is_Held(Controller->Move_Up))    Player_Delta_Ys[Controller_Index] -= Delta;
            if(Is_Held(Controller->Move_Down))  Player_Delta_Ys[Controller_Index] += Delta;
            if(Is_Held(Controller->Move_Left))  Player_Delta_Xs[Controller_Index] -= Delta;
            if(Is_Held(Controller->Move_Right)) Player_Delta_Xs[Controller_Index] += Delta;
         }

         int Camera_Stick_Delta = 2;
         Camera_Delta_X += (Camera_Stick_Delta * Controller->Stick_Right_X);
         Camera_Delta_Y += (Camera_Stick_Delta * Controller->Stick_Right_Y);

         if(Was_Pressed(Controller->Start))
         {
            // Camera->Position = Player->Position;
         }
      }
   }

   entity *Camera = Get_Entity(Game_State, Game_State->Camera_ID);

   // Entity Update.
   for(int Entity_Index = 1; Entity_Index < Game_State->Entity_Count; ++Entity_Index)
   {
      entity *Entity = Get_Entity(Game_State, Entity_Index);
      if(Is_Active(Entity))
      {
         switch(Entity->Type)
         {
            case Entity_Type_Player: {
               if(!Is_Animating(Entity))
               {
                  // NOTE: Assumes that players are stored contiguously.
                  int Player_Index = Entity_Index - Game_State->Player_IDs[0];
                  int Delta_X = Player_Delta_Xs[Player_Index];
                  int Delta_Y = Player_Delta_Ys[Player_Index];

                  if(Delta_X || Delta_Y)
                  {
                     if(Move(Game_State, Entity, Delta_X, Delta_Y))
                     {
                        Camera->Position.Z = Entity->Position.Z;
                     }
                  }
               }
               Advance_Animation(&Entity->Animation, Frame_Seconds, 10.0f);
            } break;

            case Entity_Type_Camera: {
               if(Camera_Delta_X || Camera_Delta_Y)
               {
                  Move(Game_State, Entity, Camera_Delta_X, Camera_Delta_Y);
               }
            } break;

            case Entity_Type_Dragon: {
               if(!Is_Animating(Entity))
               {
                  int Delta_X = 0;
                  int Delta_Y = 0;
                  switch(Random_Range(Entropy, Direction_None, Direction_Count-1))
                  {
                     case Direction_Up:    Delta_Y--; break;
                     case Direction_Down:  Delta_Y++; break;
                     case Direction_Left:  Delta_X--; break;
                     case Direction_Right: Delta_X++; break;
                     default: break;
                  }
                  if((Delta_X || Delta_Y) && Move(Game_State, Entity, Delta_X, Delta_Y))
                  {
                     // ...
                  }
               }
               Advance_Animation(&Entity->Animation, Frame_Seconds, 5.0f);
            } break;

            default: {
            } break;
         }
      }
   }

   int3 Camera_Position = Camera->Position;
   int3 Camera_Chunk_Position = Raw_To_Chunk_Position(Camera_Position.X, Camera_Position.Y, Camera_Position.Z);

   // Rendering
   u32 Palettes[2][4] = {
      {0x000088FF, 0x0000CCFF, 0x0000FFFF, 0x008800FF},
      {0x008800FF, 0x00CC00FF, 0x00FF00FF, 0x000088FF},
   };
   u32 *Palette = Palettes[Camera_Position.Z];

   Push_Clear(Renderer, Render_Layer_Background, Palette[0]);

   // TODO: Loop over the surrounding chunks instead of tiles so that we don't
   // have to query for the chunk on each iteration.

   int Border_Pixels = Maximum(1, Tile_Pixels / 16);

   int Mouse_X_Pixel = Input->Mouse_X * (float)Backbuffer_Width;
   int Mouse_Y_Pixel = Input->Mouse_Y * (float)Backbuffer_Height;

   for(int Chunk_Y = Camera_Chunk_Position.Y-1; Chunk_Y <= Camera_Chunk_Position.Y+1; ++Chunk_Y)
   {
      for(int Chunk_X = Camera_Chunk_Position.X-1; Chunk_X <= Camera_Chunk_Position.X+1; ++Chunk_X)
      {
         map_chunk *Chunk = Query_Map_Chunk_By_Chunk(Map, Chunk_X, Chunk_Y, Camera_Position.Z);
         if(Chunk)
         {
            for(map_chunk_entities *Entities = Chunk->Entities; Entities; Entities = Entities->Next)
            {
               for(int Index = 0; Index < Entities->Index_Count; ++Index)
               {
                  int Entity_Index = Entities->Indices[Index];

                  entity *Entity = Game_State->Entities + Entity_Index;
                  if(Is_Visible(Entity) && Entity->Position.Z == Camera_Position.Z)
                  {
                     int Pixel_Width  = Entity->Width * Tile_Pixels;
                     int Pixel_Height = Entity->Height * Tile_Pixels;
#if 1
                     int Offset_X = Tile_Pixels * Entity->Animation.Offset_X;
                     int Offset_Y = Tile_Pixels * Entity->Animation.Offset_Y;
#else
                     int Offset_X = 0;
                     int Offset_Y = 0;
#endif

                     int Pixel_X = Tile_Pixels * (Entity->Position.X - Camera_Position.X) + Backbuffer_Width/2 - Offset_X;
                     int Pixel_Y = Tile_Pixels * (Entity->Position.Y - Camera_Position.Y) + Backbuffer_Height/2 - Offset_Y;

                     if(Was_Pressed(Input->Mouse_Button_Left))
                     {
                        if(Mouse_X_Pixel >= Pixel_X && Mouse_X_Pixel < (Pixel_X + Pixel_Width) &&
                           Mouse_Y_Pixel >= Pixel_Y && Mouse_Y_Pixel < (Pixel_Y + Pixel_Height))
                        {
                           Game_State->Selected_Debug_Entity_ID = (Entity_Index != Game_State->Selected_Debug_Entity_ID) ? Entity_Index : 0;
                        }
                     }

                     int Nose_Dim = 4 * Border_Pixels;
                     int Nose_X = Pixel_X + Pixel_Width/2  - Nose_Dim/2;
                     int Nose_Y = Pixel_Y + Pixel_Height/2 - Nose_Dim/2;
                     switch(Entity->Animation.Direction)
                     {
                        case Direction_Up:    { Nose_Y -= (Pixel_Height/2 - Nose_Dim/2); } break;
                        case Direction_Down:  { Nose_Y += (Pixel_Height/2 - Nose_Dim/2); } break;
                        case Direction_Left:  { Nose_X -= (Pixel_Width/2  - Nose_Dim/2); } break;
                        case Direction_Right: { Nose_X += (Pixel_Width/2  - Nose_Dim/2); } break;
                        default: {} break;
                     }

                     switch(Entity->Type)
                     {
                        case Entity_Type_Player: {
                           render_layer Layer = Render_Layer_Foreground;
                           Push_Rectangle(Renderer, Layer, Pixel_X, Pixel_Y, Pixel_Width, Pixel_Height, 0x000088FF);
                           Push_Outline(Renderer, Layer, Pixel_X, Pixel_Y, Pixel_Width, Pixel_Height, 4*Border_Pixels, 0xFFFFFFFF);
                           Push_Rectangle(Renderer, Layer, Nose_X, Nose_Y, Nose_Dim, Nose_Dim, 0x0000FFFF);
                        } break;

                        case Entity_Type_Dragon: {
                           render_layer Layer = Render_Layer_Foreground;
                           Push_Rectangle(Renderer, Layer, Pixel_X, Pixel_Y, Pixel_Width, Pixel_Height, 0x880000FF);
                           Push_Outline(Renderer, Layer, Pixel_X, Pixel_Y, Pixel_Width, Pixel_Height, 4*Border_Pixels, 0xFF0000FF);
                           Push_Rectangle(Renderer, Layer, Nose_X, Nose_Y, Nose_Dim, Nose_Dim, 0xFFFF00FF);
                        } break;

                        case Entity_Type_Floor: {
                           render_layer Layer = Render_Layer_Background;
                           Push_Rectangle(Renderer, Layer, Pixel_X, Pixel_Y, Tile_Pixels, Tile_Pixels, Palette[0]);
                           Push_Outline(Renderer, Layer, Pixel_X, Pixel_Y, Tile_Pixels, Tile_Pixels, Border_Pixels, Palette[1]);
                        } break;

                        case Entity_Type_Wall: {
                           render_layer Layer = Render_Layer_Background;
                           Push_Rectangle(Renderer, Layer, Pixel_X, Pixel_Y, Tile_Pixels, Tile_Pixels, Palette[2]);
                           Push_Outline(Renderer, Layer, Pixel_X, Pixel_Y, Tile_Pixels, Tile_Pixels, Border_Pixels, Palette[1]);
                        } break;

                        case Entity_Type_Stairs: {
                           render_layer Layer = Render_Layer_Background;
                           texture Texture = (Entity->Position.Z == 0) ? Game_State->Upstairs : Game_State->Downstairs;

                           Push_Texture(Renderer, Layer, Texture, Pixel_X, Pixel_Y);
                           Push_Texture(Renderer, Layer, Texture, Pixel_X+Tile_Pixels, Pixel_Y);
                           Push_Texture(Renderer, Layer, Texture, Pixel_X, Pixel_Y+Tile_Pixels);
                           Push_Texture(Renderer, Layer, Texture, Pixel_X+Tile_Pixels, Pixel_Y+Tile_Pixels);
                        } break;

                        default: {
                        } break;
                     }

                     if(Entity_Index == Game_State->Selected_Debug_Entity_ID)
                     {
                        Push_Outline(Renderer, Render_Layer_UI, Pixel_X, Pixel_Y, Pixel_Width, Pixel_Height, 4*Border_Pixels, 0x00FF00FF);
                     }
                  }
               }
            }
         }
      }
   }

   // User Interface.
   text_context Text = Begin_Text(Renderer, Tile_Pixels/2, 0, &Game_State->Varia_Font, Text_Size_Large);
   Text_Line(&Text, "Dungeon Simulator");

   Text.Font = &Game_State->Fixed_Font;
   Text.Size = Text_Size_Medium;
   Text_Line(&Text, "Frame Time: %3.3fms", Frame_Seconds*1000.0f);
   Text_Line(&Text, "Permanent: %zuMB", (Permanent->End - Permanent->Begin) / (1024 * 1024));
   Text_Line(&Text, "Map: %zuMB", (Map->Arena.End - Map->Arena.Begin) / (1024 * 1024));
   Text_Line(&Text, "Scratch: %zuMB", (Scratch->End - Scratch->Begin) / (1024 * 1024));
   Text_Line(&Text, "Mouse: {%0.2f, %0.2f}", Input->Mouse_X, Input->Mouse_Y);
   if(Game_State->Selected_Debug_Entity_ID)
   {
      entity *Selected = Get_Entity(Game_State, Game_State->Selected_Debug_Entity_ID);
      string Name = Entity_Type_Names[Selected->Type];
      Text_Line(&Text, "[SELECTED] %.*s: {X:%d, Y:%d, Z:%d, W:%d, H:%d}",
                             (int)Name.Length, Name.Data,
                             Selected->Position.X,
                             Selected->Position.Y,
                             Selected->Position.Z,
                             Selected->Width,
                             Selected->Height);
   }

   Text_Line(&Text, "");
   Text_Line(&Text, "Total Entities: %d", Game_State->Entity_Count);

   map_chunk *Chunk = Query_Map_Chunk(Map, Camera_Position.X, Camera_Position.Y, Camera_Position.Z);
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
                  Text_Line(&Text, "[%d] %.*s: {%d, %d, %d}", Entity_Index, (int)Name.Length, Name.Data,
                            Entity->Position.X, Entity->Position.Y, Entity->Position.Z);
               } break;
            }
         }
      }
   }

   Text_Line(&Text, "");
   Text_Line(&Text, "Audio Tracks:");

   int Track_Index = 0;
   for(audio_track *Track = Game_State->Audio_Tracks; Track; Track = Track->Next)
   {
      Text_Line(&Text, "[%d] %d/%d samples %s", Track_Index++, Track->Sample_Index, Track->Sound->Sample_Count,
                (Track->Playback == Audio_Playback_Loop) ? "(looping)" : "");
   }

   if(Game_State->Active_Textbox_Index)
   {
      Display_Textbox(Game_State, Renderer, Game_State->Textbox_Dialogue[Game_State->Active_Textbox_Index]);
   }

   int Gui_Dim = Tile_Pixels;
   int Gui_X = Backbuffer_Width - 2*Gui_Dim*GAME_CONTROLLER_COUNT;
   int Gui_Y = Gui_Dim;

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

   Render(Renderer);
}

MIX_SOUND(Mix_Sound)
{
   game_state *Game_State = (game_state *)Memory.Base;

   // NOTE: Audio_Output should get its own arena if we decide to handle sound
   // mixing on a different thread.

   arena Arena = Game_State->Scratch;
   Audio_Output->Samples = Allocate(&Arena, s16, Audio_Output->Sample_Count*GAME_AUDIO_CHANNEL_COUNT);

   s16 *Destination = Audio_Output->Samples;
   for(int Sample_Index = 0; Sample_Index < Audio_Output->Sample_Count; ++Sample_Index)
   {
      for(int Channel_Index = 0; Channel_Index < GAME_AUDIO_CHANNEL_COUNT; ++Channel_Index)
      {
         *Destination++ = 0;
      }
   }

   audio_track **Track_Ptr = &Game_State->Audio_Tracks;
   while(*Track_Ptr)
   {
      audio_track *Track = *Track_Ptr;

      game_sound *Sound = Track->Sound;
      s16 *Destination = Audio_Output->Samples;

      int Samples_Left = Sound->Sample_Count - Track->Sample_Index;
      int Sample_Count = Audio_Output->Sample_Count;
      if(Track->Playback == Audio_Playback_Once && Samples_Left < Sample_Count)
      {
         Sample_Count = Samples_Left;
      }

      for(int Sample_Index = 0; Sample_Index < Sample_Count; ++Sample_Index)
      {
         for(int Channel_Index = 0; Channel_Index < GAME_AUDIO_CHANNEL_COUNT; ++Channel_Index)
         {
            *Destination++ += Sound->Samples[Channel_Index][Track->Sample_Index];
         }

         Track->Sample_Index++;
         if(Track->Playback == Audio_Playback_Loop)
         {
            Track->Sample_Index %= Sound->Sample_Count;
         }
      }

      if(Track->Playback == Audio_Playback_Once && Track->Sample_Index == Sound->Sample_Count)
      {
         *Track_Ptr = Track->Next;
         Track->Next = Game_State->Free_Audio_Tracks;
         Game_State->Free_Audio_Tracks = Track;
      }
      else
      {
         Track_Ptr = &Track->Next;
      }
   }
}
