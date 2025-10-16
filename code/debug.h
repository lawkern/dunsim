/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

typedef struct {
   char *Name;
   u64 Start;
   u64 Elapsed;
   int Hits;
} debug_profile;

static struct {
   debug_profile Profiles[128];
} Debug_Profiler;

#define BEGIN_PROFILE(Name) int Debug_Profile_Index_##Name = Begin_Profile(#Name, __COUNTER__)
#define END_PROFILE(Name) End_Profile(Debug_Profile_Index_##Name)

static inline int Begin_Profile(char *Name, int Profile_Index)
{
   debug_profile *Profile = Debug_Profiler.Profiles + Profile_Index;
   Profile->Name = Name;
   Profile->Start = Cpu_Cycle_Counter();

   return(Profile_Index);
}

static inline void End_Profile(int Profile_Index)
{
   debug_profile *Profile = Debug_Profiler.Profiles + Profile_Index;
   Profile->Elapsed += (Cpu_Cycle_Counter() - Profile->Start);
   Profile->Hits++;
}
