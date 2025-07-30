/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

// NOTE: This file is where we keep any compiler-specific or
// architecture-specific intrinsics, separated by appropriate #ifdef's.

// TODO: Support MSVC once we decide to care about building on Windows.

// TODO: Support SIMD (SSE/AVX2/NEON)

#define Read_Barrier()  asm volatile("" ::: "memory")
#define Write_Barrier() asm volatile("" ::: "memory")

static inline u32 Atomic_Add(volatile u32 *Address, u32 Value)
{
   u32 Result = __sync_add_and_fetch(Address, Value);
   return(Result);
}

static inline u32 Atomic_Compare_Exchange(volatile u32 *Address, u32 Old, u32 New)
{
   u32 Result = __sync_val_compare_and_swap(Address, Old, New);
   return(Result);
}
