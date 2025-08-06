/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include "SDL3/SDL.h"
#include "SDL3/SDL_opengl.h"
#include "game.c"

LOG(Log)
{
   va_list Arguments;
   va_start(Arguments, Format);
   SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, Format, Arguments);
   va_end(Arguments);
}

READ_ENTIRE_FILE(Read_Entire_File)
{
   string Result = {0};

   size_t Size = 0;
   void *Data = SDL_LoadFile(Path, &Size);
   if(Data && Size)
   {
      Result.Data = Allocate(Arena, u8, Size + 1);
      SDL_memcpy(Result.Data, Data, Size + 1);
      Result.Length = Size;

      SDL_free(Data);
   }
   else
   {
      SDL_Log("Failed to read file: %s", Path);
   }

   return(Result);
}

WRITE_ENTIRE_FILE(Write_Entire_File)
{
   bool Result = SDL_SaveFile(Path, Memory, Size);
   if(!Result)
   {
      SDL_Log("Failed to write file %s: %s", Path, SDL_GetError());
   }

   return(Result);
}

ENQUEUE_WORK(Enqueue_Work)
{
   u32 New_Write_Index = (Queue->Write_Index + 1) % Array_Count(Queue->Entries);
   SDL_assert(New_Write_Index != Queue->Read_Index);

   work_queue_entry *Entry = Queue->Entries + Queue->Write_Index;
   Entry->Data = Data;
   Entry->Task = Task;

   Queue->Completion_Target++;

   Write_Barrier();

   Queue->Write_Index = New_Write_Index;

   SDL_SignalSemaphore(Queue->Semaphore);
}

static bool Sdl3_Dequeue_Work(work_queue *Queue)
{
   // NOTE: Return whether this thread should be made to wait until more work
   // becomes available.

   u32 Read_Index = Queue->Read_Index;
   u32 New_Read_Index = (Read_Index + 1) % Array_Count(Queue->Entries);
   if(Read_Index == Queue->Write_Index)
   {
      return(true);
   }

   u32 Index = Atomic_Compare_Exchange(&Queue->Read_Index, Read_Index, New_Read_Index);
   if(Index == Read_Index)
   {
      work_queue_entry Entry = Queue->Entries[Index];
      Entry.Task(Entry.Data);

      Atomic_Add(&Queue->Completion_Count, 1);
   }

   return(false);
}

FLUSH_QUEUE(Flush_Queue)
{
   while(Queue->Completion_Target > Queue->Completion_Count)
   {
      Sdl3_Dequeue_Work(Queue);
   }

   Queue->Completion_Target = 0;
   Queue->Completion_Count = 0;
}

static int Sdl3_Thread_Procedure(void *Parameter)
{
   work_queue *Queue = (work_queue *)Parameter;
   while(1)
   {
      if(Sdl3_Dequeue_Work(Queue))
      {
         SDL_WaitSemaphore(Queue->Semaphore);
      }
   }
   return(0);
}

static struct {
   SDL_Window *Window;

   int Backbuffer_Width;
   int Backbuffer_Height;
   union
   {
      struct
      {
         SDL_Renderer *Renderer;
         SDL_Texture *Texture;
      };
      SDL_GLContext OpenGL_Context;
   };

   SDL_Gamepad *Gamepads[GAME_CONTROLLER_COUNT];

   Uint64 Frequency;
   Uint64 Frame_Start;
   Uint64 Frame_Count;

   float Monitor_Refresh_Rate;
   float Target_Frame_Seconds;
   float Actual_Frame_Seconds;

   SDL_AudioStream *Audio_Stream;
} Sdl3;

static void Sdl3_Process_Button(game_button *Button, bool Pressed)
{
   Button->Pressed = Pressed;
   Button->Transitioned = true;
}

static float Sdl3_Process_Stick(SDL_Gamepad *Gamepad, SDL_GamepadAxis Axis)
{
   float Result = 0.0f;

   Sint16 Axis_State = SDL_GetGamepadAxis(Gamepad, Axis);
   Sint16 Deadzone = 8000;

   if(Axis_State > Deadzone)
   {
      Result = (float)(Axis_State - Deadzone) / (float)(SDL_JOYSTICK_AXIS_MAX - Deadzone);
   }
   else if(Axis_State < -Deadzone)
   {
      Result = -1.0f * (float)(Axis_State + Deadzone) / (float)(SDL_JOYSTICK_AXIS_MIN + Deadzone);
   }

   return(Result);
}

static float Sdl3_Process_Trigger(SDL_Gamepad *Gamepad, SDL_GamepadAxis Axis)
{
   float Result = 0.0f;

   // TODO: SDL's documentation doesn't indicate a preferred deadzone for
   // triggers. The XBox Elite controller used for testing always reports an
   // Axis_State of 1 while released. Test with more controllers.
   Sint16 Deadzone = 50;

   Sint16 Trigger_State  = SDL_GetGamepadAxis(Gamepad, Axis);
   if(Trigger_State > Deadzone)
   {
      Result = (float)(Trigger_State - Deadzone) / (float)(SDL_JOYSTICK_AXIS_MAX - Deadzone);
   }

   return(Result);
}

static int Sdl3_Get_Gamepad_Index(SDL_JoystickID ID)
{
   int Result = 0;
   for(int Gamepad_Index = 1; Gamepad_Index < GAME_CONTROLLER_COUNT; ++Gamepad_Index)
   {
      SDL_Gamepad *Gamepad = Sdl3.Gamepads[Gamepad_Index];
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

static void Sdl3_Initialize_Software_Renderer(int Window_Width, int Window_Height)
{
   if(!SDL_CreateWindowAndRenderer("SDL Platform Build", Window_Width, Window_Height, 0, &Sdl3.Window, &Sdl3.Renderer))
   {
      SDL_Log("Failed to create window/renderer: %s", SDL_GetError());
      SDL_assert(0);
   }

   if(!SDL_SetRenderVSync(Sdl3.Renderer, 1))
   {
      SDL_Log("Failed to set vsync: %s", SDL_GetError());
   }

   if(!SDL_GetWindowSizeInPixels(Sdl3.Window, &Sdl3.Backbuffer_Width, &Sdl3.Backbuffer_Height))
   {
      SDL_Log("Failed to get window size: %s", SDL_GetError());
      Sdl3.Backbuffer_Width = Window_Width;
      Sdl3.Backbuffer_Height = Window_Height;
   }

   Sdl3.Texture = SDL_CreateTexture(Sdl3.Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, Sdl3.Backbuffer_Width, Sdl3.Backbuffer_Height);
   if(!Sdl3.Texture)
   {
      SDL_Log("Failed to create SDL texture: %s", SDL_GetError());
      SDL_assert(0);
   }
}

static void Sdl3_Initialize_OpenGL(int Window_Width, int Window_Height)
{
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

   Sdl3.Window = SDL_CreateWindow("SDL Platform Build (OpenGL)", Window_Width, Window_Height, SDL_WINDOW_OPENGL);
   if(!Sdl3.Window)
   {
      SDL_Log("Failed to create OpenGL window: %s", SDL_GetError());
      SDL_assert(0);
   }

   if(!SDL_GetWindowSizeInPixels(Sdl3.Window, &Sdl3.Backbuffer_Width, &Sdl3.Backbuffer_Height))
   {
      SDL_Log("Failed to get window size: %s", SDL_GetError());
      Sdl3.Backbuffer_Width = Window_Width;
      Sdl3.Backbuffer_Height = Window_Height;
   }

   SDL_GLContext Context = SDL_GL_CreateContext(Sdl3.Window);
   if(!Context)
   {
      SDL_Log("Failed to create OpenGL context: %s", SDL_GetError());
      SDL_assert(0);
   }

   SDL_GL_SetSwapInterval(1);
}

static void Sdl3_Display_With_Software_Renderer(renderer *Renderer, SDL_FRect Dst_Rect)
{
   SDL_SetRenderDrawColor(Sdl3.Renderer, 0, 0, 0, 255);
   SDL_RenderClear(Sdl3.Renderer);

   void *Backbuffer_Memory = Renderer->Backbuffer.Memory;
   size Backbuffer_Size = Renderer->Backbuffer.Width * sizeof(*Renderer->Backbuffer.Memory);
   SDL_UpdateTexture(Sdl3.Texture, 0, Backbuffer_Memory, Backbuffer_Size);
   SDL_RenderTexture(Sdl3.Renderer, Sdl3.Texture, 0, &Dst_Rect);
   SDL_RenderPresent(Sdl3.Renderer);
}

static void Sdl3_Display_With_OpenGL(void)
{
   glViewport(0, 0, Sdl3.Backbuffer_Width, Sdl3.Backbuffer_Height);
   SDL_GL_SwapWindow(Sdl3.Window);
}

int main(void)
{
   // Initialize SDL.
   int Window_Width = 640;
   int Window_Height = 480;

   if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMEPAD))
   {
      SDL_Log("Failed to initialize SDL3: %s", SDL_GetError());
      SDL_assert(0);
   }

#if USING_OPENGL
   Sdl3_Initialize_OpenGL(Window_Width, Window_Height);
#else
   Sdl3_Initialize_Software_Renderer(Window_Width, Window_Height);
#endif

   Sdl3.Frequency = SDL_GetPerformanceFrequency();

   int Display_ID = SDL_GetDisplayForWindow(Sdl3.Window);
   const SDL_DisplayMode *Display_Mode = SDL_GetCurrentDisplayMode(Display_ID);
   Sdl3.Monitor_Refresh_Rate = (Display_Mode && Display_Mode->refresh_rate > 0)
      ? Display_Mode->refresh_rate
      : 60.0f;
   Sdl3.Target_Frame_Seconds = 1.0f / Sdl3.Monitor_Refresh_Rate;

   SDL_Log("Monitor refresh rate: %02f", Sdl3.Monitor_Refresh_Rate);
   SDL_Log("Target frame time: %0.03fms", Sdl3.Target_Frame_Seconds * 1000.0f);

   SDL_AudioSpec Audio_Spec = {0};
   Audio_Spec.format = SDL_AUDIO_S16;
   Audio_Spec.channels = AUDIO_CHANNEL_COUNT;
   Audio_Spec.freq = AUDIO_FREQUENCY;

   Sdl3.Audio_Stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &Audio_Spec, 0, 0);
   if(!Sdl3.Audio_Stream)
   {
      SDL_Log("Failed to open audio stream: %s", SDL_GetError());
      SDL_assert(0);
   }
   SDL_ResumeAudioStreamDevice(Sdl3.Audio_Stream);

   // Initialize game.
   game_memory Memory = {0};
   Memory.Size = Megabytes(256);
   Memory.Base = SDL_calloc(1, Memory.Size);
   SDL_assert(Memory.Base);

   renderer Renderer = {0};
   Renderer.Backbuffer.Width = Sdl3.Backbuffer_Width;
   Renderer.Backbuffer.Height = Sdl3.Backbuffer_Height;

   int Input_Index = 0;
   game_input Inputs[16] = {0};

   game_audio_output Audio_Output = {0};

   work_queue Work_Queue = {0};
   Work_Queue.Semaphore = SDL_CreateSemaphore(0);

   int Core_Count = SDL_GetNumLogicalCPUCores();
   for(int Thread_Index = 1; Thread_Index < Core_Count; ++Thread_Index)
   {
      SDL_Thread *Thread = SDL_CreateThread(Sdl3_Thread_Procedure, 0, &Work_Queue);
      if(Thread)
      {
         SDL_DetachThread(Thread);
      }
      else
      {
         SDL_Log("Failed to create worker thread: %s.", SDL_GetError());
      }
   }

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
               SDL_KeyboardEvent Key_Event = Event.key;

               game_controller *Keyboard = Input->Controllers + 0;
               Keyboard->Connected = true;

               if(!Key_Event.repeat)
               {
                  switch(Key_Event.key)
                  {
                     case SDLK_ESCAPE: {
                        Running = false;
                     } break;

                     case SDLK_F:
                     case SDLK_RETURN: {
                        if(Key_Event.down)
                        {
                           if(Key_Event.key == SDLK_F || (Key_Event.mod & SDL_KMOD_ALT))
                           {
                              bool Is_Fullscreen = (SDL_GetWindowFlags(Sdl3.Window) & SDL_WINDOW_FULLSCREEN);
                              SDL_SetWindowFullscreen(Sdl3.Window, Is_Fullscreen ? 0 : SDL_WINDOW_FULLSCREEN);
                           }
                        }
                     } break;

                     case SDLK_I:                  { Sdl3_Process_Button(&Keyboard->Action_Up, Key_Event.down); } break;
                     case SDLK_K:                  { Sdl3_Process_Button(&Keyboard->Action_Down, Key_Event.down); } break;
                     case SDLK_J:                  { Sdl3_Process_Button(&Keyboard->Action_Left, Key_Event.down); } break;
                     case SDLK_L:                  { Sdl3_Process_Button(&Keyboard->Action_Right, Key_Event.down); } break;
                     case SDLK_W: case SDLK_UP:    { Sdl3_Process_Button(&Keyboard->Move_Up, Key_Event.down); } break;
                     case SDLK_S: case SDLK_DOWN:  { Sdl3_Process_Button(&Keyboard->Move_Down, Key_Event.down); } break;
                     case SDLK_A: case SDLK_LEFT:  { Sdl3_Process_Button(&Keyboard->Move_Left, Key_Event.down); } break;
                     case SDLK_D: case SDLK_RIGHT: { Sdl3_Process_Button(&Keyboard->Move_Right, Key_Event.down); } break;
                     case SDLK_Q:                  { Sdl3_Process_Button(&Keyboard->Shoulder_Left, Key_Event.down); } break;
                     case SDLK_E:                  { Sdl3_Process_Button(&Keyboard->Shoulder_Right, Key_Event.down); } break;
                     case SDLK_SPACE:              { Sdl3_Process_Button(&Keyboard->Start, Key_Event.down); } break;
                     case SDLK_BACKSPACE:          { Sdl3_Process_Button(&Keyboard->Back, Key_Event.down); } break;
                  }
               }
            } break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
               SDL_MouseButtonEvent Button_Event = Event.button;
               switch(Button_Event.button)
               {
                  case SDL_BUTTON_LEFT:   { Sdl3_Process_Button(&Input->Mouse_Button_Left, Button_Event.down); } break;
                  case SDL_BUTTON_MIDDLE: { Sdl3_Process_Button(&Input->Mouse_Button_Middle, Button_Event.down); } break;
                  case SDL_BUTTON_RIGHT:  { Sdl3_Process_Button(&Input->Mouse_Button_Right, Button_Event.down); } break;
               }
            } break;

            case SDL_EVENT_GAMEPAD_BUTTON_UP:
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
               SDL_GamepadButtonEvent Button_Event = Event.gbutton;

               int Gamepad_Index = Sdl3_Get_Gamepad_Index(Button_Event.which);
               SDL_assert(Gamepad_Index > 0);
               SDL_assert(Gamepad_Index < GAME_CONTROLLER_COUNT);

               game_controller *Controller = Input->Controllers + Gamepad_Index;
               switch(Button_Event.button)
               {
                  // TODO: Confirm if other controllers map buttons on based name or position.
                  case SDL_GAMEPAD_BUTTON_SOUTH:          { Sdl3_Process_Button(&Controller->Action_Down, Button_Event.down); } break;
                  case SDL_GAMEPAD_BUTTON_EAST:           { Sdl3_Process_Button(&Controller->Action_Right, Button_Event.down); } break;
                  case SDL_GAMEPAD_BUTTON_WEST:           { Sdl3_Process_Button(&Controller->Action_Left, Button_Event.down); } break;
                  case SDL_GAMEPAD_BUTTON_NORTH:          { Sdl3_Process_Button(&Controller->Action_Up, Button_Event.down); } break;
                  case SDL_GAMEPAD_BUTTON_DPAD_UP:        { Sdl3_Process_Button(&Controller->Move_Up, Button_Event.down); } break;
                  case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      { Sdl3_Process_Button(&Controller->Move_Down, Button_Event.down); } break;
                  case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      { Sdl3_Process_Button(&Controller->Move_Left, Button_Event.down); } break;
                  case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     { Sdl3_Process_Button(&Controller->Move_Right, Button_Event.down); } break;
                  case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  { Sdl3_Process_Button(&Controller->Shoulder_Left, Button_Event.down); } break;
                  case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: { Sdl3_Process_Button(&Controller->Shoulder_Right, Button_Event.down); } break;
                  case SDL_GAMEPAD_BUTTON_START:          { Sdl3_Process_Button(&Controller->Start, Button_Event.down); } break;
                  case SDL_GAMEPAD_BUTTON_BACK:           { Sdl3_Process_Button(&Controller->Back, Button_Event.down); } break;
               }
            } break;

            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED: {
               SDL_GamepadDeviceEvent Gamepad_Event = Event.gdevice;

               SDL_JoystickID ID = Gamepad_Event.which;
               if(SDL_IsGamepad(ID))
               {
                  if(Gamepad_Event.type == SDL_EVENT_GAMEPAD_ADDED)
                  {
                     // TODO: SDL_EVENT_GAMEPAD_ADDED can fire multiple times
                     // for a single controller (e.g. when plugged in after
                     // initialization), so we need to check if ID corresponds
                     // to a connected gamepad. Determine if we'd be better off
                     // checking our own list, or if SDL_GetGamepadFromID is
                     // fast enough.
                     bool Is_Unconnected_Gamepad = (SDL_GetGamepadFromID(ID) == 0);
                     if(Is_Unconnected_Gamepad)
                     {
                        for(int Gamepad_Index = 1; Gamepad_Index < GAME_CONTROLLER_COUNT; ++Gamepad_Index)
                        {
                           if(!Sdl3.Gamepads[Gamepad_Index])
                           {
                              Sdl3.Gamepads[Gamepad_Index] = SDL_OpenGamepad(ID);
                              if(Sdl3.Gamepads[Gamepad_Index])
                              {
                                 Input->Controllers[Gamepad_Index].Connected = true;
                                 SDL_Log("Gamepad added to slot %d.", Gamepad_Index);
                              }
                              else
                              {
                                 SDL_Log("Failed to add gamepad: %s.", SDL_GetError());
                              }
                              break;
                           }
                        }
                     }
                  }
                  else
                  {
                     SDL_assert(Gamepad_Event.type == SDL_EVENT_GAMEPAD_REMOVED);

                     int Gamepad_Index = Sdl3_Get_Gamepad_Index(ID);
                     SDL_assert(Gamepad_Index > 0);
                     SDL_assert(Gamepad_Index < GAME_CONTROLLER_COUNT);
                     SDL_assert(Sdl3.Gamepads[Gamepad_Index]);

                     SDL_CloseGamepad(Sdl3.Gamepads[Gamepad_Index]);

                     Sdl3.Gamepads[Gamepad_Index] = 0;
                     Input->Controllers[Gamepad_Index].Connected = false;

                     SDL_Log("Gamepad removed from slot %d.", Gamepad_Index);
                  }
               }
               else
               {
                  SDL_Log("This joystick is not supported by SDL's gamepad interface.");
               }
            } break;
         }
      }

      for(int Gamepad_Index = 1; Gamepad_Index < Array_Count(Input->Controllers); ++Gamepad_Index)
      {
         game_controller *Controller = Input->Controllers + Gamepad_Index;
         if(Controller->Connected)
         {
            SDL_Gamepad *Gamepad = Sdl3.Gamepads[Gamepad_Index];

            Controller->Stick_Left_X  = Sdl3_Process_Stick(Gamepad, SDL_GAMEPAD_AXIS_LEFTX);
            Controller->Stick_Left_Y  = Sdl3_Process_Stick(Gamepad, SDL_GAMEPAD_AXIS_LEFTY);
            Controller->Stick_Right_X = Sdl3_Process_Stick(Gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
            Controller->Stick_Right_Y = Sdl3_Process_Stick(Gamepad, SDL_GAMEPAD_AXIS_RIGHTY);

            Controller->Trigger_Left  = Sdl3_Process_Trigger(Gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
            Controller->Trigger_Right = Sdl3_Process_Trigger(Gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
         }
      }

      int Dst_Width, Dst_Height;
      SDL_GetCurrentRenderOutputSize(Sdl3.Renderer, &Dst_Width, &Dst_Height);

      float Src_Aspect = (float)Sdl3.Backbuffer_Width / (float)Sdl3.Backbuffer_Height;
      float Dst_Aspect = (float)Dst_Width / (float)Dst_Height;
      SDL_FRect Dst_Rect = {0, 0, (float)Dst_Width, (float)Dst_Height};
      if(Src_Aspect > Dst_Aspect)
      {
         // NOTE: Bars on top and bottom.
         int Bar_Height = (int)(0.5f * (Dst_Height - (Dst_Width / Src_Aspect)));
         Dst_Rect.y += Bar_Height;
         Dst_Rect.h -= (Bar_Height * 2);
      }
      else if(Src_Aspect < Dst_Aspect)
      {
         // NOTE: Bars on left and right;
         int Bar_Width = (int)(0.5f * (Dst_Width - (Dst_Height * Src_Aspect)));
         Dst_Rect.x += Bar_Width;
         Dst_Rect.w -= (Bar_Width * 2);
      }

      float Raw_Mouse_X, Raw_Mouse_Y;
      SDL_GetMouseState(&Raw_Mouse_X, &Raw_Mouse_Y);
      Input->Normalized_Mouse_X = (2.0f * (Raw_Mouse_X - Dst_Rect.x) / Dst_Rect.w) - 1.0f;
      Input->Normalized_Mouse_Y = (2.0f * (Raw_Mouse_Y - Dst_Rect.y) / Dst_Rect.h) - 1.0f;

      // Update game state.
      Update(Memory, Input, &Renderer, &Work_Queue, Sdl3.Actual_Frame_Seconds);

      // Fill audio.
      size Bytes_Per_Sample = AUDIO_CHANNEL_COUNT * sizeof(*Audio_Output.Samples);
      size Max_Audio_Output_Size = 2048 * Bytes_Per_Sample;

      int Bytes_Queued = SDL_GetAudioStreamQueued(Sdl3.Audio_Stream);
      if(Bytes_Queued < 0)
      {
         SDL_Log("Failed to query audio queue size: %s", SDL_GetError());
      }
      else if(Bytes_Queued < Max_Audio_Output_Size)
      {
         Audio_Output.Sample_Count = Max_Audio_Output_Size / Bytes_Per_Sample;
         Mix_Audio_Output(Memory, &Audio_Output);

         if(!SDL_PutAudioStreamData(Sdl3.Audio_Stream, Audio_Output.Samples, Max_Audio_Output_Size))
         {
            SDL_Log("Failed to fill audio stream: %s", SDL_GetError());
         }
      }

      // Render frame.
#if USING_OPENGL
      Sdl3_Display_With_OpenGL();
#else
      Sdl3_Display_With_Software_Renderer(&Renderer, Dst_Rect);
#endif

      // End of frame.
      Input_Index++;
      if(Input_Index == Array_Count(Inputs)) Input_Index = 0;
      End_Frame_Input(Input, Inputs + Input_Index);

      Uint64 Delta = SDL_GetPerformanceCounter() - Sdl3.Frame_Start;
      float Actual_Frame_Seconds = (float)Delta / (float)Sdl3.Frequency;

      int Sleep_ms = 0;
      if(Actual_Frame_Seconds < Sdl3.Target_Frame_Seconds)
      {
         Sleep_ms = (int)((Sdl3.Target_Frame_Seconds - Actual_Frame_Seconds) * 1000.0f) - 1;
         if(Sleep_ms > 0)
         {
            SDL_Delay(Sleep_ms);
         }
      }
      while(Actual_Frame_Seconds < Sdl3.Target_Frame_Seconds)
      {
         Uint64 Delta = SDL_GetPerformanceCounter() - Sdl3.Frame_Start;
         Actual_Frame_Seconds = (float)Delta / (float)Sdl3.Frequency;
      }

      Sdl3.Frame_Start = SDL_GetPerformanceCounter();
      Sdl3.Actual_Frame_Seconds = Actual_Frame_Seconds;
      Sdl3.Frame_Count++;

#     if DEBUG && 0
      int FPS = (int)(Sdl3.Monitor_Refresh_Rate + 0.5f);
      if((Sdl3.Frame_Count % FPS) == 0)
      {
         float Frame_ms = Sdl3.Actual_Frame_Seconds * 1000.0f;
         SDL_Log("Frame time: % .3fms (slept %dms)", Frame_ms, Sleep_ms);
      }
#     endif
   }

   return(0);
}
