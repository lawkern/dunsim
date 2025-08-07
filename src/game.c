/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include "shared.h"
#include "math.h"
#include "intrinsics.h"
#include "debug.h"
#include "render.h"
#include "random.h"
#include "assets.h"
#include "map.h"
#include "entity.h"
#include "text.h"
#include "audio.h"
#include "platform.h"
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

#include "math.c"
#include "random.c"
#include "assets.c"
#include "map.c"
#include "entity.c"
#include "text.c"
#include "render.c"
#include "audio.c"
#include "renderer_software.c"
#include "renderer_opengl.c"
#include "debug.c"

static void Display_Textbox(game_state *Game_State, renderer *Renderer, string Text)
{
   text_font *Font = &Game_State->Varia_Font;
   if(Font->Loaded)
   {
      text_size Size = Text_Size_Medium;
      text_glyphs *Glyphs = Font->Glyphs + Size;

      float Pixels_Per_Meter = Renderer->Pixels_Per_Meter;
      float Meters_Per_Pixel = 1.0f / Pixels_Per_Meter;

      float Scale = Glyphs->Pixel_Scale * Meters_Per_Pixel;
      float Line_Advance = Scale * (Font->Ascent - Font->Descent + Font->Line_Gap);

      float Margin = 0.5f;
      float Padding = 1.0f;

      vec2 Screen_Dim = {Renderer->Screen_Width_Meters, Renderer->Screen_Height_Meters};
      vec2 Screen_Center = Mul2(Screen_Dim, 0.5f);

      int Line_Count = 1;
      int Max_Line_Count = 6;

      float Box_Width = Screen_Dim.X - 2*Margin;
      float Box_Height = ((float)Max_Line_Count * Line_Advance) + 2*Padding;
      float Box_X = Margin - Screen_Center.X;
      float Box_Y = -(Margin + Box_Height - Screen_Center.Y);

      Push_Rectangle(Renderer, Render_Layer_UI, Box_X, Box_Y, Box_Width, Box_Height, Vec4(0, 0, 0, 1));

      float Text_X_Initial = Margin + Padding - Screen_Center.X;
      float Text_X = Text_X_Initial;
      float Text_Y = Box_Y + Padding + Scale*Font->Ascent;

      cut Words = {0};
      Words.After = Text;
      while(Words.After.Length)
      {
         Words = Cut(Words.After, ' ');
         string Word = Words.Before;

         float Width = Get_Text_Width_Pixels(Font, Size, Word) * Meters_Per_Pixel;
         if((Box_X + Box_Width - Padding) < (Text_X + Width))
         {
            Text_X = Text_X_Initial;
            Advance_Text_Line(Font, Size, Meters_Per_Pixel, &Text_Y);
            Line_Count++;
         }

         for(int Index = 0; Index < Word.Length; ++Index)
         {
            int Codepoint = Word.Data[Index];
            if(Codepoint == '\n')
            {
               Text_X = Text_X_Initial;
               Advance_Text_Line(Font, Size, Meters_Per_Pixel, &Text_Y);
               Line_Count++;
            }
            else
            {
               texture Glyph = Glyphs->Bitmaps[Codepoint];
               Push_Texture(Renderer, Render_Layer_UI, Glyph, Text_X, Text_Y, Glyph.Width, Glyph.Height);

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

static int Add_Camera(game_state *Game_State, int3 Position)
{
   u32 Flags = Entity_Flag_Active;
   int Result = Create_Entity(Game_State, Entity_Type_Camera, Position, 0, 0, Flags);
   return(Result);
}

static int Add_Floor(game_state *Game_State, int3 Position, int Width, int Height)
{
   u32 Flags = Entity_Flag_Active|Entity_Flag_Visible;
   int Result = Create_Entity(Game_State, Entity_Type_Floor, Position, Width, Height, Flags);
   return(Result);
}

static int Add_Wall(game_state *Game_State, int3 Position)
{
   u32 Flags = Entity_Flag_Active|Entity_Flag_Visible|Entity_Flag_Collides;
   int Result = Create_Entity(Game_State, Entity_Type_Wall, Position, 1, 1, Flags);
   return(Result);
}

static int Add_Stairs(game_state *Game_State, int3 Position)
{
   u32 Flags = Entity_Flag_Active|Entity_Flag_Visible;
   int Result = Create_Entity(Game_State, Entity_Type_Stairs, Position, 2, 2, Flags);
   return(Result);
}

static int Add_Player(game_state *Game_State, int3 Position)
{
   u32 Flags = Entity_Flag_Visible|Entity_Flag_Collides;
   int Result = Create_Entity(Game_State, Entity_Type_Player, Position, 2, 2, Flags);
   return(Result);
}

static int Add_Creature(game_state *Game_State, entity_type Type, int3 Position, int Width, int Height)
{
   u32 Flags = Entity_Flag_Active|Entity_Flag_Visible|Entity_Flag_Collides;
   int Result = Create_Entity(Game_State, Type, Position, Width, Height, Flags);
   return(Result);
}

static void Create_Debug_Room(game_state *Game_State)
{
   map *Map = &Game_State->Map;

   int Min_X = -12;
   int Max_X = 12;

   int Min_Y = -8;
   int Max_Y = 8;

   for(int Z = 0; Z <= 1; ++Z)
   {
      for(int X = Min_X-1; X <= Max_X+2; ++X)
      {
         Add_Wall(Game_State, Int3(X, Min_Y-1, Z));
         Add_Wall(Game_State, Int3(X, Max_Y+2, Z));
      }

      for(int Y = Min_Y-1; Y <= Max_Y+1; ++Y)
      {
         Add_Wall(Game_State, Int3(Min_X-1, Y, Z));
         Add_Wall(Game_State, Int3(Max_X+2, Y, Z));
      }

      for(int Y = Min_Y; Y <= Max_Y; Y+=2)
      {
         for(int X = Min_X; X <= Max_X; X+=2)
         {
            if(Y == Min_Y && X == Max_X)
            {
               Add_Stairs(Game_State, Int3(X, Y, Z));
            }
            else
            {
               Add_Floor(Game_State, Int3(X, Y, Z), 2, 2);
            }
         }
      }
   }
}

UPDATE(Update)
{
   game_state *Game_State = (game_state *)Memory.Base;

   arena *Permanent = &Game_State->Permanent;
   arena *Scratch = &Game_State->Scratch;

   random_entropy *Entropy = &Game_State->Entropy;
   map *Map = &Game_State->Map;

   float Backbuffer_Width = (float)Renderer->Backbuffer.Width;
   float Backbuffer_Height = (float)Renderer->Backbuffer.Height;

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
      Renderer->Screen_Width_Meters = 40.0f;
      Renderer->Screen_Height_Meters = (Backbuffer_Height / Backbuffer_Width) * Renderer->Screen_Width_Meters;

      for(int Queue_Index = 0; Queue_Index < Array_Count(Renderer->Queues); ++Queue_Index)
      {
         Renderer->Queues[Queue_Index] = Allocate(Permanent, render_queue, 1);
      }

      // Initialize entropy.
      Game_State->Entropy = Random_Seed(0x13);

      // Initialize entities.
      int3 Origin = {0, 0, 0};
      Game_State->Entity_Count++; // Skip null entity.
      Game_State->Camera_ID = Add_Camera(Game_State, Origin);
      for(int Player_Index = 0; Player_Index < GAME_CONTROLLER_COUNT; ++Player_Index)
      {
         Game_State->Player_IDs[Player_Index] = Add_Player(Game_State, Origin);
      }
      Add_Creature(Game_State, Entity_Type_Dragon, Int3(6, 0, Origin.Z + 1), 4, 4);

      Create_Debug_Room(Game_State);

      // Initialize assets.
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
#if 0
      Play_Sound(Game_State, &Game_State->Background_Music, Audio_Playback_Loop);
#endif

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
   vec4 Palettes[2][4] = {
      // {Vec4(0.25, 0.25, 0.25, 1), Vec4(0.75, 0.75, 0.75, 1), Vec4(1, 1, 1, 1), Vec4(0.5, 0.5, 0.5, 1)},
      {Vec4(0, 0, 0.5, 1), Vec4(0, 0, 0.75, 1), Vec4(0, 0, 1, 1), Vec4(0, 0.5, 0, 1)},
      {Vec4(0, 0.5, 0, 1), Vec4(0, 0.75, 0, 1), Vec4(0, 1, 0, 1), Vec4(0, 0, 0.5, 1)}, // 0x008800FF, 0x00CC00FF, 0x00FF00FF, 0x000088FF},
   };
   vec4 *Palette = Palettes[Camera_Position.Z];

   Push_Clear(Renderer, Palette[0]);

   float Mouse_X = Input->Binormal_Mouse_X * Renderer->Screen_Width_Meters * 0.5f;
   float Mouse_Y = Input->Binormal_Mouse_Y * Renderer->Screen_Height_Meters * 0.5f;

   int Chunk_Z = Camera_Position.Z;
   for(int Chunk_Y = Camera_Chunk_Position.Y-1; Chunk_Y <= Camera_Chunk_Position.Y+1; ++Chunk_Y)
   {
      for(int Chunk_X = Camera_Chunk_Position.X-1; Chunk_X <= Camera_Chunk_Position.X+1; ++Chunk_X)
      {
         map_chunk *Chunk = Get_Map_Chunk_By_Chunk_Position(Map, Chunk_X, Chunk_Y, Chunk_Z);
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
                     float Width = Entity->Width;
                     float Height = Entity->Height;
                     float Half_Width = 0.5f * Width;
                     float Half_Height = 0.5f * Height;

                     float X = (float)(Entity->Position.X - Camera_Position.X) - Entity->Animation.Offset_X;
                     float Y = (float)(Entity->Position.Y - Camera_Position.Y) - Entity->Animation.Offset_Y;

                     if(Was_Pressed(Input->Mouse_Button_Left))
                     {
                        if(Mouse_X >= X && Mouse_X < (X + Width) &&
                           Mouse_Y >= Y && Mouse_Y < (Y + Height))
                        {
                           Game_State->Selected_Debug_Entity_ID = (Entity_Index != Game_State->Selected_Debug_Entity_ID) ? Entity_Index : 0;
                        }
                     }

                     float Nose_Dim = 0.2f;
                     float Nose_Half_Dim = 0.5f * Nose_Dim;
                     float Nose_X = X + Half_Width  - Nose_Half_Dim;
                     float Nose_Y = Y + Half_Height - Nose_Half_Dim;
                     switch(Entity->Animation.Direction)
                     {
                        case Direction_Up:    { Nose_Y -= (Half_Height - Nose_Half_Dim); } break;
                        case Direction_Down:  { Nose_Y += (Half_Height - Nose_Half_Dim); } break;
                        case Direction_Left:  { Nose_X -= (Half_Width  - Nose_Half_Dim); } break;
                        case Direction_Right: { Nose_X += (Half_Width  - Nose_Half_Dim); } break;
                        default: {} break;
                     }

                     switch(Entity->Type)
                     {
                        case Entity_Type_Player: {
                           render_layer Layer = Render_Layer_Foreground;
                           Push_Rectangle(Renderer, Layer, X, Y, Width, Height, Vec4(0, 0, 0.5, 1));
                           Push_Outline(Renderer, Layer, X, Y, Width, Height, 0.1f, Vec4(1, 1, 1, 1));
                           Push_Rectangle(Renderer, Layer, Nose_X, Nose_Y, Nose_Dim, Nose_Dim, Vec4(0, 0, 1, 1));
                        } break;

                        case Entity_Type_Dragon: {
                           render_layer Layer = Render_Layer_Foreground;
                           Push_Rectangle(Renderer, Layer, X, Y, Width, Height, Vec4(0.5, 0, 0, 1));
                           Push_Outline(Renderer, Layer, X, Y, Width, Height, 0.2f, Vec4(1, 0, 0, 1));
                           Push_Rectangle(Renderer, Layer, Nose_X, Nose_Y, Nose_Dim, Nose_Dim, Vec4(1, 1, 0, 1));
                        } break;

                        case Entity_Type_Floor: {
                           render_layer Layer = Render_Layer_Background;
                           Push_Rectangle(Renderer, Layer, X, Y, Width, Height, Palette[0]);
                           Push_Outline(Renderer, Layer, X, Y, Width, Height, 0.05f, Palette[1]);
                        } break;

                        case Entity_Type_Wall: {
                           render_layer Layer = Render_Layer_Background;
                           Push_Rectangle(Renderer, Layer, X, Y, Width, Height, Palette[2]);
                           Push_Outline(Renderer, Layer, X, Y, Width, Height, 0.05f, Palette[1]);
                        } break;

                        case Entity_Type_Stairs: {
                           render_layer Layer = Render_Layer_Background;
                           texture Texture = (Entity->Position.Z == 0) ? Game_State->Upstairs : Game_State->Downstairs;

                           vec2 Origin = {X, Y};
                           vec2 X_Axis = {Width, 0};
                           vec2 Y_Axis = {0, Height};
                           Push_Textured_Quad(Renderer, Layer, Texture, Origin, X_Axis, Y_Axis);
                        } break;

                        default: {
                        } break;
                     }

                     if(Entity_Index == Game_State->Selected_Debug_Entity_ID)
                     {
                        Push_Outline(Renderer, Render_Layer_UI, X, Y, Width, Height, 0.2f, Vec4(0, 1, 0, 1));
                     }
                  }
               }
            }
#if 0
            if(Game_State->Debug_Overlay)
            {
               // NOTE: Highlight chunk boundaries.
               int3 Position = Chunk_To_Raw_Position(Chunk_X, Chunk_Y, Chunk_Z);
               float X = (float)(Position.X - Camera_Position.X);
               float Y = (float)(Position.Y - Camera_Position.Y);
               float Dim = MAP_CHUNK_DIM;
               Push_Outline(Renderer, Render_Layer_Foreground, X, Y, Dim, Dim, 0.05f, Palette[2]);
            }
#endif
         }
      }
   }


   static float Time = 0;
   float Scale = 2.0f;
   float Speed = 0.05f;

   vec2 Origin = {-15 + 2.0f*Sine(Speed*Time), 0};
   vec2 X_Axis = Mul2(Vec2(Cosine(Speed*Time), Sine(Speed*Time)), Scale);
   vec2 Y_Axis = Perp2(X_Axis);

   texture Texture = Game_State->Varia_Font.Glyphs[Text_Size_Large].Bitmaps['0'];
   float Aspect = (float)Texture.Width / (float)Texture.Height;
   X_Axis = Mul2(X_Axis, Aspect);

   Push_Debug_Basis(Renderer, Texture, Origin, X_Axis, Y_Axis);
   Time += Frame_Seconds;


   // User Interface.
   if(Game_State->Debug_Overlay)
   {
      Display_Debug_Overlay(Game_State, Input, Renderer, Frame_Seconds);
   }

   if(Game_State->Active_Textbox_Index)
   {
      Display_Textbox(Game_State, Renderer, Game_State->Textbox_Dialogue[Game_State->Active_Textbox_Index]);
   }
}
