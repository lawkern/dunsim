/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include "game.h"

#include "math.c"
#include "text.c"
#include "map.c"

typedef struct {
   int X;
   int Y;
   int Z;

   float Animation_Offset_X;
   float Animation_Offset_Y;

   bool Active;
} player;

typedef struct {
   arena Arena;
   arena Scratch;

   text_font Font;

   map Map;
   player Players[GAME_CONTROLLER_COUNT];

   int Camera_X;
   int Camera_Y;
   int Camera_Z;
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

static void Draw_Text(game_texture Destination, text_font *Font, int X, int Y, string Text)
{
   // float Line_Advance = Font->Scale * (Font->Ascent - Font->Descent + Font->Line_Gap);

   for(size Index = 0; Index < Text.Length; ++Index)
   {
      int Codepoint = Text.Data[Index];

      game_texture Glyph = Font->Glyphs[Codepoint];
      Draw_Bitmap(Destination, Glyph, X, Y);

      if(Index != Text.Length-1)
      {
         int Next_Codepoint = Text.Data[Index + 1];
         int Pair_Index = (Codepoint * Array_Count(Font->Glyphs)) + Next_Codepoint;
         X += Font->Distances[Pair_Index];
      }
   }
}

static bool Player_Can_Move(map *Map, player *Player, int Delta_X, int Delta_Y)
{
   bool Result = false;

   int New_Player_X = Player->X + Delta_X;
   int New_Player_Y = Player->Y + Delta_Y;
   int New_Player_Z = Player->Z;

   int Tile_00 = Get_Map_Position_Value(Map, New_Player_X, New_Player_Y, New_Player_Z);
   int Tile_01 = Get_Map_Position_Value(Map, New_Player_X + 1, New_Player_Y, New_Player_Z);
   int Tile_10 = Get_Map_Position_Value(Map, New_Player_X, New_Player_Y + 1, New_Player_Z);
   int Tile_11 = Get_Map_Position_Value(Map, New_Player_X + 1, New_Player_Y + 1, New_Player_Z);

   if(!Position_Is_Occupied(Tile_00) &&
      !Position_Is_Occupied(Tile_01) &&
      !Position_Is_Occupied(Tile_10) &&
      !Position_Is_Occupied(Tile_11))
   {
      Result = true;
   }

   return(Result);
}

static bool Player_Can_Ascend(map *Map, player *Player)
{
   bool Result = false;

   int Tile_00 = Get_Map_Position_Value(Map, Player->X, Player->Y, Player->Z);
   int Tile_01 = Get_Map_Position_Value(Map, Player->X + 1, Player->Y, Player->Z);
   int Tile_10 = Get_Map_Position_Value(Map, Player->X, Player->Y + 1, Player->Z);
   int Tile_11 = Get_Map_Position_Value(Map, Player->X + 1, Player->Y + 1, Player->Z);

   if(Tile_00 == 3 &&
      Tile_01 == 3 &&
      Tile_10 == 3 &&
      Tile_11 == 3)
   {
      Result = true;
   }

   return(Result);
}

UPDATE(Update)
{
   game_state *Game_State = (game_state *)Memory.Base;

   arena *Arena = &Game_State->Arena;
   arena *Scratch = &Game_State->Scratch;
   text_font *Font = &Game_State->Font;
   map *Map = &Game_State->Map;

   if(!Arena->Base)
   {
      Arena->Size = Megabytes(64);
      Arena->Base = Memory.Base + sizeof(*Game_State);

      Scratch->Size = Memory.Size - Arena->Size;
      Scratch->Base = Arena->Base + Arena->Size;

      Assert((Scratch->Base + Scratch->Size) >= (Memory.Base + Memory.Size));

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

      Game_State->Camera_X = 2;
      Game_State->Camera_Y = 2;

      for(int Player_Index = 0; Player_Index < Array_Count(Game_State->Players); ++Player_Index)
      {
         player *Player = Game_State->Players + Player_Index;
         Player->X = Game_State->Camera_X;
         Player->Y = Game_State->Camera_Y;
      }

      Load_Font(Font, Arena, *Scratch, "data/LiberationSans.ttf", 32);
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
         bool Player_Animating = (Player->Animation_Offset_X != 0.0f || Player->Animation_Offset_Y != 0.0f);

         if(Moving_Camera)
         {
            int Camera_Delta = 4;
            if(Is_Held(Controller->Shoulder_Right))
            {
               if(Was_Pressed(Controller->Move_Up))    Game_State->Camera_Y -= Camera_Delta;
               if(Was_Pressed(Controller->Move_Down))  Game_State->Camera_Y += Camera_Delta;
               if(Was_Pressed(Controller->Move_Left))  Game_State->Camera_X -= Camera_Delta;
               if(Was_Pressed(Controller->Move_Right)) Game_State->Camera_X += Camera_Delta;
            }
         }
         else if(!Player_Animating)
         {
            bool Dash = Is_Held(Controller->Action_Down);
            bool Up    = Is_Held(Controller->Move_Up)    || (Dash && Is_Held(Controller->Move_Up));
            bool Down  = Is_Held(Controller->Move_Down)  || (Dash && Is_Held(Controller->Move_Down));
            bool Left  = Is_Held(Controller->Move_Left)  || (Dash && Is_Held(Controller->Move_Left));
            bool Right = Is_Held(Controller->Move_Right) || (Dash && Is_Held(Controller->Move_Right));

            int Delta_X = 0;
            int Delta_Y = 0;

            if(Up)    Delta_Y -= 1;
            if(Down)  Delta_Y += 1;
            if(Left)  Delta_X -= 1;
            if(Right) Delta_X += 1;

            // TODO: This includes a number of redundant checks just to support
            // sliding along walls when moving into them diagonally. It's not
            // clear we even want to support diagonal movement, so this should
            // all be cleaned up.
            bool Can_Move = Player_Can_Move(Map, Player, Delta_X, Delta_Y);
            if(!Can_Move && Delta_X && Delta_Y)
            {
               Can_Move = Player_Can_Move(Map, Player, Delta_X, 0);
               if(Can_Move)
               {
                  Delta_Y = 0;
               }
               else
               {
                  Can_Move = Player_Can_Move(Map, Player, 0, Delta_Y);
                  if(Can_Move)
                  {
                     Delta_X = 0;
                  }
               }
            }

            if(Can_Move && (Delta_X || Delta_Y))
            {
               Player->Animation_Offset_X = (float)Delta_X;
               Player->Animation_Offset_Y = (float)Delta_Y;

               Player->X += Delta_X;
               Player->Y += Delta_Y;

               if(Player_Can_Ascend(Map, Player))
               {
                  if(Player->Z == 0)
                  {
                     Player->Z = 1;
                     Game_State->Camera_Z = 1;
                  }
                  else
                  {
                     Player->Z = 0;
                     Game_State->Camera_Z = 0;
                  }
               }
            }
         }

         int Camera_Stick_Delta = 4;
         Game_State->Camera_X += (Camera_Stick_Delta * Controller->Stick_Right_X);
         Game_State->Camera_Y += (Camera_Stick_Delta * Controller->Stick_Right_Y);

         float Delta = 10.0f * Frame_Seconds;
         if(Player->Animation_Offset_X > 0)
         {
            Player->Animation_Offset_X -= Delta;
            if(Player->Animation_Offset_X < 0) Player->Animation_Offset_X = 0;
         }
         else if(Player->Animation_Offset_X < 0)
         {
            Player->Animation_Offset_X += Delta;
            if(Player->Animation_Offset_X > 0) Player->Animation_Offset_X = 0;
         }

         if(Player->Animation_Offset_Y > 0)
         {
            Player->Animation_Offset_Y -= Delta;
            if(Player->Animation_Offset_Y < 0) Player->Animation_Offset_Y = 0;
         }
         else if(Player->Animation_Offset_Y < 0)
         {
            Player->Animation_Offset_Y += Delta;
            if(Player->Animation_Offset_Y > 0) Player->Animation_Offset_Y = 0;
         }

         if(Was_Pressed(Controller->Start))
         {
            Game_State->Camera_X = Player->X;
            Game_State->Camera_Y = Player->Y;
         }
      }
   }

   int Cam_X = Game_State->Camera_X;
   int Cam_Y = Game_State->Camera_Y;
   int Cam_Z = Game_State->Camera_Z;

   u32 Palettes[2][4] = {
      {0x000088FF, 0x0000CCFF, 0x0000FFFF, 0x008800FF},
      {0x008800FF, 0x00CC00FF, 0x00FF00FF, 0x000088FF},
   };
   u32 *Palette = Palettes[Cam_Z];

   Clear(Backbuffer, Palette[0]);

   // TODO: Loop over the surrounding chunks instead of tiles so that we don't
   // have to query for the chunk on each iteration.
   int Min_X = Game_State->Camera_X - MAP_CHUNK_DIM*2;
   int Max_X = Game_State->Camera_X + MAP_CHUNK_DIM*2;

   int Min_Y = Game_State->Camera_Y - MAP_CHUNK_DIM*2;
   int Max_Y = Game_State->Camera_Y + MAP_CHUNK_DIM*2;

   for(int Y = Min_Y; Y < Max_Y; ++Y)
   {
      for(int X = Min_X; X < Max_X; ++X)
      {
         int Tile = Get_Map_Position_Value(Map, X, Y, Cam_Z);
         int Pixel_X = Tile_Dim_Pixels*(X - Cam_X) + Backbuffer.Width/2;
         int Pixel_Y = Tile_Dim_Pixels*(Y - Cam_Y) + Backbuffer.Height/2;

         Draw_Rectangle(Backbuffer, Pixel_X, Pixel_Y, Tile_Dim_Pixels, Tile_Dim_Pixels, Palette[Tile]);
         if(Tile)
         {
            Draw_Outline(Backbuffer, Pixel_X, Pixel_Y, Tile_Dim_Pixels, Tile_Dim_Pixels, Border_Dim_Pixels, Palette[1]);
         }
      }
   }

   int Player_Pixel_Dim = 2 * Tile_Dim_Pixels;
   for(int Player_Index = 0; Player_Index < Array_Count(Game_State->Players); ++Player_Index)
   {
      player *Player = Game_State->Players + Player_Index;
      if(Player->Active && Player->Z == Cam_Z)
      {
#if 0
         int Offset_X = Tile_Dim_Pixels * Player->Animation_Offset_X;
         int Offset_Y = Tile_Dim_Pixels * Player->Animation_Offset_Y;
#else
         int Offset_X = 0;
         int Offset_Y = 0;
#endif
         int Pixel_X = Tile_Dim_Pixels * (Player->X - Game_State->Camera_X) + Backbuffer.Width/2  - Offset_X;
         int Pixel_Y = Tile_Dim_Pixels * (Player->Y - Game_State->Camera_Y) + Backbuffer.Height/2 - Offset_Y;

         Draw_Rectangle(Backbuffer, Pixel_X, Pixel_Y, Player_Pixel_Dim, Player_Pixel_Dim, 0x000088FF);
         Draw_Outline(Backbuffer, Pixel_X, Pixel_Y, Player_Pixel_Dim, Player_Pixel_Dim, 4*Border_Dim_Pixels, 0xFFFFFFFF);
      }
   }

   int Text_X = 5;
   int Text_Y = Backbuffer.Height - 10;
   Draw_Text(Backbuffer, Font, Text_X, Text_Y, (Cam_Z == 0)
             ? S("Dungeon Simulator: Floor 1")
             : S("Dungeon Simulator: Floor 2"));

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
