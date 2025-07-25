/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include <math.h>

#define TAU32 6.28318530718f

static float Sine(float Turns)
{
   float Radians = Turns * TAU32;
   float Result = sinf(Radians);
   return(Result);
}

static float Floor(float Value)
{
   float Result = floorf(Value);
   return(Result);
}

// NOTE: Max coordinates are currently defined to be one past the last value
// contained in the rectangle, i.e. for(int X = Min_X; X < Max_X; ++X) will loop
// over all horizontal coordintates inside a rectangle. This may change.

typedef struct {
   int Min_X;
   int Min_Y;
   int Max_X;
   int Max_Y;
} rectangle;

static rectangle To_Rectangle(int X, int Y, int Width, int Height)
{
   rectangle Result = {0};
   Result.Min_X = X;
   Result.Min_Y = Y;
   Result.Max_X = X + Width;
   Result.Max_Y = Y + Height;

   return(Result);
}

static bool Rectangles_Intersect(rectangle A, rectangle B)
{
   bool Result = !((A.Min_X >= B.Max_X) ||
                   (B.Min_X >= A.Max_X) ||
                   (A.Min_Y >= B.Max_Y) ||
                   (B.Min_Y >= A.Max_Y));

   return(Result);
}
