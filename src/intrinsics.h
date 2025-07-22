/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

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
