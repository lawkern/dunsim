/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include "game.h"
#include "text.c"

typedef struct {
   arena Arena;
   arena Scratch;

   text_font Font;

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

UPDATE(Update)
{
   game_state *Game_State = (game_state *)Memory.Base;

   arena *Arena = &Game_State->Arena;
   arena *Scratch = &Game_State->Scratch;
   text_font *Font = &Game_State->Font;

   if(!Arena->Base)
   {
      Arena->Size = Megabytes(64);
      Arena->Base = Memory.Base + sizeof(*Game_State);

      Scratch->Size = Memory.Size - Arena->Size;
      Scratch->Base = Arena->Base + Arena->Size;

      Assert((Scratch->Base + Scratch->Size) >= (Memory.Base + Memory.Size));

      Game_State->Player_X = 64;
      Game_State->Player_Y = 64;

      Load_Font(Font, Arena, *Scratch, "data/LiberationSans.ttf", 32);
   }

   int Tile_Count_X = 12;
   int Tile_Count_Y = 12;
   int Tiles[] =
      {
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
         0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
         0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0,
         0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
         0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
         0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
         0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
         0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,
         0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
         0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      };
   int Tile_Dim = Backbuffer.Width / 20;
   int Border = 2;
   int Draw_Dim = Tile_Dim - Border*2;

   u32 Color = 0xFFFFFFFF;
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

         if(Up)    Game_State->Player_Y -= Tile_Dim/2;
         if(Down)  Game_State->Player_Y += Tile_Dim/2;
         if(Left)  Game_State->Player_X -= Tile_Dim/2;
         if(Right) Game_State->Player_X += Tile_Dim/2;

         if(Is_Held(Controller->Action_Up))    Game_State->Camera_Y -= 10;
         if(Is_Held(Controller->Action_Down))  Game_State->Camera_Y += 10;
         if(Is_Held(Controller->Action_Left))  Game_State->Camera_X -= 10;
         if(Is_Held(Controller->Action_Right)) Game_State->Camera_X += 10;

         if(Is_Held(Controller->Start))
         {
            Color = 0x00FF00FF;
         }
      }
   }

   Clear(Backbuffer, 0x0000FFFF);
   for(int Tile_Y = 0; Tile_Y < Tile_Count_Y; ++Tile_Y)
   {
      int Y = Tile_Y*Tile_Dim + Border - Game_State->Camera_Y;
      for(int Tile_X = 0; Tile_X < Tile_Count_X; ++Tile_X)
      {
         int X = Tile_X*Tile_Dim + Border - Game_State->Camera_X;
         Draw_Rectangle(Backbuffer, X, Y, Draw_Dim, Draw_Dim, (Tiles[Tile_Y*12 + Tile_X]) ? 0x0000BBFF : 0x0000DDFF);
      }
   }

   int Player_X = Game_State->Player_X - Game_State->Camera_X;
   int Player_Y = Game_State->Player_Y - Game_State->Camera_Y;
   Draw_Rectangle(Backbuffer, Player_X + Border, Player_Y + Border, Draw_Dim, Draw_Dim, Color);

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
}
