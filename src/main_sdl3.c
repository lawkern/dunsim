/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include "SDL3/SDL.h"
#include "game.h"

static struct {
   SDL_Window *Window;
   SDL_Renderer *Renderer;
   SDL_Texture *Texture;
   SDL_Gamepad *Gamepads[GAME_CONTROLLER_COUNT];

   Uint64 Frequency;
   Uint64 Frame_Start;
   Uint64 Frame_Count;

   float Monitor_Refresh_Rate;
   float Target_Frame_Seconds;
   float Actual_Frame_Seconds;
} Sdl;

static void Sdl_Process_Button(game_button *Button, bool Pressed)
{
   Button->Pressed = Pressed;
   Button->Transitioned = true;
}

static int Sdl_Get_Gamepad_Index(SDL_JoystickID ID)
{
   int Result = 0;
   for(int Gamepad_Index = 1; Gamepad_Index < GAME_CONTROLLER_COUNT; ++Gamepad_Index)
   {
      SDL_Gamepad *Gamepad = Sdl.Gamepads[Gamepad_Index];
      if(Gamepad)
      {
         SDL_Joystick *Joystick = SDL_GetGamepadJoystick(Gamepad);
         if(ID == SDL_GetJoystickID(Joystick))
         {
            Result = Gamepad_Index;
            break;
         }
      }
   }
   return(Result);
}

int main(void)
{
   // Initialize SDL.
   int Window_Width = 640;
   int Window_Height = 480;

   if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD))
   {
      SDL_Log("Failed to initialize SDL3: %s", SDL_GetError());
      SDL_assert(0);
   }

   if(!SDL_CreateWindowAndRenderer("SDL Platform Build", Window_Width, Window_Height, 0, &Sdl.Window, &Sdl.Renderer))
   {
      SDL_Log("Failed to create window/renderer: %s", SDL_GetError());
      SDL_assert(0);
   }

   if(!SDL_SetRenderVSync(Sdl.Renderer, 1))
   {
      SDL_Log("Failed to set vsync: %s", SDL_GetError());
   }

   int Texture_Width, Texture_Height;
   if(!SDL_GetWindowSizeInPixels(Sdl.Window, &Texture_Width, &Texture_Height))
   {
      SDL_Log("Failed to get window size: %s", SDL_GetError());
      Texture_Width = Window_Width;
      Texture_Height = Window_Height;
   }

   Sdl.Texture = SDL_CreateTexture(Sdl.Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, Texture_Width, Texture_Height);
   if(!Sdl.Texture)
   {
      SDL_Log("Failed to create SDL texture: %s", SDL_GetError());
      SDL_assert(0);
   }

   Sdl.Frequency = SDL_GetPerformanceFrequency();

   int Display_ID = SDL_GetDisplayForWindow(Sdl.Window);
   const SDL_DisplayMode *Display_Mode = SDL_GetCurrentDisplayMode(Display_ID);
   Sdl.Monitor_Refresh_Rate = (Display_Mode && Display_Mode->refresh_rate > 0)
      ? Display_Mode->refresh_rate
      : 60.0f;
   Sdl.Target_Frame_Seconds = 1.0f / Sdl.Monitor_Refresh_Rate;

   SDL_Log("Monitor refresh rate: %02f", Sdl.Monitor_Refresh_Rate);
   SDL_Log("Target frame time: %0.03fms", Sdl.Target_Frame_Seconds * 1000.0f);

   // Initialize game.
   game_memory Memory = {0};
   Memory.Size = 64 * 1024 * 1024;
   Memory.Data = SDL_calloc(1, Memory.Size);
   SDL_assert(Memory.Data);

   game_texture Backbuffer = {0};
   Backbuffer.Width = Texture_Width;
   Backbuffer.Height = Texture_Height;
   Backbuffer.Memory = SDL_calloc(Texture_Width*Texture_Height, sizeof(*Backbuffer.Memory));
   SDL_assert(Backbuffer.Memory);

   int Input_Index = 0;
   game_input Inputs[16] = {0};

   // Main loop.
   bool Running = true;
   while(Running)
   {
      game_input *Input = Inputs + Input_Index;

      // Process input loop.
      SDL_Event Event;
      while(SDL_PollEvent(&Event))
      {
         switch(Event.type)
         {
            case SDL_EVENT_QUIT: {
               Running = false;
            } break;

            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_KEY_DOWN: {
               game_controller *Keyboard = Input->Controllers + 0;
               Keyboard->Connected = true;

               bool Pressed = Event.key.down;
               switch(Event.key.key)
               {
                  case SDLK_ESCAPE: {
                     Running = false;
                  } break;

                  case SDLK_F: {
                     if(Pressed)
                     {
                        bool Is_Fullscreen = (SDL_GetWindowFlags(Sdl.Window) & SDL_WINDOW_FULLSCREEN);
                        SDL_SetWindowFullscreen(Sdl.Window, Is_Fullscreen ? 0 : SDL_WINDOW_FULLSCREEN);
                     }
                  } break;

                  case SDLK_I:         {Sdl_Process_Button(&Keyboard->Action_Up, Pressed);} break;
                  case SDLK_K:         {Sdl_Process_Button(&Keyboard->Action_Down, Pressed);} break;
                  case SDLK_J:         {Sdl_Process_Button(&Keyboard->Action_Left, Pressed);} break;
                  case SDLK_L:         {Sdl_Process_Button(&Keyboard->Action_Right, Pressed);} break;
                  case SDLK_W:         {Sdl_Process_Button(&Keyboard->Move_Up, Pressed);} break;
                  case SDLK_S:         {Sdl_Process_Button(&Keyboard->Move_Down, Pressed);} break;
                  case SDLK_A:         {Sdl_Process_Button(&Keyboard->Move_Left, Pressed);} break;
                  case SDLK_D:         {Sdl_Process_Button(&Keyboard->Move_Right, Pressed);} break;
                  case SDLK_Q:         {Sdl_Process_Button(&Keyboard->Shoulder_Left, Pressed);} break;
                  case SDLK_E:         {Sdl_Process_Button(&Keyboard->Shoulder_Right, Pressed);} break;
                  case SDLK_SPACE:     {Sdl_Process_Button(&Keyboard->Start, Pressed);} break;
                  case SDLK_BACKSPACE: {Sdl_Process_Button(&Keyboard->Back, Pressed);} break;
               }
            } break;

            case SDL_EVENT_GAMEPAD_BUTTON_UP:
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
               int Gamepad_Index = Sdl_Get_Gamepad_Index(Event.gdevice.which);
               SDL_assert(Gamepad_Index > 0);
               SDL_assert(Gamepad_Index < GAME_CONTROLLER_COUNT);

               game_controller *Controller = Input->Controllers + Gamepad_Index;
               bool Pressed = Event.gbutton.down;
               switch(Event.gbutton.button)
               {
                  // TODO: Confirm if other controllers map buttons on based name or position.
                  case SDL_GAMEPAD_BUTTON_SOUTH:          {Sdl_Process_Button(&Controller->Action_Down, Pressed);} break;
                  case SDL_GAMEPAD_BUTTON_EAST:           {Sdl_Process_Button(&Controller->Action_Right, Pressed);} break;
                  case SDL_GAMEPAD_BUTTON_WEST:           {Sdl_Process_Button(&Controller->Action_Left, Pressed);} break;
                  case SDL_GAMEPAD_BUTTON_NORTH:          {Sdl_Process_Button(&Controller->Action_Up, Pressed);} break;
                  case SDL_GAMEPAD_BUTTON_DPAD_UP:        {Sdl_Process_Button(&Controller->Move_Up, Pressed);} break;
                  case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      {Sdl_Process_Button(&Controller->Move_Down, Pressed);} break;
                  case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      {Sdl_Process_Button(&Controller->Move_Left, Pressed);} break;
                  case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     {Sdl_Process_Button(&Controller->Move_Right, Pressed);} break;
                  case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  {Sdl_Process_Button(&Controller->Shoulder_Left, Pressed);} break;
                  case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: {Sdl_Process_Button(&Controller->Shoulder_Right, Pressed);} break;
                  case SDL_GAMEPAD_BUTTON_START:          {Sdl_Process_Button(&Controller->Start, Pressed);} break;
                  case SDL_GAMEPAD_BUTTON_BACK:           {Sdl_Process_Button(&Controller->Back, Pressed);} break;
               }
            } break;

            case SDL_EVENT_GAMEPAD_ADDED: {
               SDL_JoystickID ID = Event.gdevice.which;
               if(SDL_IsGamepad(ID))
               {
                  for(int Gamepad_Index = 1; Gamepad_Index < GAME_CONTROLLER_COUNT; ++Gamepad_Index)
                  {
                     if(!Sdl.Gamepads[Gamepad_Index])
                     {
                        Sdl.Gamepads[Gamepad_Index] = SDL_OpenGamepad(ID);
                        if(Sdl.Gamepads[Gamepad_Index])
                        {
                           Input->Controllers[Gamepad_Index].Connected = true;
                           SDL_Log("Gamepad added to slot %d\n", Gamepad_Index);
                        }
                        else
                        {
                           SDL_Log("Failed to add gamepad: %s\n", SDL_GetError());
                        }

                        break;
                     }
                  }
               }
               else
               {
                  SDL_Log("This joystick is not supported by SDL's gamepad interface.");
               }
            } break;

            case SDL_EVENT_GAMEPAD_REMOVED: {
               SDL_JoystickID ID = Event.gdevice.which;
               if(SDL_IsGamepad(ID))
               {
                  int Gamepad_Index = Sdl_Get_Gamepad_Index(ID);
                  SDL_assert(Gamepad_Index > 0);
                  SDL_assert(Gamepad_Index < GAME_CONTROLLER_COUNT);
                  SDL_assert(Sdl.Gamepads[Gamepad_Index]);

                  SDL_CloseGamepad(Sdl.Gamepads[Gamepad_Index]);

                  Sdl.Gamepads[Gamepad_Index] = 0;
                  Input->Controllers[Gamepad_Index].Connected = false;

                  SDL_Log("Gamepad removed from slot %d\n", Gamepad_Index);
               }
               else
               {
                  SDL_Log("This joystick is not supported by SDL's gamepad interface.");
               }
            } break;
         }
      }

      // Update game state.
      Update(Memory, Backbuffer, Input);

      // Render frame.
      SDL_SetRenderDrawColor(Sdl.Renderer, 0, 0, 0, 255);
      SDL_RenderClear(Sdl.Renderer);

      int Dst_Width, Dst_Height;
      SDL_GetCurrentRenderOutputSize(Sdl.Renderer, &Dst_Width, &Dst_Height);

      float Src_Aspect = (float)Texture_Width / (float)Texture_Height;
      float Dst_Aspect = (float)Dst_Width / (float)Dst_Height;
      SDL_FRect Dst_Rect = {0, 0, (float)Dst_Width, (float)Dst_Height};
      if(Src_Aspect > Dst_Aspect)
      {
         // NOTE: Bars on top and bottom.
         int bar_height = (int)(0.5f * (Dst_Height - (Dst_Width / Src_Aspect)));
         Dst_Rect.y += bar_height;
         Dst_Rect.h -= (bar_height * 2);
      }
      else if(Src_Aspect < Dst_Aspect)
      {
         // NOTE: Bars on left and right;
         int bar_width = (int)(0.5f * (Dst_Width - (Dst_Height * Src_Aspect)));
         Dst_Rect.x += bar_width;
         Dst_Rect.w -= (bar_width * 2);
      }

      SDL_UpdateTexture(Sdl.Texture, 0, Backbuffer.Memory, Backbuffer.Width * sizeof(*Backbuffer.Memory));
      SDL_RenderTexture(Sdl.Renderer, Sdl.Texture, 0, &Dst_Rect);
      SDL_RenderPresent(Sdl.Renderer);

      // End of frame.
      game_input *Next_Input = Inputs + Input_Index;
      *Next_Input = *Input;
      for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
      {
         game_controller *Next = Next_Input->Controllers + Controller_Index;
         for(int Button_Index = 0; Button_Index < GAME_BUTTON_COUNT; ++Button_Index)
         {
            Next->Buttons[Button_Index].Transitioned = false;
         }
      }

      Uint64 Delta = SDL_GetPerformanceCounter() - Sdl.Frame_Start;
      float Actual_Frame_Seconds = (float)Delta / (float)Sdl.Frequency;

      int Sleep_ms = 0;
      if(Actual_Frame_Seconds < Sdl.Target_Frame_Seconds)
      {
         Sleep_ms = (int)((Sdl.Target_Frame_Seconds - Actual_Frame_Seconds) * 1000.0f) - 1;
         if(Sleep_ms > 0)
         {
            SDL_Delay(Sleep_ms);
         }
      }
      while(Actual_Frame_Seconds < Sdl.Target_Frame_Seconds)
      {
         Uint64 Delta = SDL_GetPerformanceCounter() - Sdl.Frame_Start;
         Actual_Frame_Seconds = (float)Delta / (float)Sdl.Frequency;
      }

      Sdl.Frame_Start = SDL_GetPerformanceCounter();
      Sdl.Actual_Frame_Seconds = Actual_Frame_Seconds;
      Sdl.Frame_Count++;

#if DEBUG
      int FPS = (int)(Sdl.Monitor_Refresh_Rate + 0.5f);
      if((Sdl.Frame_Count % FPS) == 0)
      {
         float Frame_ms = Sdl.Actual_Frame_Seconds * 1000.0f;
         SDL_Log("Frame time: % .3fms (slept %dms)\n", Frame_ms, Sleep_ms);
      }
#endif
   }

   return(0);
}
