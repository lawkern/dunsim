/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

// NOTE: This pseudorandom number generation is based on the version described
// at http://burtleburtle.net/bob/rand/smallprng.html

typedef struct {
   u64 A;
   u64 B;
   u64 C;
   u64 D;
} random_entropy;

#define Rotate(X, K) (((X) << (K)) | ((X) >> (32 - (K))))
static u64 Random_Value(random_entropy *Entropy)
{
   u64 Entropy_E = Entropy->A - Rotate(Entropy->B, 27);
   Entropy->A    = Entropy->B ^ Rotate(Entropy->C, 17);
   Entropy->B    = Entropy->C + Entropy->D;
   Entropy->C    = Entropy->D + Entropy_E;
   Entropy->D    = Entropy_E  + Entropy->A;

   return(Entropy->D);
}
#undef Rotate

static random_entropy Random_Seed(u64 Seed)
{
   random_entropy Result;
   Result.A = 0xF1EA5EED;
   Result.B = Seed;
   Result.C = Seed;
   Result.D = Seed;

   for(u64 Index = 0; Index < 20; ++Index)
   {
      Random_Value(&Result);
   }

   return(Result);
}

static u64 Random_Range(random_entropy *Entropy, u64 Minimum, u64 Maximum)
{
   u64 Value = Random_Value(Entropy);
   u64 Range = Maximum - Minimum + 1;

   u64 Result = (Value % Range) + Minimum;
   return(Result);
}

static float Random_Unit_Interval(random_entropy *Entropy)
{
   u64 Value = Random_Value(Entropy);
   u64 Maximum = UINT64_MAX;

   float Result = (float)Value / (float)Maximum;
   return(Result);
}
