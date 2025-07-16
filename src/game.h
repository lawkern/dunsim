/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#include <stddef.h>
typedef ptrdiff_t size;

typedef struct {
   size Length;
   u8 *Data;
} string;

#define S(Literal) (string){sizeof(Literal)-1, (u8 *)(Literal)}

typedef struct {
   u8 *Begin;
   u8 *End;
} arena;

#define Assert(Cond) do { if(!(Cond)) { __builtin_trap(); } } while(0)
#define Array_Count(Array) (size)(sizeof(Array) / sizeof((Array)[0]))

#define Minimum(A, B) ((A) < (B) ? (A) : (B))
#define Maximum(A, B) ((A) > (B) ? (A) : (B))

#define Kilobytes(N) (1024 * (N))
#define Megabytes(N) (1024 * Kilobytes(N))
#define Gigabytes(N) (1024 * Megabytes(N))

static void *Zero_Size(void *Result, size Size)
{
   u8 *Bytes = (u8 *)Result;
   while(Size--)
   {
      *Bytes++ = 0;
   }
   return(Result);
}

#define Allocate(Arena, type, Count) (type *)Allocate_Size((Arena), (Count)*sizeof(type))

static void *Allocate_Size(arena *Arena, size Size)
{
   Assert(Arena->Begin < (Arena->End - Size));

   void *Result = Zero_Size(Arena->Begin, Size);
   Arena->Begin += Size;

   return(Result);
}

typedef struct {
   size Size;
   u8 *Base;
} game_memory;

typedef struct {
   int Width;
   int Height;
   u32 *Memory;

   int Offset_X;
   int Offset_Y;
} game_texture;

typedef struct {
   bool Pressed;
   bool Transitioned;
} game_button;

#define GAME_BUTTONS                            \
   X(Action_Up)                                 \
      X(Action_Down)				\
      X(Action_Left)				\
      X(Action_Right)				\
      X(Move_Up)				\
      X(Move_Down)				\
      X(Move_Left)				\
      X(Move_Right)				\
      X(Shoulder_Left)				\
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
} game_input;

static inline bool Is_Held(game_button Button)
{
   // NOTE(law): The specified button is currently pressed.
   bool Result = (Button.Pressed);
   return(Result);
}

static inline bool Was_Pressed(game_button Button)
{
   // NOTE(law): The specified button was pressed on this frame.
   bool Result = (Button.Pressed && Button.Transitioned);
   return(Result);
}

static inline bool Was_Released(game_button Button)
{
   // NOTE(law): The specified button was released on this frame.
   bool Result = (!Button.Pressed && Button.Transitioned);
   return(Result);
}

// Game API:
#define UPDATE(Name) void Name(game_memory Memory, game_texture Backbuffer, game_input *Input, float Frame_Seconds)
UPDATE(Update);

// Platform API:
#define LOG(Name) void Name(char *Format, ...)
LOG(Log);

#define READ_ENTIRE_FILE(Name) string Name(arena *Arena, char *Path)
READ_ENTIRE_FILE(Read_Entire_File);
