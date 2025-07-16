/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include "game.h"

#include "math.c"
#include "random.c"
#include "assets.c"
#include "map.c"

typedef enum {
   Direction_None,
   Direction_Up,
   Direction_Down,
   Direction_Left,
   Direction_Right,

   Direction_Count,
} movement_direction;

typedef struct {
   movement_direction Direction;
   float Offset_X;
   float Offset_Y;
} animation;

typedef struct {
   int X, Y, Z;
} position;

typedef struct {
   position Position;
   animation Animation;
   bool Active;
} player;

typedef struct {
   position Position;
   animation Animation;
} dragon;

typedef struct {
   arena Arena;
   arena Scratch;
   random_entropy Entropy;

   text_font Varia_Font;
   text_font Fixed_Font;

   game_texture Upstairs;
   game_texture Downstairs;

   map Map;
   position Camera;

   player Players[GAME_CONTROLLER_COUNT];
   dragon Dragon;
} game_state;

static void Clear(game_texture Destination, u32 Color)
{
   for(int Y = 0; Y < Destination.Height; ++Y)
   {
      for(int X = 0; X < Destination.Width; ++X)
      {
         Destination.Memory[(Destination.Width * Y) + X] = Color;
      }
   }
}

static void Draw_Rectangle(game_texture Destination, int X, int Y, int Width, int Height, u32 Color)
{
   int Min_X = Maximum(X, 0);
   int Min_Y = Maximum(Y, 0);
   int Max_X = Minimum(Destination.Width, X + Width);
   int Max_Y = Minimum(Destination.Height, Y + Height);

   for(int Y = Min_Y; Y < Max_Y; ++Y)
   {
      for(int X = Min_X; X < Max_X; ++X)
      {
         Destination.Memory[(Destination.Width * Y) + X] = Color;
      }
   }
}

static void Draw_Outline(game_texture Destination, int X, int Y, int Width, int Height, int Weight, u32 Color)
{
   Draw_Rectangle(Destination, X, Y, Width-Weight, Weight, Color); // Top
   Draw_Rectangle(Destination, X+Weight, Y+Height-Weight, Width-Weight, Weight, Color); // Bottom
   Draw_Rectangle(Destination, X, Y+Weight, Weight, Height-Weight, Color); // Left
   Draw_Rectangle(Destination, X+Width-Weight, Y, Weight, Height-Weight, Color); // Right
}

static void Draw_Bitmap(game_texture Destination, game_texture Source, float X, float Y)
{
   X += Source.Offset_X;
   Y += Source.Offset_Y;

   int Min_X = Maximum(X, 0);
   int Min_Y = Maximum(Y, 0);
   int Max_X = Minimum(Destination.Width, X + Source.Width);
   int Max_Y = Minimum(Destination.Height, Y + Source.Height);

   int Clip_X_Offset = Min_X - X;
   int Clip_Y_Offset = Min_Y - Y;

   u32 *Source_Row = Source.Memory + Clip_Y_Offset*Source.Width;
   for(int Destination_Y = Min_Y; Destination_Y < Max_Y; ++Destination_Y)
   {
      int X_Offset = Clip_X_Offset;
      u32 *Destination_Row = Destination.Memory + Destination.Width*Destination_Y;

      for(int Destination_X = Min_X; Destination_X < Max_X; ++Destination_X)
      {
         u32 Source_Pixel = Source_Row[X_Offset++];
         u32 *Destination_Pixel = Destination_Row + Destination_X;

         float SR = (float)((Source_Pixel >> 24) & 0xFF);
         float SG = (float)((Source_Pixel >> 16) & 0xFF);
         float SB = (float)((Source_Pixel >>  8) & 0xFF);
         float SA = (float)((Source_Pixel >>  0) & 0xFF) / 255.0f;

         float DR = (float)((*Destination_Pixel >> 24) & 0xFF);
         float DG = (float)((*Destination_Pixel >> 16) & 0xFF);
         float DB = (float)((*Destination_Pixel >>  8) & 0xFF);

         u32 R = (u32)((DR * (1.0f-SA) + SR) + 0.5f);
         u32 G = (u32)((DG * (1.0f-SA) + SG) + 0.5f);
         u32 B = (u32)((DB * (1.0f-SA) + SB) + 0.5f);

         *Destination_Pixel = (R<<24) | (G<<16) | (B<<8) | 0xFF;
      }

      Source_Row += Source.Width;
   }
}

static void Advance_Text_Line(text_font *Font, text_size Size, int *Y)
{
   float Scale = Font->Glyphs[Size].Scale;

   float Line_Advance = Scale * (Font->Ascent - Font->Descent + Font->Line_Gap);
   *Y += Line_Advance;
}

static void Draw_Text(game_texture Destination, text_font *Font, text_size Size, int X, int Y, string Text)
{
   if(Font->Loaded)
   {
      text_glyphs *Glyphs = Font->Glyphs + Size;
      float Scale = Glyphs->Scale;

      for(size Index = 0; Index < Text.Length; ++Index)
      {
         int Codepoint = Text.Data[Index];

         game_texture Glyph = Glyphs->Bitmaps[Codepoint];
         Draw_Bitmap(Destination, Glyph, X, Y);

         if(Index != Text.Length-1)
         {
            int Next_Codepoint = Text.Data[Index + 1];
            int Pair_Index = (Codepoint * Glyph_Count) + Next_Codepoint;
            X += (Scale * Font->Distances[Pair_Index]);
         }
      }
   }
}

static bool Can_Move(map *Map, position *Position, int Delta_X, int Delta_Y)
{
   bool Result = false;

   int New_X = Position->X + Delta_X;
   int New_Y = Position->Y + Delta_Y;
   int New_Z = Position->Z;

   int Tile_00 = Get_Map_Position_Value(Map, New_X, New_Y, New_Z);
   int Tile_01 = Get_Map_Position_Value(Map, New_X + 1, New_Y, New_Z);
   int Tile_10 = Get_Map_Position_Value(Map, New_X, New_Y + 1, New_Z);
   int Tile_11 = Get_Map_Position_Value(Map, New_X + 1, New_Y + 1, New_Z);

   if(!Position_Is_Occupied(Tile_00) &&
      !Position_Is_Occupied(Tile_01) &&
      !Position_Is_Occupied(Tile_10) &&
      !Position_Is_Occupied(Tile_11))
   {
      Result = true;
   }

   return(Result);
}

static bool Can_Ascend(map *Map, position *Position)
{
   bool Result = false;

   int Tile_00 = Get_Map_Position_Value(Map, Position->X, Position->Y, Position->Z);
   int Tile_01 = Get_Map_Position_Value(Map, Position->X + 1, Position->Y, Position->Z);
   int Tile_10 = Get_Map_Position_Value(Map, Position->X, Position->Y + 1, Position->Z);
   int Tile_11 = Get_Map_Position_Value(Map, Position->X + 1, Position->Y + 1, Position->Z);

   if(Tile_00 == 3 &&
      Tile_01 == 3 &&
      Tile_10 == 3 &&
      Tile_11 == 3)
   {
      Result = true;
   }

   return(Result);
}

static bool Move(map *Map, position *Position, animation *Animation, int Delta_X, int Delta_Y)
{
   bool Ok = false;
   if(Delta_X && Delta_Y)
   {
      if(Animation->Direction == Direction_Right || Animation->Direction == Direction_Left)
      {
         Ok = Can_Move(Map, Position, 0, Delta_Y);
         if(Ok)
         {
            Delta_X = 0;
         }
         else
         {
            Ok = Can_Move(Map, Position, Delta_X, 0);
            Delta_Y = 0;
         }
      }
      else
      {
         Ok = Can_Move(Map, Position, Delta_X, 0);
         if(Ok)
         {
            Delta_Y = 0;
         }
         else
         {
            Ok = Can_Move(Map, Position, 0, Delta_Y);
            Delta_X = 0;
         }
      }
   }
   else
   {
      Ok = Can_Move(Map, Position, Delta_X, Delta_Y);
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

      Position->X += Delta_X;
      Position->Y += Delta_Y;

      if(Can_Ascend(Map, Position))
      {
         if(Position->Z == 0)
         {
            Position->Z = 1;
         }
         else
         {
            Position->Z = 0;
         }
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

UPDATE(Update)
{
   game_state *Game_State = (game_state *)Memory.Base;

   arena *Arena = &Game_State->Arena;
   arena *Scratch = &Game_State->Scratch;
   random_entropy *Entropy = &Game_State->Entropy;
   map *Map = &Game_State->Map;

   if(!Arena->Begin)
   {
      Arena->Begin = Memory.Base + sizeof(*Game_State);
      Arena->End = Arena->Begin + Megabytes(64);
      Log("Arena Size: %dMB", (Arena->End - Arena->Begin) >> 20);

      Scratch->Begin = Arena->End;
      Scratch->End = Memory.Base + Memory.Size;
      Log("Scratch Size: %dMB", (Scratch->End - Scratch->Begin) >> 20);

      Assert(Scratch->Begin < Scratch->End);

      Game_State->Entropy = Random_Seed(0x13);

      for(int Chunk_Z = 0; Chunk_Z <= 1; ++Chunk_Z)
      {
         int Z = Chunk_Z;
         for(int Chunk_Y = -8; Chunk_Y <= 8; ++Chunk_Y)
         {
            int Y = Chunk_Y * MAP_CHUNK_DIM;
            for(int Chunk_X = -10; Chunk_X <= 10; ++Chunk_X)
            {
               int X = Chunk_X * MAP_CHUNK_DIM;
               *Insert_Map_Chunk(Map, X, Y, Z) = Debug_Map_Chunk;
            }
         }
      }

      Game_State->Camera.X = 2;
      Game_State->Camera.Y = 2;

      for(int Player_Index = 0; Player_Index < Array_Count(Game_State->Players); ++Player_Index)
      {
         player *Player = Game_State->Players + Player_Index;
         Player->Position.X = Game_State->Camera.X;
         Player->Position.Y = Game_State->Camera.Y;
      }

      Game_State->Dragon.Position.X = 6;
      Game_State->Dragon.Position.Y = -8;

      Load_Font(&Game_State->Varia_Font, Arena, *Scratch, "data/Inter.ttf");
      Load_Font(&Game_State->Fixed_Font, Arena, *Scratch, "data/JetBrainsMono.ttf");
      if(!Game_State->Varia_Font.Loaded)
      {
         Log("During development, make sure to run the program from the project root folder.");
      }

      Game_State->Upstairs = Load_Image(Arena, "data/upstairs.png");
      Game_State->Downstairs = Load_Image(Arena, "data/downstairs.png");
   }

   // int Tile_Dim_Pixels = Backbuffer.Width / 40;
   int Tile_Dim_Pixels = Backbuffer.Height / 32;
   int Border_Dim_Pixels = Maximum(1, Tile_Dim_Pixels / 16);

   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      game_controller *Controller = Input->Controllers + Controller_Index;
      if(Controller->Connected)
      {
         Assert(GAME_CONTROLLER_COUNT == Array_Count(Game_State->Players));
         player *Player = Game_State->Players + Controller_Index;
         Player->Active = true;

         bool Moving_Camera = Is_Held(Controller->Shoulder_Right);
         bool Player_Animating = (Player->Animation.Offset_X != 0.0f || Player->Animation.Offset_Y != 0.0f);

         if(Moving_Camera)
         {
            int Camera_Delta = 4;
            if(Is_Held(Controller->Shoulder_Right))
            {
               if(Was_Pressed(Controller->Move_Up))    Game_State->Camera.Y -= Camera_Delta;
               if(Was_Pressed(Controller->Move_Down))  Game_State->Camera.Y += Camera_Delta;
               if(Was_Pressed(Controller->Move_Left))  Game_State->Camera.X -= Camera_Delta;
               if(Was_Pressed(Controller->Move_Right)) Game_State->Camera.X += Camera_Delta;
            }
         }
         else if(!Player_Animating)
         {
            bool Dash = Is_Held(Controller->Action_Down);
            int Delta = (Dash) ? 2 : 1;

            bool Up    = Is_Held(Controller->Move_Up);
            bool Down  = Is_Held(Controller->Move_Down);
            bool Left  = Is_Held(Controller->Move_Left);
            bool Right = Is_Held(Controller->Move_Right);

            int Delta_X = 0;
            int Delta_Y = 0;

            if(Up)    Delta_Y -= Delta;
            if(Down)  Delta_Y += Delta;
            if(Left)  Delta_X -= Delta;
            if(Right) Delta_X += Delta;

            // TODO: This includes a number of redundant checks just to support
            // sliding along walls when moving into them diagonally. It's not
            // clear we even want to support diagonal movement, so this should
            // all be cleaned up.

            if(Delta_X || Delta_Y)
            {
               if(Move(Map, &Player->Position, &Player->Animation, Delta_X, Delta_Y))
               {
                  Game_State->Camera.Z = Player->Position.Z;
               }
            }
         }

         int Camera_Stick_Delta = 2;
         Game_State->Camera.X += (Camera_Stick_Delta * Controller->Stick_Right_X);
         Game_State->Camera.Y += (Camera_Stick_Delta * Controller->Stick_Right_Y);

         Advance_Animation(&Player->Animation, Frame_Seconds, 10.0f);

         if(Was_Pressed(Controller->Start))
         {
            Game_State->Camera.X = Player->Position.X;
            Game_State->Camera.Y = Player->Position.Y;
         }
      }
   }

   dragon *Dragon = &Game_State->Dragon;
   if(Dragon->Animation.Offset_X == 0.0f && Dragon->Animation.Offset_Y == 0.0f)
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

      Move(Map, &Dragon->Position, &Dragon->Animation, Delta_X, Delta_Y);
   }
   Advance_Animation(&Dragon->Animation, Frame_Seconds, 5.0f);

   position Camera = Game_State->Camera;

   u32 Palettes[2][4] = {
      {0x000088FF, 0x0000CCFF, 0x0000FFFF, 0x008800FF},
      {0x008800FF, 0x00CC00FF, 0x00FF00FF, 0x000088FF},
   };
   u32 *Palette = Palettes[Camera.Z];

   Clear(Backbuffer, Palette[0]);

   // TODO: Loop over the surrounding chunks instead of tiles so that we don't
   // have to query for the chunk on each iteration.
   int Min_X = Camera.X - MAP_CHUNK_DIM*2;
   int Max_X = Camera.X + MAP_CHUNK_DIM*2;

   int Min_Y = Camera.Y - MAP_CHUNK_DIM*2;
   int Max_Y = Camera.Y + MAP_CHUNK_DIM*2;

   for(int Y = Min_Y; Y < Max_Y; ++Y)
   {
      for(int X = Min_X; X < Max_X; ++X)
      {
         int Tile = Get_Map_Position_Value(Map, X, Y, Camera.Z);
         int Pixel_X = Tile_Dim_Pixels*(X - Camera.X) + Backbuffer.Width/2;
         int Pixel_Y = Tile_Dim_Pixels*(Y - Camera.Y) + Backbuffer.Height/2;

         if(Tile == 3)
         {
            game_texture Bitmap = (Camera.Z == 0) ? Game_State->Upstairs : Game_State->Downstairs;
            Draw_Bitmap(Backbuffer, Bitmap, Pixel_X, Pixel_Y);
         }
         else
         {
            Draw_Rectangle(Backbuffer, Pixel_X, Pixel_Y, Tile_Dim_Pixels, Tile_Dim_Pixels, Palette[Tile]);
            Draw_Outline(Backbuffer, Pixel_X, Pixel_Y, Tile_Dim_Pixels, Tile_Dim_Pixels, Border_Dim_Pixels, Palette[1]);
         }
      }
   }

   int Player_Pixel_Dim = 2 * Tile_Dim_Pixels;
   for(int Player_Index = 0; Player_Index < Array_Count(Game_State->Players); ++Player_Index)
   {
      player *Player = Game_State->Players + Player_Index;
      if(Player->Active && Player->Position.Z == Camera.Z)
      {
#if 1
         int Offset_X = Tile_Dim_Pixels * Player->Animation.Offset_X;
         int Offset_Y = Tile_Dim_Pixels * Player->Animation.Offset_Y;
#else
         int Offset_X = 0;
         int Offset_Y = 0;
#endif
         int Pixel_X = Tile_Dim_Pixels * (Player->Position.X - Camera.X) + Backbuffer.Width/2  - Offset_X;
         int Pixel_Y = Tile_Dim_Pixels * (Player->Position.Y - Camera.Y) + Backbuffer.Height/2 - Offset_Y;

         Draw_Rectangle(Backbuffer, Pixel_X, Pixel_Y, Player_Pixel_Dim, Player_Pixel_Dim, 0x000088FF);
         Draw_Outline(Backbuffer, Pixel_X, Pixel_Y, Player_Pixel_Dim, Player_Pixel_Dim, 4*Border_Dim_Pixels, 0xFFFFFFFF);

         int Half_Dim = Player_Pixel_Dim/2;
         int Nose_Dim = 4 * Border_Dim_Pixels;
         int Nose_X = Pixel_X + Half_Dim - Nose_Dim/2;
         int Nose_Y = Pixel_Y + Half_Dim - Nose_Dim/2;

         switch(Player->Animation.Direction)
         {
            case Direction_Up:    { Nose_Y -= (Half_Dim - Nose_Dim/2); } break;
            case Direction_Down:  { Nose_Y += (Half_Dim - Nose_Dim/2); } break;
            case Direction_Left:  { Nose_X -= (Half_Dim - Nose_Dim/2); } break;
            case Direction_Right: { Nose_X += (Half_Dim - Nose_Dim/2); } break;
            default: {} break;
         }
         Draw_Rectangle(Backbuffer, Nose_X, Nose_Y, Nose_Dim, Nose_Dim, 0x0000FFFF);
      }
   }

   if(Dragon->Position.Z == Camera.Z)
   {
#if 1
      int Offset_X = Tile_Dim_Pixels * Dragon->Animation.Offset_X;
      int Offset_Y = Tile_Dim_Pixels * Dragon->Animation.Offset_Y;
#else
      int Offset_X = 0;
      int Offset_Y = 0;
#endif

      int Pixel_X = Tile_Dim_Pixels * (Dragon->Position.X - Camera.X) + Backbuffer.Width/2 - Offset_X;
      int Pixel_Y = Tile_Dim_Pixels * (Dragon->Position.Y - Camera.Y) + Backbuffer.Height/2 - Offset_Y;
      Draw_Rectangle(Backbuffer, Pixel_X, Pixel_Y, Player_Pixel_Dim, Player_Pixel_Dim, 0x880000FF);
      Draw_Outline(Backbuffer, Pixel_X, Pixel_Y, Player_Pixel_Dim, Player_Pixel_Dim, 4*Border_Dim_Pixels, 0xFF0000FF);
   }

   int Text_X = 5;
   int Text_Y = 5;
   text_size Text_Size = Text_Size_Large;
   text_font *Font = &Game_State->Varia_Font;

   Advance_Text_Line(Font, Text_Size, &Text_Y);
   Draw_Text(Backbuffer, Font, Text_Size, Text_X, Text_Y, S("Dungeon Simulator"));

   char Data[128];
   string Text = {0};
   Text.Data = (u8 *)Data;
   Text_Size = Text_Size_Medium;
   Font = &Game_State->Fixed_Font;

   Advance_Text_Line(Font, Text_Size, &Text_Y);
   Text.Length = snprintf(Data, sizeof(Data), "Frame Time: %3.3fms", Frame_Seconds*1000.0f);
   Draw_Text(Backbuffer, Font, Text_Size, Text_X, Text_Y, Text);

   Advance_Text_Line(Font, Text_Size, &Text_Y);
   Text.Length = snprintf(Data, sizeof(Data), "Camera: {%d, %d, %d}", Camera.X, Camera.Y, Camera.Z);
   Draw_Text(Backbuffer, Font, Text_Size, Text_X, Text_Y, Text);

   Advance_Text_Line(Font, Text_Size, &Text_Y);
   Text.Length = snprintf(Data, sizeof(Data), "Dragon: {%d, %d, %d}", Dragon->Position.X, Dragon->Position.Y, Dragon->Position.Z);
   Draw_Text(Backbuffer, Font, Text_Size, Text_X, Text_Y, Text);

   for(int Player_Index = 0; Player_Index < Array_Count(Game_State->Players); ++Player_Index)
   {
      player *Player = Game_State->Players + Player_Index;
      if(Player->Active)
      {
         Advance_Text_Line(Font, Text_Size, &Text_Y);
         Text.Length = snprintf(Data, sizeof(Data), "Player %d: {%d, %d, %d}", Player_Index, Player->Position.X, Player->Position.Y, Player->Position.Z);
         Draw_Text(Backbuffer, Font, Text_Size, Text_X, Text_Y, Text);
      }
   }

   int Gui_Dim = 16;
   int Gui_X = Backbuffer.Width - 2*Gui_Dim*GAME_CONTROLLER_COUNT;
   int Gui_Y = Backbuffer.Height - 2*Gui_Dim;

   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      game_controller *Controller = Input->Controllers + Controller_Index;

      if(Controller->Connected)
      {
         Draw_Rectangle(Backbuffer, Gui_X, Gui_Y, Gui_Dim, Gui_Dim, 0x00FF00FF);
      }
      else
      {
         Draw_Rectangle(Backbuffer, Gui_X, Gui_Y, Gui_Dim, Gui_Dim, 0x004400FF);
         Draw_Outline(Backbuffer, Gui_X, Gui_Y, Gui_Dim, Gui_Dim, 2*Border_Dim_Pixels, 0x00FF00FF);
      }
      Gui_X += (2 * Gui_Dim);
   }
}
