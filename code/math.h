/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include <math.h>

#define TAU32 6.2831853f

typedef union {
   struct
   {
      float X;
      float Y;
   };
   struct
   {
      float U;
      float V;
   };
   float Elements[2];
} vec2;

typedef union {
   struct
   {
      float X;
      float Y;
      float Z;
      float W;
   };
   struct
   {
      float R;
      float G;
      float B;
      float A;
   };
   float Elements[4];
} vec4;

static inline vec2 Vec2(float X, float Y)
{
   vec2 Result = {X, Y};
   return(Result);
}

static inline vec4 Vec4(float R, float G, float B, float A)
{
   vec4 Result = {R, G, B, A};
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
