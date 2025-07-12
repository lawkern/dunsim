/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include "game.h"
#include "memory.c"

typedef struct {
   arena Arena;

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

UPDATE(Update)
{
   game_state *Game_State = (game_state *)Memory.Data;
   if(!Game_State->Arena.Base)
   {
      Game_State->Arena.Size = 256 * 1024;
      Game_State->Arena.Base = Memory.Data + Game_State->Arena.Size;
      Assert(Game_State->Arena.Size <= Memory.Size);

      Game_State->Player_X = 100;
      Game_State->Player_Y = 100;
   }

   u32 Color = 0xFFFFFFFF;
   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      game_controller *Controller = Input->Controllers + Controller_Index;
      if(Controller->Connected)
      {
         if(Is_Held(Controller->Move_Up))    Game_State->Player_Y -= 10;
         if(Is_Held(Controller->Move_Down))  Game_State->Player_Y += 10;
         if(Is_Held(Controller->Move_Left))  Game_State->Player_X -= 10;
         if(Is_Held(Controller->Move_Right)) Game_State->Player_X += 10;

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
   for(int Tile_Y = 0; Tile_Y < 9; ++Tile_Y)
   {
      int Y = Tile_Y*32 + 2 - Game_State->Camera_Y;
      for(int Tile_X = 0; Tile_X < 12; ++Tile_X)
      {
         int X = Tile_X*32 + 2 - Game_State->Camera_X;
         Draw_Rectangle(Backbuffer, X, Y, 28, 28, 0x0000DDFF);
      }
   }

   int Player_X = Game_State->Player_X - Game_State->Camera_X;
   int Player_Y = Game_State->Player_Y - Game_State->Camera_Y;
   Draw_Rectangle(Backbuffer, Player_X, Player_Y, 32, 32, Color);
}
