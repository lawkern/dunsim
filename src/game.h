/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

typedef struct {
   size Size;
   u8 *Base;
} game_memory;

typedef struct {
   bool Pressed;
   bool Transitioned;
} game_button;

#define GAME_BUTTONS                            \
   X(Action_Up)                                 \
   X(Action_Down)                               \
   X(Action_Left)                               \
   X(Action_Right)                              \
   X(Move_Up)                                   \
   X(Move_Down)                                 \
   X(Move_Left)                                 \
   X(Move_Right)                                \
   X(Shoulder_Left)                             \
   X(Shoulder_Right)                            \
   X(Start)                                     \
   X(Back)

typedef enum {
#  define X(button_name) GAME_BUTTON_##button_name,
   GAME_BUTTONS
#  undef X

   GAME_BUTTON_COUNT,
} game_button_kind;

typedef struct {
   union
   {
      struct
      {
#        define X(Button_Name) game_button Button_Name;
         GAME_BUTTONS
#        undef X
      };
      game_button Buttons[GAME_BUTTON_COUNT];
   };

   // Range: -1.0f to 1.0f
   float Stick_Left_X;
   float Stick_Left_Y;
   float Stick_Right_X;
   float Stick_Right_Y;

   // Range: 0.0f to 1.0f
   float Trigger_Left;
   float Trigger_Right;

   bool Connected;
} game_controller;

#define GAME_CONTROLLER_COUNT (5) // 1 Keyboard + 4 Gamepads
typedef struct {
   float Frame_Seconds;
   game_controller Controllers[GAME_CONTROLLER_COUNT];

   float Mouse_X;
   float Mouse_Y;
   game_button Mouse_Button_Left;
   game_button Mouse_Button_Middle;
   game_button Mouse_Button_Right;
} game_input;

static inline bool Is_Held(game_button Button)
{
   // NOTE: The specified button is currently pressed.
   bool Result = (Button.Pressed);
   return(Result);
}

static inline bool Was_Pressed(game_button Button)
{
   // NOTE: The specified button was pressed on this frame.
   bool Result = (Button.Pressed && Button.Transitioned);
   return(Result);
}

static inline bool Was_Released(game_button Button)
{
   // NOTE: The specified button was released on this frame.
   bool Result = (!Button.Pressed && Button.Transitioned);
   return(Result);
}

static inline void End_Frame_Input(game_input *Previous, game_input *Next)
{
   // NOTE: Copy necessary input state to the game_input structure that will be
   // used on the next frame. Button down state is carried over, while
   // transition state is reset.

   *Next = *Previous;
   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      game_controller *Next_Controller = Next->Controllers + Controller_Index;
      for(int Button_Index = 0; Button_Index < GAME_BUTTON_COUNT; ++Button_Index)
      {
         Next_Controller->Buttons[Button_Index].Transitioned = false;
      }
   }

   Next->Mouse_Button_Left.Transitioned = false;
   Next->Mouse_Button_Middle.Transitioned = false;
   Next->Mouse_Button_Right.Transitioned = false;
}

typedef struct {
   int Sample_Count;
   s16 *Samples;
} game_audio_output;

#define WORK_QUEUE_TASK(Name) void Name(void *Data)
typedef WORK_QUEUE_TASK(work_queue_task);

typedef struct {
   void *Data;
   work_queue_task *Task;
} work_queue_entry;

typedef struct {
   volatile u32 Read_Index;
   volatile u32 Write_Index;

   volatile u32 Completion_Target;
   volatile u32 Completion_Count;

   void *Semaphore;
   work_queue_entry Entries[512];
} work_queue;

// Game API:
#define UPDATE(Name) void Name(game_memory Memory, game_input *Input, renderer *Renderer, work_queue *Work_Queue, float Frame_Seconds)
UPDATE(Update);

#define MIX_SOUND(Name) void Name(game_memory Memory, game_audio_output *Audio_Output)
MIX_SOUND(Mix_Sound);

// Platform API:
#define LOG(Name) void Name(char *Format, ...)
LOG(Log);

#define READ_ENTIRE_FILE(Name) string Name(arena *Arena, char *Path)
READ_ENTIRE_FILE(Read_Entire_File);

#define WRITE_ENTIRE_FILE(Name) bool Name(u8 *Memory, size Size, char *Path)
WRITE_ENTIRE_FILE(Write_Entire_File);

#define ENQUEUE_WORK(name) void name(work_queue *Queue, void *Data, work_queue_task *Task)
ENQUEUE_WORK(Enqueue_Work);

#define FLUSH_QUEUE(name) void Name(work_queue *Queue)
FLUSH_QUEUE(Flush_Queue);
