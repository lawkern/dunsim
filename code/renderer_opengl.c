/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

static DRAW_CLEAR(OpenGL_Draw_Clear)
{
   glClearColor(Color.R, Color.G, Color.B, Color.A);
   glClear(GL_COLOR_BUFFER_BIT);
}

static DRAW_RECTANGLE(OpenGL_Draw_Rectangle)
{
   float Min_X = X;
   float Min_Y = Y;
   float Max_X = X + Width;
   float Max_Y = Y + Height;

   glBegin(GL_TRIANGLES);
   glColor4f(Color.R, Color.G, Color.B, Color.A);

   glVertex2f(Min_X, Min_Y);
   glVertex2f(Max_X, Min_Y);
   glVertex2f(Max_X, Max_Y);

   glVertex2f(Min_X, Min_Y);
   glVertex2f(Max_X, Max_Y);
   glVertex2f(Min_X, Max_Y);

   glEnd();
}

static DRAW_TEXTURE(OpenGL_Draw_Texture)
{
}

static DRAW_TEXTURED_QUAD(OpenGL_Draw_Textured_Quad)
{
}

static void Render_With_OpenGL(renderer *Renderer)
{
   texture Backbuffer = Renderer->Backbuffer;
   float Pixels_Per_Meter = Renderer->Pixels_Per_Meter;

   // NOTE: Clear entires screen to black.
   glClearColor(0, 0, 0, 1);
   glClear(GL_COLOR_BUFFER_BIT);

   glViewport(Renderer->Bounds_X, Renderer->Bounds_Y, Renderer->Bounds_Width, Renderer->Bounds_Height);

   glMatrixMode(GL_TEXTURE);
   glLoadIdentity();

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   // NOTE: B and D are reversed since we render top down.
   float A = 2.0f/(Renderer->Backbuffer.Width);
   float B = -2.0f/(Renderer->Backbuffer.Height);
   float C = -1.0f;
   float D = 1.0f;

   glMatrixMode(GL_PROJECTION);
   float Projection[] =
   {
      A, 0, 0, 0,
      0, B, 0, 0,
      0, 0, 1, 0,
      C, D, 0, 1,
   };
   glLoadMatrixf(Projection);
   // glLoadIdentity();

   glEnable(GL_SCISSOR_TEST);
   glScissor(Renderer->Bounds_X, Renderer->Bounds_Y, Renderer->Bounds_Width, Renderer->Bounds_Height);

   for(int Queue_Index = 0; Queue_Index < Array_Count(Renderer->Queues); ++Queue_Index)
   {
      render_queue *Queue = Renderer->Queues[Queue_Index];
      for(int Command_Index = 0; Command_Index < Queue->Command_Count; ++Command_Index)
      {
         render_command *Command = Queue->Commands + Command_Index;
         switch(Command->Type)
         {
            case Render_Command_Clear: {
               OpenGL_Draw_Clear(Backbuffer, Command->Color);
            } break;

            case Render_Command_Rectangle: {
               OpenGL_Draw_Rectangle(Backbuffer, Command->X, Command->Y, Command->Width, Command->Height, Command->Color);
            } break;

            case Render_Command_Texture: {
               OpenGL_Draw_Texture(Backbuffer, Command->Texture, Command->X, Command->Y, Command->Width, Command->Height);
            } break;

            case Render_Command_Textured_Quad: {
               OpenGL_Draw_Textured_Quad(Backbuffer, Command->Texture, Command->Origin, Command->X_Axis, Command->Y_Axis);
            } break;

            case Render_Command_Debug_Basis: {
               vec2 Origin = Command->Origin;
               vec2 X_Axis = Command->X_Axis;
               vec2 Y_Axis = Command->Y_Axis;

               OpenGL_Draw_Textured_Quad(Backbuffer, Command->Texture, Origin, X_Axis, Y_Axis);

               vec2 Origin0 = Origin;
               vec2 Origin1 = Add2(Origin, X_Axis);
               vec2 Origin2 = Add2(Origin, Y_Axis);
               float Dim = 5;

               OpenGL_Draw_Rectangle(Backbuffer, Origin0.X, Origin0.Y, Dim, Dim, Vec4(1, 1, 0, 1));
               OpenGL_Draw_Rectangle(Backbuffer, Origin1.X, Origin1.Y, Dim, Dim, Vec4(1, 0, 0, 1));
               OpenGL_Draw_Rectangle(Backbuffer, Origin2.X, Origin2.Y, Dim, Dim, Vec4(0, 1, 0, 1));
            } break;

            default: {
               Assert(0);
            } break;
         }
      }
      Queue->Command_Count = 0;
   }

   glDisable(GL_SCISSOR_TEST);
}
