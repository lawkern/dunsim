/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

typedef struct {
   int Width;
   int Height;
   u32 *Memory;

   float Offset_X;
   float Offset_Y;
} texture;

typedef enum {
   Render_Command_Clear,
   Render_Command_Rectangle,
   Render_Command_Texture,
} render_command_type;

typedef struct {
   render_command_type Type;
   float X;
   float Y;
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
   float Pixels_Per_Meter;

   render_queue *Queues[Render_Layer_Count];
} renderer;

// Renderer API:
#define DRAW_CLEAR(Name) void Name(texture Destination, u32 Color)
static DRAW_CLEAR(Draw_Clear);

#define DRAW_RECTANGLE(Name) void Name(texture Destination, float X, float Y, float Width, float Height, u32 Color)
static DRAW_RECTANGLE(Draw_Rectangle);

#define DRAW_TEXTURE(Name) void Name(texture Destination, texture Source, float X, float Y)
static DRAW_TEXTURE(Draw_Texture);
