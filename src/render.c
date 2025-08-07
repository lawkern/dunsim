/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

// NOTE: This file implements the API used by game code to issue render commands
// to the renderer. The actual draw calls are implemented in renderer_*.c files,
// which correspond to different graphics APIs.

static render_command *Push_Command(renderer *Renderer, render_layer Layer, render_command_type Type)
{
   render_command *Command = 0;

   render_queue *Queue = Renderer->Queues[Layer];
   if(Queue->Command_Count < Array_Count(Queue->Commands))
   {
      Command = Queue->Commands + Queue->Command_Count++;
      Command->Type = Type;
   }
   else
   {
      Log("Ran out of render commands in layer %u.", Layer);
   }

   return(Command);
}

static void Push_Clear(renderer *Renderer, vec4 Color)
{
   render_command *Command = Push_Command(Renderer, Render_Layer_Background, Render_Command_Clear);
   if(Command)
   {
      Command->Color = Color;
   }
}

static void Push_Rectangle(renderer *Renderer, render_layer Layer, float X, float Y, float Width, float Height, vec4 Color)
{
   render_command *Command = Push_Command(Renderer, Layer, Render_Command_Rectangle);
   if(Command)
   {
      float Pixels_Per_Meter = Renderer->Pixels_Per_Meter;
      float Screen_Center_X = Renderer->Backbuffer.Width * 0.5f;
      float Screen_Center_Y = Renderer->Backbuffer.Height * 0.5f;

      Command->X = (X * Pixels_Per_Meter) + Screen_Center_X;
      Command->Y = (Y * Pixels_Per_Meter) + Screen_Center_Y;
      Command->Width = Width * Pixels_Per_Meter;
      Command->Height = Height * Pixels_Per_Meter;
      Command->Color = Color;
   }
}

static void Push_Outline(renderer *Renderer, render_layer Layer, float X, float Y, float Width, float Height, float Weight, vec4 Color)
{
   Push_Rectangle(Renderer, Layer, X, Y, Width-Weight, Weight, Color); // Top
   Push_Rectangle(Renderer, Layer, X+Weight, Y+Height-Weight, Width-Weight, Weight, Color); // Bottom
   Push_Rectangle(Renderer, Layer, X, Y+Weight, Weight, Height-Weight, Color); // Left
   Push_Rectangle(Renderer, Layer, X+Width-Weight, Y, Weight, Height-Weight, Color); // Right
}

static void Push_Texture(renderer *Renderer, render_layer Layer, texture Texture, float X, float Y, float Width, float Height)
{
   render_command *Command = Push_Command(Renderer, Layer, Render_Command_Texture);
   if(Command)
   {
      float Pixels_Per_Meter = Renderer->Pixels_Per_Meter;
      float Screen_Center_X = Renderer->Backbuffer.Width * 0.5f;
      float Screen_Center_Y = Renderer->Backbuffer.Height * 0.5f;

      Command->Texture = Texture;
      Command->X = (X * Pixels_Per_Meter) + Screen_Center_X;
      Command->Y = (Y * Pixels_Per_Meter) + Screen_Center_Y;
      Command->Width = Width * Pixels_Per_Meter;
      Command->Height = Height * Pixels_Per_Meter;
   }
}

static void Push_Text(renderer *Renderer, text_font *Font, text_size Size, float X, float Y, string Text)
{
   if(Font->Loaded)
   {
      float Pixels_Per_Meter = Renderer->Pixels_Per_Meter;
      float Screen_Center_X = Renderer->Backbuffer.Width * 0.5f;
      float Screen_Center_Y = Renderer->Backbuffer.Height * 0.5f;

      text_glyphs *Glyphs = Font->Glyphs + Size;
      float Scale = Glyphs->Pixel_Scale * 1.0f/Pixels_Per_Meter;

      for(size Index = 0; Index < Text.Length; ++Index)
      {
         int Codepoint = Text.Data[Index];
         texture Glyph = Glyphs->Bitmaps[Codepoint];

         render_command *Command = Push_Command(Renderer, Render_Layer_UI, Render_Command_Texture);
         if(Command)
         {
            Command->Texture = Glyph;
            Command->X = (X * Pixels_Per_Meter) + Screen_Center_X;
            Command->Y = (Y * Pixels_Per_Meter) + Screen_Center_Y;
            Command->Width = Glyph.Width;
            Command->Height = Glyph.Height;
         }

         if(Index != Text.Length-1)
         {
            int Next_Codepoint = Text.Data[Index + 1];
            int Pair_Index = (Codepoint * GLYPH_COUNT) + Next_Codepoint;
            X += (Scale * Font->Distances[Pair_Index]);
         }
      }
   }
}

static void Push_Textured_Quad(renderer *Renderer, render_layer Layer, texture Texture, vec2 Origin, vec2 X_Axis, vec2 Y_Axis)
{
   render_command *Command = Push_Command(Renderer, Layer, Render_Command_Textured_Quad);
   if(Command)
   {
      float Pixels_Per_Meter = Renderer->Pixels_Per_Meter;
      vec2 Screen_Dim = {Renderer->Backbuffer.Width, Renderer->Backbuffer.Height};
      vec2 Screen_Center = Mul2(Screen_Dim, 0.5f);

      Command->Texture = Texture;
      Command->Origin = Add2(Mul2(Origin, Pixels_Per_Meter), Screen_Center);
      Command->X_Axis = Mul2(X_Axis, Pixels_Per_Meter);
      Command->Y_Axis = Mul2(Y_Axis, Pixels_Per_Meter);
   }
}

static void Push_Debug_Basis(renderer *Renderer, texture Texture, vec2 Origin, vec2 X_Axis, vec2 Y_Axis)
{
   render_command *Command = Push_Command(Renderer, Render_Layer_UI, Render_Command_Debug_Basis);
   if(Command)
   {
      float Pixels_Per_Meter = Renderer->Pixels_Per_Meter;
      vec2 Screen_Dim = {Renderer->Backbuffer.Width, Renderer->Backbuffer.Height};
      vec2 Screen_Center = Mul2(Screen_Dim, 0.5f);

      Command->Texture = Texture;
      Command->Origin = Add2(Mul2(Origin, Renderer->Pixels_Per_Meter), Screen_Center);
      Command->X_Axis = Mul2(X_Axis, Pixels_Per_Meter);
      Command->Y_Axis = Mul2(Y_Axis, Pixels_Per_Meter);
   }
}
