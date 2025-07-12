/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#define KILOBYTES(N) (1024 * (N))
#define MEGABYTES(N) (1024 * KILOBYTES(N))

typedef struct {
   size Size;
   size Used;
   u8 *Base;
} arena;

#define Allocate(Arena, type, Count) (type)Allocate_Size((Arena), (Count)*sizeof(type))

static void *Allocate_Size(arena *Arena, size Size)
{
   void *Result = 0;
   if((Arena->Size - Arena->Used) < Size)
   {
      Result = Arena->Base + Arena->Used;
      Arena->Used += Size;
   }

   return(Result);
}
