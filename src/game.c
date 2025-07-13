/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include "game.h"
#include "text.c"

typedef struct {
   int Positions[16][16];
} map_chunk;

typedef struct {
   int Chunk_Count_X;
   int Chunk_Count_Y;
   map_chunk *Chunks;
} map;

typedef struct {
   arena Arena;
   arena Scratch;

   text_font Font;

   map Map;
   int Player_X;
   int Player_Y;

   int Camera_X;
   int Camera_Y;
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

         float SR = (float)((Source_Pixel >> 24) & 0xFF) / 255.0f;
         float SG = (float)((Source_Pixel >> 16) & 0xFF) / 255.0f;
         float SB = (float)((Source_Pixel >>  8) & 0xFF) / 255.0f;
         float SA = (float)((Source_Pixel >>  0) & 0xFF) / 255.0f;

         float DR = (float)((*Destination_Pixel >> 24) & 0xFF) / 255.0f;
         float DG = (float)((*Destination_Pixel >> 16) & 0xFF) / 255.0f;
         float DB = (float)((*Destination_Pixel >>  8) & 0xFF) / 255.0f;

         u32 R = (u8)((255.0f*DR * (1.0f-SA) + 255.0f*SR) + 0.5f);
         u32 G = (u8)((255.0f*DG * (1.0f-SA) + 255.0f*SG) + 0.5f);
         u32 B = (u8)((255.0f*DB * (1.0f-SA) + 255.0f*SB) + 0.5f);

         *Destination_Pixel = (R<<24) | (G<<16) | (B<<8) | 0xFF;
      }

      Source_Row += Source.Width;
   }
}

static bool Position_Is_Occupied(map *Map, int X, int Y)
{
   bool Result = true;

   int Tile_Count_X = Array_Count(Map->Chunks[0].Positions[0]);
   int Tile_Count_Y = Array_Count(Map->Chunks[0].Positions);

   int Max_X = Map->Chunk_Count_X * Tile_Count_X;
   int Max_Y = Map->Chunk_Count_Y * Tile_Count_Y;

   if(X >= 0 && X < Max_X && Y >= 0 && Y < Max_Y)
   {
      int Chunk_X = X / Tile_Count_X;
      int Chunk_Y = Y / Tile_Count_Y;

      map_chunk *Chunk = Map->Chunks + Chunk_Y*Map->Chunk_Count_X + Chunk_X;
      int Relative_X = X % Tile_Count_X;
      int Relative_Y = Y % Tile_Count_Y;

      Result = (Chunk->Positions[Relative_Y][Relative_X] == 1);
   }

   return(Result);
}

static map_chunk Debug_Map_Chunk = {{
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
      {1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
      {1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
   }};

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

      Map->Chunk_Count_X = 2;
      Map->Chunk_Count_Y = 2;

      int Chunk_Count = Map->Chunk_Count_X * Map->Chunk_Count_Y;
      Map->Chunks = Allocate(Arena, map_chunk, Chunk_Count);

      for(int Chunk_Index = 0; Chunk_Index < Chunk_Count; ++Chunk_Index)
      {
         Map->Chunks[Chunk_Index] = Debug_Map_Chunk;
      }

      Game_State->Player_X = 8;
      Game_State->Player_Y = 8;

      Load_Font(Font, Arena, *Scratch, "data/LiberationSans.ttf", 32);
   }

   u32 Palette[] = {0x0000FFFF, 0x0000DDFF};
   int Tile_Count_X = Array_Count(Debug_Map_Chunk.Positions[0]);
   int Tile_Count_Y = Array_Count(Debug_Map_Chunk.Positions);

   int Tile_Dim_Pixels = Backbuffer.Width / 40;
   int Player_Radius = 1;

   int Player_Chunk_X = Game_State->Player_X / Tile_Count_X;
   int Player_Chunk_Y = Game_State->Player_Y / Tile_Count_Y;

   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      game_controller *Controller = Input->Controllers + Controller_Index;
      if(Controller->Connected)
      {
         bool Dash = Is_Held(Controller->Shoulder_Right);

         bool Up    = Was_Pressed(Controller->Move_Up)    || (Dash && Is_Held(Controller->Move_Up));
         bool Down  = Was_Pressed(Controller->Move_Down)  || (Dash && Is_Held(Controller->Move_Down));
         bool Left  = Was_Pressed(Controller->Move_Left)  || (Dash && Is_Held(Controller->Move_Left));
         bool Right = Was_Pressed(Controller->Move_Right) || (Dash && Is_Held(Controller->Move_Right));

         int New_Player_X = Game_State->Player_X;
         int New_Player_Y = Game_State->Player_Y;

         if(Up)    New_Player_Y -= 1;
         if(Down)  New_Player_Y += 1;
         if(Left)  New_Player_X -= 1;
         if(Right) New_Player_X += 1;

         if(!Position_Is_Occupied(Map, New_Player_X, New_Player_Y) &&
            !Position_Is_Occupied(Map, New_Player_X + 1, New_Player_Y) &&
            !Position_Is_Occupied(Map, New_Player_X, New_Player_Y + 1) &&
            !Position_Is_Occupied(Map, New_Player_X + 1, New_Player_Y + 1))
         {
            Game_State->Player_X = New_Player_X;
            Game_State->Player_Y = New_Player_Y;
         }

         if(Is_Held(Controller->Action_Up))    Game_State->Camera_Y -= 10;
         if(Is_Held(Controller->Action_Down))  Game_State->Camera_Y += 10;
         if(Is_Held(Controller->Action_Left))  Game_State->Camera_X -= 10;
         if(Is_Held(Controller->Action_Right)) Game_State->Camera_X += 10;
      }
   }

   Clear(Backbuffer, 0x0000FFFF);

   for(int Chunk_Y = 0; Chunk_Y < Map->Chunk_Count_Y; ++Chunk_Y)
   {
      for(int Chunk_X = 0; Chunk_X < Map->Chunk_Count_X; ++Chunk_X)
      {
         map_chunk *Chunk = Map->Chunks + Chunk_Y*Map->Chunk_Count_X + Chunk_X;

         int Chunk_Y_Offset = Tile_Dim_Pixels * Chunk_Y * Tile_Count_Y;
         for(int Tile_Y = 0; Tile_Y < Tile_Count_Y; ++Tile_Y)
         {
            int Y = Chunk_Y_Offset + Tile_Y*Tile_Dim_Pixels - Game_State->Camera_Y;
            for(int Tile_X = 0; Tile_X < Tile_Count_X; ++Tile_X)
            {
               int Chunk_X_Offset = Tile_Dim_Pixels * Chunk_X * Tile_Count_X;
               int X = Chunk_X_Offset + Tile_X*Tile_Dim_Pixels - Game_State->Camera_X;
               int Palette_Index = Chunk->Positions[Tile_Y][Tile_X];
               Draw_Rectangle(Backbuffer, X, Y, Tile_Dim_Pixels, Tile_Dim_Pixels, Palette[Palette_Index]);
            }
         }
      }
   }

   int Player_X = Tile_Dim_Pixels*Game_State->Player_X - Game_State->Camera_X;
   int Player_Y = Tile_Dim_Pixels*Game_State->Player_Y - Game_State->Camera_Y;
   Draw_Rectangle(Backbuffer, Player_X, Player_Y, 2*(Tile_Dim_Pixels), 2*(Tile_Dim_Pixels), 0xFFFFFFFF);

   float Text_X = 5.0f;
   float Text_Y = (float)Backbuffer.Height - 10.0f;
   float Line_Advance = Font->Scale * (Font->Ascent - Font->Descent + Font->Line_Gap);

   string Message = S("Dungeon Simulator: Floor 1");
   for(size Index = 0; Index < Message.Length; ++Index)
   {
      int Codepoint = Message.Data[Index];

      game_texture Glyph = Font->Glyphs[Codepoint];
      Draw_Bitmap(Backbuffer, Glyph, Text_X, Text_Y);

      if(Index != Message.Length-1)
      {
         int Next_Codepoint = Message.Data[Index + 1];
         int Pair_Index = (Codepoint * Array_Count(Font->Glyphs)) + Next_Codepoint;
         Text_X += Font->Distances[Pair_Index];
      }
   }

   int Gui_Dim = 16;
   int Gui_X = Backbuffer.Width - 2*Gui_Dim*GAME_CONTROLLER_COUNT;
   int Gui_Y = Backbuffer.Height - 2*Gui_Dim;

   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      game_controller *Controller = Input->Controllers + Controller_Index;

      u32 Color = (Controller->Connected) ? 0xFFFFFFFF : 0x0000CCFF;
      Draw_Rectangle(Backbuffer, Gui_X, Gui_Y, Gui_Dim, Gui_Dim, Color);
      Gui_X += (2 * Gui_Dim);
   }
}
