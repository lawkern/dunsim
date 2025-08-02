/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include <math.h>

#define TAU32 6.2831853f

static inline float Sine(float Turns)
{
   float Radians = Turns * TAU32;
   float Result = sinf(Radians);
   return(Result);
}

static inline float Cosine(float Turns)
{
   float Radians = Turns * TAU32;
   float Result = cosf(Radians);
   return(Result);
}

static inline float Floor(float Value)
{
   float Result = floorf(Value);
   return(Result);
}

static inline float Ceiling(float Value)
{
   float Result = ceilf(Value);
   return(Result);
}

static inline float Clamp(float Value, float Min, float Max)
{
   float Result = Minimum(Maximum(Value, Min), Max);
   return(Result);
}

static inline float Clamp_01(float Value)
{
   float Result = Minimum(Maximum(Value, 0.0f), 1.0f);
   return(Result);
}

// Vector Math:

static vec2 Add2(vec2 A, vec2 B)
{
   vec2 Result = {A.X+B.X, A.Y+B.Y};
   return(Result);
}

static vec2 Sub2(vec2 A, vec2 B)
{
   vec2 Result = {A.X-B.X, A.Y-B.Y};
   return(Result);
}

static vec2 Mul2(vec2 Vector, float Scalar)
{
   vec2 Result = Vector;
   Result.X *= Scalar;
   Result.Y *= Scalar;

   return(Result);
}

static vec2 Neg2(vec2 Vector)
{
   vec2 Result = {-Vector.X, -Vector.Y};
   return(Result);
}

static float Dot2(vec2 A, vec2 B)
{
   float Result = A.X*B.X + B.Y*A.Y;
   return(Result);
}

static vec2 Perp2(vec2 Vector)
{
   vec2 Result = {-Vector.Y, Vector.X};
   return(Result);
}

static float Length2_Squared(vec2 Vector)
{
   float Result = Vector.X*Vector.X + Vector.Y*Vector.Y;
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

static bool Rectangle_Contains(rectangle Outer, rectangle Inner)
{
   bool Result = (Inner.Min_X >= Outer.Min_X &&
                  Inner.Min_Y >= Outer.Min_Y &&
                  Inner.Max_X <= Outer.Max_X &&
                  Inner.Max_Y <= Outer.Max_Y);
   return(Result);
}
