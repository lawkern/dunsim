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

static void Push_Clear(renderer *Renderer, u32 Color)
{
   render_command *Command = Push_Command(Renderer, Render_Layer_Background, Render_Command_Clear);
   if(Command)
   {
      Command->Color = Color;
   }
}

static void Push_Rectangle(renderer *Renderer, render_layer Layer, float X, float Y, float Width, float Height, u32 Color)
{
   render_command *Command = Push_Command(Renderer, Layer, Render_Command_Rectangle);
   if(Command)
   {
      Command->X = X;
      Command->Y = Y;
      Command->Width = Width;
      Command->Height = Height;
      Command->Color = Color;
   }
}

static void Push_Outline(renderer *Renderer, render_layer Layer, float X, float Y, float Width, float Height, float Weight, u32 Color)
{
   Push_Rectangle(Renderer, Layer, X, Y, Width-Weight, Weight, Color); // Top
   Push_Rectangle(Renderer, Layer, X+Weight, Y+Height-Weight, Width-Weight, Weight, Color); // Bottom
   Push_Rectangle(Renderer, Layer, X, Y+Weight, Weight, Height-Weight, Color); // Left
   Push_Rectangle(Renderer, Layer, X+Width-Weight, Y, Weight, Height-Weight, Color); // Right
}

static void Push_Texture(renderer *Renderer, render_layer Layer, texture Texture, float X, float Y)
{
   render_command *Command = Push_Command(Renderer, Layer, Render_Command_Texture);
   if(Command)
   {
      Command->X = X;
      Command->Y = Y;
      Command->Texture = Texture;
   }
}

static void Push_Text(renderer *Renderer, text_font *Font, text_size Size, float X, float Y, string Text)
{
   if(Font->Loaded)
   {
      text_glyphs *Glyphs = Font->Glyphs + Size;
      float Scale = Glyphs->Scale;

      for(size Index = 0; Index < Text.Length; ++Index)
      {
         int Codepoint = Text.Data[Index];

         texture Glyph = Glyphs->Bitmaps[Codepoint];
         Push_Texture(Renderer, Render_Layer_UI, Glyph, X, Y);

         if(Index != Text.Length-1)
         {
            int Next_Codepoint = Text.Data[Index + 1];
            int Pair_Index = (Codepoint * GLYPH_COUNT) + Next_Codepoint;
            X += (Scale * Font->Distances[Pair_Index]);
         }
      }
   }
}

static void Render(renderer *Renderer)
{
   texture Backbuffer = Renderer->Backbuffer;
   for(int Queue_Index = 0; Queue_Index < Array_Count(Renderer->Queues); ++Queue_Index)
   {
      render_queue *Queue = Renderer->Queues[Queue_Index];
      for(int Command_Index = 0; Command_Index < Queue->Command_Count; ++Command_Index)
      {
         render_command *Command = Queue->Commands + Command_Index;
         switch(Command->Type)
         {
            case Render_Command_Clear: {
               Draw_Clear(Backbuffer, Command->Color);
            } break;

            case Render_Command_Rectangle: {
               Draw_Rectangle(Backbuffer, Command->X, Command->Y, Command->Width, Command->Height, Command->Color);
            } break;

            case Render_Command_Texture: {
               Draw_Texture(Backbuffer, Command->Texture, Command->X, Command->Y);
            } break;
         }
      }
      Queue->Command_Count = 0;
   }
}
