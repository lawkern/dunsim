/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include "SDL3/SDL.h"
#include "game.h"

static struct {
   SDL_Window *Window;
   SDL_Renderer *Renderer;
   SDL_Texture *Texture;
   SDL_Gamepad *Controllers[GAME_CONTROLLER_COUNT];

   Uint64 Frequency;
   Uint64 Frame_Start;
   Uint64 Frame_Count;

   int Monitor_Refresh_Rate;
   float Target_Frame_Seconds;
   float Actual_Frame_Seconds;
} Sdl;

static void Sdl_Process_Button(game_button *Button, bool Pressed)
{
   Button->Pressed = Pressed;
   Button->Transitioned = true;
}

static int Sdl_Get_Controller_Index(SDL_JoystickID ID)
{
   int Result = -1;

   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      SDL_Gamepad *Test = Sdl.Controllers[Controller_Index];
      if(Test)
      {
         SDL_Joystick *Joystick = SDL_GetGamepadJoystick(Test);
         if(ID == SDL_GetJoystickID(Joystick))
         {
            Result = Controller_Index;
            break;
         }
      }
   }

   return(Result);
}

static int Sdl_Get_Monitor_Refresh_Rate(void)
{
   int Result = 60;

   int Display_Count;
   SDL_DisplayID *Displays = SDL_GetDisplays(&Display_Count);
   if(Displays)
   {
      for(int Display_Index = 0; Display_Index < Display_Count; ++Display_Index)
      {
         SDL_DisplayID ID = Displays[Display_Index];
         if(ID)
         {
            // TODO: Determine which display to use when Display_Count > 1.
            const SDL_DisplayMode *Mode = SDL_GetDesktopDisplayMode(ID);
            if(Mode)
            {
               if(Mode->refresh_rate > 0)
               {
                  Result = Mode->refresh_rate;
                  break;
               }
            }
            else
            {
               SDL_Log("Failed to get desktop display mode: %s", SDL_GetError());
            }
         }
      }
      SDL_free(Displays);
   }
   else
   {
      SDL_Log("Failed to count desktop displays: %s", SDL_GetError());
   }

   return(Result);
}

int main(void)
{
   // Initialization.
   int Width = 420;
   int Height = 360;

   if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD))
   {
      SDL_Log("Failed to initialize SDL3: %s", SDL_GetError());
      SDL_assert(0);
   }

   if(!SDL_CreateWindowAndRenderer("SDL Window", Width, Height, 0, &Sdl.Window, &Sdl.Renderer))
   {
      SDL_Log("Failed to create window/renderer: %s", SDL_GetError());
      SDL_assert(0);
   }

   if(!SDL_SetRenderVSync(Sdl.Renderer, 1))
   {
      SDL_Log("Failed to set vsync: %s", SDL_GetError());
   }

   Sdl.Texture = SDL_CreateTexture(Sdl.Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, Width, Height);
   if(!Sdl.Texture)
   {
      SDL_Log("Failed to create SDL texture: %s", SDL_GetError());
      SDL_assert(0);
   }

   Sdl.Frequency = SDL_GetPerformanceFrequency();
   Sdl.Monitor_Refresh_Rate = Sdl_Get_Monitor_Refresh_Rate();
   Sdl.Target_Frame_Seconds = 1.0f / Sdl.Monitor_Refresh_Rate;

   SDL_Log("Monitor refresh rate: %d", Sdl.Monitor_Refresh_Rate);
   SDL_Log("Target frame time: %0.03fms", Sdl.Target_Frame_Seconds * 1000.0f);

   game_texture Backbuffer = {0};
   Backbuffer.Width = Width;
   Backbuffer.Height = Height;
   Backbuffer.Memory = SDL_calloc(Backbuffer.Width*Backbuffer.Height, sizeof(*Backbuffer.Memory));
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
               int Controller_Index = Sdl_Get_Controller_Index(Event.gdevice.which);
               if(Controller_Index >= 0)
               {
                  game_controller *Controller = Input->Controllers + Controller_Index;
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
               }
            } break;

            case SDL_EVENT_GAMEPAD_ADDED: {
               // NOTE: Find the first available controller slot and store the controller
               // pointer. The indices for Sdl.Controllers and Input->Controllers must be
               // manually maintained.

               for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
               {
                  if(Sdl.Controllers[Controller_Index] == 0 && SDL_IsGamepad(Controller_Index))
                  {
                     Sdl.Controllers[Controller_Index] = SDL_OpenGamepad(Controller_Index);
                     if(Sdl.Controllers[Controller_Index])
                     {
                        Input->Controllers[Controller_Index].Connected = true;
                        SDL_Log("Controller added at slot %d\n", Controller_Index);
                     }
                     else
                     {
                        SDL_Log("Failed to open gamepad: %s\n", SDL_GetError());
                     }

                     // TODO: Should we only break on success? Look into what types of
                     // errors are actually caught here.
                     break;
                  }
               }
            } break;

            case SDL_EVENT_GAMEPAD_REMOVED: {
               int Controller_Index = Sdl_Get_Controller_Index(Event.cdevice.which);
               if(Controller_Index >= 0)
               {
                  SDL_assert(Controller_Index < GAME_CONTROLLER_COUNT);
                  SDL_assert(Sdl.Controllers[Controller_Index]);

                  SDL_CloseGamepad(Sdl.Controllers[Controller_Index]);

                  Sdl.Controllers[Controller_Index] = 0;
                  Input->Controllers[Controller_Index].Connected = false;

                  SDL_Log("Controller removed from slot %d\n", Controller_Index);
               }
            } break;
         }
      }

      // Update game state.
      Update(&Backbuffer, Input);

      // Render frame.
      SDL_SetRenderDrawColor(Sdl.Renderer, 0, 0, 0, 255);
      SDL_RenderClear(Sdl.Renderer);

      int Dst_Width, Dst_Height;
      SDL_GetCurrentRenderOutputSize(Sdl.Renderer, &Dst_Width, &Dst_Height);

      float Src_Aspect = (float)Width / (float)Height;
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

      if((Sdl.Frame_Count % Sdl.Monitor_Refresh_Rate) == 0)
      {
         float Frame_ms = Sdl.Actual_Frame_Seconds * 1000.0f;
         SDL_Log("Frame time: % .3fms (slept %dms)\n", Frame_ms, Sleep_ms);
      }
   }

   return(0);
}
