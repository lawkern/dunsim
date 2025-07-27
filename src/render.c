/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

// NOTE: Command Calls.

static render_command *Push_Command(renderer *Renderer, render_layer Layer, render_command_type Type)
{
   render_queue *Queue = Renderer->Queues[Layer];
   Assert(Queue->Command_Count < Array_Count(Queue->Commands));

   render_command *Command = Queue->Commands + Queue->Command_Count++;
   Command->Type = Type;

   return(Command);
}

static void Push_Clear(renderer *Renderer, render_layer Layer, u32 Color)
{
   render_command *Command = Push_Command(Renderer, Layer, Render_Command_Clear);
   Command->Color = Color;
}

static void Push_Rectangle(renderer *Renderer, render_layer Layer, int X, int Y, int Width, int Height, u32 Color)
{
   render_command *Command = Push_Command(Renderer, Layer, Render_Command_Rectangle);
   Command->X = X;
   Command->Y = Y;
   Command->Width = Width;
   Command->Height = Height;
   Command->Color = Color;
}

static void Push_Outline(renderer *Renderer, render_layer Layer, int X, int Y, int Width, int Height, int Weight, u32 Color)
{
   Push_Rectangle(Renderer, Layer, X, Y, Width-Weight, Weight, Color); // Top
   Push_Rectangle(Renderer, Layer, X+Weight, Y+Height-Weight, Width-Weight, Weight, Color); // Bottom
   Push_Rectangle(Renderer, Layer, X, Y+Weight, Weight, Height-Weight, Color); // Left
   Push_Rectangle(Renderer, Layer, X+Width-Weight, Y, Weight, Height-Weight, Color); // Right
}

static void Push_Texture(renderer *Renderer, render_layer Layer, texture Texture, int X, int Y)
{
   render_command *Command = Push_Command(Renderer, Layer, Render_Command_Texture);
   Command->X = X;
   Command->Y = Y;
   Command->Texture = Texture;
}

// NOTE: Draw Calls.

static void Clear(texture Destination, u32 Color)
{
   for(int Y = 0; Y < Destination.Height; ++Y)
   {
      for(int X = 0; X < Destination.Width; ++X)
      {
         Destination.Memory[(Destination.Width * Y) + X] = Color;
      }
   }
}

static void Draw_Rectangle(texture Destination, int X, int Y, int Width, int Height, u32 Color)
{
   int Min_X = Maximum(X, 0);
   int Min_Y = Maximum(Y, 0);
   int Max_X = Minimum(Destination.Width, X + Width);
   int Max_Y = Minimum(Destination.Height, Y + Height);

   for(int Y = Min_Y; Y < Max_Y; ++Y)
   {
      for(int X = Min_X; X < Max_X; ++X)
      {
         Destination.Memory[(Destination.Width * Y) + X] = Color;
      }
   }
}

static void Draw_Outline(texture Destination, int X, int Y, int Width, int Height, int Weight, u32 Color)
{
   Draw_Rectangle(Destination, X, Y, Width-Weight, Weight, Color); // Top
   Draw_Rectangle(Destination, X+Weight, Y+Height-Weight, Width-Weight, Weight, Color); // Bottom
   Draw_Rectangle(Destination, X, Y+Weight, Weight, Height-Weight, Color); // Left
   Draw_Rectangle(Destination, X+Width-Weight, Y, Weight, Height-Weight, Color); // Right
}

static void Draw_Texture(texture Destination, texture Source, float X, float Y)
{
   X += Source.Offset_X;
   Y += Source.Offset_Y;

   int Min_X = Maximum(X, 0);
   int Min_Y = Maximum(Y, 0);
   int Max_X = Minimum(Destination.Width, X + Source.Width);
   int Max_Y = Minimum(Destination.Height, Y + Source.Height);

   int Clip_X_Offset = Min_X - X;
   int Clip_Y_Offset = Min_Y - Y;

   u32 *Source_Row = Source.Memory + Clip_Y_Offset*Source.Width;
   for(int Destination_Y = Min_Y; Destination_Y < Max_Y; ++Destination_Y)
   {
      int X_Offset = Clip_X_Offset;
      u32 *Destination_Row = Destination.Memory + Destination.Width*Destination_Y;

      for(int Destination_X = Min_X; Destination_X < Max_X; ++Destination_X)
      {
         u32 Source_Pixel = Source_Row[X_Offset++];
         u32 *Destination_Pixel = Destination_Row + Destination_X;

         float SR = (float)((Source_Pixel >> 24) & 0xFF);
         float SG = (float)((Source_Pixel >> 16) & 0xFF);
         float SB = (float)((Source_Pixel >>  8) & 0xFF);
         float SA = (float)((Source_Pixel >>  0) & 0xFF) / 255.0f;

         float DR = (float)((*Destination_Pixel >> 24) & 0xFF);
         float DG = (float)((*Destination_Pixel >> 16) & 0xFF);
         float DB = (float)((*Destination_Pixel >>  8) & 0xFF);

         u32 R = (u32)((DR * (1.0f-SA) + SR) + 0.5f);
         u32 G = (u32)((DG * (1.0f-SA) + SG) + 0.5f);
         u32 B = (u32)((DB * (1.0f-SA) + SB) + 0.5f);

         *Destination_Pixel = (R<<24) | (G<<16) | (B<<8) | 0xFF;
      }

      Source_Row += Source.Width;
   }
}

static void Draw_Layer(texture Destination, texture Source)
{
   for(int Y = 0; Y < Destination.Height; ++Y)
   {
      u32 *Source_Row = Source.Memory + Source.Width*Y;
      u32 *Destination_Row = Destination.Memory + Destination.Width*Y;

      for(int X = 0; X < Destination.Width; ++X)
      {
         u32 Source_Pixel = Source_Row[X];
         u32 Destination_Pixel = Destination_Row[X];

         float SR = (float)((Source_Pixel >> 24) & 0xFF);
         float SG = (float)((Source_Pixel >> 16) & 0xFF);
         float SB = (float)((Source_Pixel >>  8) & 0xFF);
         float SA = (float)((Source_Pixel >>  0) & 0xFF) / 255.0f;

         float DR = (float)((Destination_Pixel >> 24) & 0xFF);
         float DG = (float)((Destination_Pixel >> 16) & 0xFF);
         float DB = (float)((Destination_Pixel >>  8) & 0xFF);

         u32 R = (u32)((DR * (1.0f-SA) + SR) + 0.5f);
         u32 G = (u32)((DG * (1.0f-SA) + SG) + 0.5f);
         u32 B = (u32)((DB * (1.0f-SA) + SB) + 0.5f);

         Destination_Row[X] = (R<<24) | (G<<16) | (B<<8) | 0xFF;
      }

      Source_Row += Source.Width;
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
               Clear(Backbuffer, Command->Color);
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
