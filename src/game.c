/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include "shared.h"
#include "render.h"
#include "random.h"
#include "assets.h"
#include "map.h"
#include "entity.h"
#include "audio.h"
#include "game.h"

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

   audio_sound Background_Music;
   audio_sound Clap;

   map Map;

   int Entity_Count;
   entity Entities[1024*1024];

   int Selected_Debug_Entity_ID;
   int Camera_ID;
   int Player_IDs[GAME_CONTROLLER_COUNT];

   audio_track *Audio_Tracks;
   audio_track *Free_Audio_Tracks;

   bool Debug_Overlay;
} game_state;

#include "intrinsics.c"
#include "math.c"
#include "random.c"
#include "assets.c"
#include "map.c"
#include "entity.c"
#include "render.c"
#include "audio.c"
#include "debug.c"

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

UPDATE(Update)
{
   game_state *Game_State = (game_state *)Memory.Base;

   arena *Permanent = &Game_State->Permanent;
   arena *Scratch = &Game_State->Scratch;

   random_entropy *Entropy = &Game_State->Entropy;
   map *Map = &Game_State->Map;

   int Backbuffer_Width = Renderer->Backbuffer.Width;
   int Backbuffer_Height = Renderer->Backbuffer.Height;

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
      Renderer->Pixels_Per_Meter = Backbuffer_Width / 40;
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
      direction Direction_Towards_Previous_Room = Direction_None;
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

         direction Direction = Direction_Towards_Previous_Room;
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

      Load_Font(&Game_State->Varia_Font, Permanent, *Scratch, "data/Inter.ttf", Renderer->Pixels_Per_Meter);
      Load_Font(&Game_State->Fixed_Font, Permanent, *Scratch, "data/JetBrainsMono.ttf", Renderer->Pixels_Per_Meter);
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

      Game_State->Debug_Overlay = true;
   }

   int Player_Delta_Xs[GAME_CONTROLLER_COUNT] = {0};
   int Player_Delta_Ys[GAME_CONTROLLER_COUNT] = {0};
   int Camera_Delta_X = 0;
   int Camera_Delta_Y = 0;

   entity *Camera = Get_Entity(Game_State, Game_State->Camera_ID);

   // General Input Handling.
   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      game_controller *Controller = Input->Controllers + Controller_Index;
      if(Controller->Connected)
      {
         if(Was_Pressed(Controller->Back))
         {
            Game_State->Debug_Overlay = !Game_State->Debug_Overlay;
         }

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
            Camera_Delta_X = Player->Position.X - Camera->Position.X;
            Camera_Delta_Y = Player->Position.Y - Camera->Position.Y;
         }
      }
   }

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

   int Pixels_Per_Meter = Renderer->Pixels_Per_Meter;
   int Border_Pixels = Maximum(1, Pixels_Per_Meter / 16);

   int Mouse_X_Pixel = Input->Mouse_X * (float)Backbuffer_Width;
   int Mouse_Y_Pixel = Input->Mouse_Y * (float)Backbuffer_Height;

   for(int Chunk_Y = Camera_Chunk_Position.Y-1; Chunk_Y <= Camera_Chunk_Position.Y+1; ++Chunk_Y)
   {
      for(int Chunk_X = Camera_Chunk_Position.X-1; Chunk_X <= Camera_Chunk_Position.X+1; ++Chunk_X)
      {
         map_chunk *Chunk = Get_Map_Chunk_By_Chunk_Position(Map, Chunk_X, Chunk_Y, Camera_Position.Z);
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
                     int Pixel_Width  = Entity->Width * Pixels_Per_Meter;
                     int Pixel_Height = Entity->Height * Pixels_Per_Meter;
#if 1
                     int Offset_X = Pixels_Per_Meter * Entity->Animation.Offset_X;
                     int Offset_Y = Pixels_Per_Meter * Entity->Animation.Offset_Y;
#else
                     int Offset_X = 0;
                     int Offset_Y = 0;
#endif

                     int Pixel_X = Pixels_Per_Meter * (Entity->Position.X - Camera_Position.X) + Backbuffer_Width/2 - Offset_X;
                     int Pixel_Y = Pixels_Per_Meter * (Entity->Position.Y - Camera_Position.Y) + Backbuffer_Height/2 - Offset_Y;

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
                           Push_Rectangle(Renderer, Layer, Pixel_X, Pixel_Y, Pixels_Per_Meter, Pixels_Per_Meter, Palette[0]);
                           Push_Outline(Renderer, Layer, Pixel_X, Pixel_Y, Pixels_Per_Meter, Pixels_Per_Meter, Border_Pixels, Palette[1]);
                        } break;

                        case Entity_Type_Wall: {
                           render_layer Layer = Render_Layer_Background;
                           Push_Rectangle(Renderer, Layer, Pixel_X, Pixel_Y, Pixels_Per_Meter, Pixels_Per_Meter, Palette[2]);
                           Push_Outline(Renderer, Layer, Pixel_X, Pixel_Y, Pixels_Per_Meter, Pixels_Per_Meter, Border_Pixels, Palette[1]);
                        } break;

                        case Entity_Type_Stairs: {
                           render_layer Layer = Render_Layer_Background;
                           texture Texture = (Entity->Position.Z == 0) ? Game_State->Upstairs : Game_State->Downstairs;

                           Push_Texture(Renderer, Layer, Texture, Pixel_X, Pixel_Y);
                           Push_Texture(Renderer, Layer, Texture, Pixel_X+Pixels_Per_Meter, Pixel_Y);
                           Push_Texture(Renderer, Layer, Texture, Pixel_X, Pixel_Y+Pixels_Per_Meter);
                           Push_Texture(Renderer, Layer, Texture, Pixel_X+Pixels_Per_Meter, Pixel_Y+Pixels_Per_Meter);
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
   if(Game_State->Debug_Overlay)
   {
      Display_Debug_Overlay(Game_State, Input, Renderer, Frame_Seconds);
   }

   if(Game_State->Active_Textbox_Index)
   {
      Display_Textbox(Game_State, Renderer, Game_State->Textbox_Dialogue[Game_State->Active_Textbox_Index]);
   }

   Render(Renderer);
}

MIX_SOUND(Mix_Sound)
{
   game_state *Game_State = (game_state *)Memory.Base;

   // NOTE: Audio_Output should get its own arena if we decide to handle sound
   // mixing on a different thread.

   arena Arena = Game_State->Scratch;
   Audio_Output->Samples = Allocate(&Arena, s16, Audio_Output->Sample_Count*AUDIO_CHANNEL_COUNT);

   s16 *Destination = Audio_Output->Samples;
   for(int Sample_Index = 0; Sample_Index < Audio_Output->Sample_Count; ++Sample_Index)
   {
      for(int Channel_Index = 0; Channel_Index < AUDIO_CHANNEL_COUNT; ++Channel_Index)
      {
         *Destination++ = 0;
      }
   }

   audio_track **Track_Ptr = &Game_State->Audio_Tracks;
   while(*Track_Ptr)
   {
      audio_track *Track = *Track_Ptr;

      audio_sound *Sound = Track->Sound;
      s16 *Destination = Audio_Output->Samples;

      int Samples_Left = Sound->Sample_Count - Track->Sample_Index;
      int Sample_Count = Audio_Output->Sample_Count;
      if(Track->Playback == Audio_Playback_Once && Samples_Left < Sample_Count)
      {
         Sample_Count = Samples_Left;
      }

      for(int Sample_Index = 0; Sample_Index < Sample_Count; ++Sample_Index)
      {
         for(int Channel_Index = 0; Channel_Index < AUDIO_CHANNEL_COUNT; ++Channel_Index)
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
