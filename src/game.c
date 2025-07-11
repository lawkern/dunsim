/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include "game.h"

UPDATE(Update)
{
   u32 Color = 0x0000FFFF;
   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      game_controller *Controller = Input->Controllers + Controller_Index;
      if(Is_Held(Controller->Action_Down))
      {
         Color = 0x00FF00FF;
      }
   }

   for(int Y = 0; Y < Backbuffer->Height; ++Y)
   {
      for(int X = 0; X < Backbuffer->Width; ++X)
      {
         Backbuffer->Memory[(Backbuffer->Width * Y) + X] = Color;
      }
   }
}
