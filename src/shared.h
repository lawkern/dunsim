/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include <stdint.h>
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#include <stddef.h>
typedef ptrdiff_t size;

typedef struct {
   int X;
   int Y;
   int Z;
} int3;

static inline int3 Int3(int X, int Y, int Z)
{
   int3 Result = {X, Y, Z};
   return(Result);
}

typedef struct {
   float R;
   float G;
   float B;
   float A;
} vec4;

static inline vec4 Vec4(float R, float G, float B, float A)
{
   vec4 Result = {R, G, B, A};
   return(Result);
}

typedef struct {
   size Length;
   u8 *Data;
} string;

#define S(Literal) (string){sizeof(Literal)-1, (u8 *)(Literal)}

static string Span(u8 *Begin, u8 *End)
{
   string Result = {0};
   Result.Data = Begin;
   if(Begin)
   {
      Result.Length = End - Begin;
   }

   return(Result);
}

typedef struct {
   string Before;
   string After;
   bool Found;
} cut;

static cut Cut(string String, u8 Separator)
{
   cut Result = {0};

   if(String.Length > 0)
   {
      u8 *Begin = String.Data;
      u8 *End = Begin + String.Length;

      u8 *Cut_Position = Begin;
      while(Cut_Position < End && *Cut_Position != Separator)
      {
         Cut_Position++;
      }

      Result.Found = (Cut_Position < End);
      Result.Before = Span(Begin, Cut_Position);
      Result.After = Span(Cut_Position + Result.Found, End);
   }

   return(Result);
}

typedef struct {
   u8 *Begin;
   u8 *End;
} arena;

#define Assert(Cond) do { if(!(Cond)) { __builtin_trap(); } } while(0)
#define Array_Count(Array) (size)(sizeof(Array) / sizeof((Array)[0]))

#define Minimum(A, B) ((A) < (B) ? (A) : (B))
#define Maximum(A, B) ((A) > (B) ? (A) : (B))

#define Kilobytes(N) (1024 * (N))
#define Megabytes(N) (1024 * Kilobytes(N))
#define Gigabytes(N) (1024 * Megabytes(N))

static inline void *Zero_Size(void *Result, size Size)
{
   u8 *Bytes = (u8 *)Result;
   while(Size--)
   {
      *Bytes++ = 0;
   }
   return(Result);
}

#define Allocate(Arena, type, Count) (type *)Allocate_Size((Arena), (Count)*sizeof(type))

static inline void *Allocate_Size(arena *Arena, size Size)
{
   Assert(Arena->Begin < (Arena->End - Size));

   void *Result = Zero_Size(Arena->Begin, Size);
   Arena->Begin += Size;

   return(Result);
}
