/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

static DRAW_CLEAR(Draw_Clear)
{
   glViewport(0, 0, Destination.Width, Destination.Height);

   glMatrixMode(GL_TEXTURE);
   glLoadIdentity();

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

   glClearColor(Color.R, Color.G, Color.B, Color.A);
   glClear(GL_COLOR_BUFFER_BIT);
}

static DRAW_RECTANGLE(Draw_Rectangle)
{
   float Min_X = Map_Binormal(X, 0, Destination.Width);
   float Min_Y = Map_Binormal(Y, 0, Destination.Height);
   float Max_X = Map_Binormal(X+Width, 0, Destination.Width);
   float Max_Y = Map_Binormal(Y+Height, 0, Destination.Height);

   Min_Y *= -1.0f;
   Max_Y *= -1.0f;

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

static DRAW_TEXTURE(Draw_Texture)
{
}

static DRAW_TEXTURED_QUAD(Draw_Textured_Quad)
{
}
