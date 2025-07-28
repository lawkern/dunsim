/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

typedef struct {
   int Width;
   int Height;
   u32 *Memory;

   int Offset_X;
   int Offset_Y;
} texture;

typedef enum {
   Render_Command_Clear,
   Render_Command_Rectangle,
   Render_Command_Texture,
} render_command_type;

typedef struct {
   render_command_type Type;
   int X;
   int Y;
   union
   {
      texture Texture;
      struct
      {
         int Width;
         int Height;
         u32 Color;
      };
   };
} render_command;

typedef struct {
   int Command_Count;
   render_command Commands[4096];
} render_queue;

typedef enum {
   Render_Layer_Background,
   Render_Layer_Foreground,
   Render_Layer_UI,

   Render_Layer_Count,
} render_layer;

typedef struct {
   texture Backbuffer;
   int Pixels_Per_Meter;

   render_queue *Queues[Render_Layer_Count];
} renderer;
