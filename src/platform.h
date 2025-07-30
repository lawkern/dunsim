/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

// NOTE: This file specifies the API used by game code to perform
// platform-dependent functionality. Platform code is implemented in main_*.c
// files, which contain the entry point for each platform.


#define LOG(Name) void Name(char *Format, ...)
LOG(Log);

#define READ_ENTIRE_FILE(Name) string Name(arena *Arena, char *Path)
READ_ENTIRE_FILE(Read_Entire_File);

#define WRITE_ENTIRE_FILE(Name) bool Name(u8 *Memory, size Size, char *Path)
WRITE_ENTIRE_FILE(Write_Entire_File);

#define WORK_TASK(Name) void Name(void *Data)
typedef WORK_TASK(work_task);

typedef struct {
   work_task *Task;
   void *Data;
} work_queue_entry;

typedef struct {
   volatile u32 Read_Index;
   volatile u32 Write_Index;

   volatile u32 Completion_Target;
   volatile u32 Completion_Count;

   void *Semaphore;
   work_queue_entry Entries[512];
} work_queue;

#define ENQUEUE_WORK(name) void name(work_queue *Queue, work_task *Task, void *Data)
ENQUEUE_WORK(Enqueue_Work);

#define FLUSH_QUEUE(name) void Name(work_queue *Queue)
FLUSH_QUEUE(Flush_Queue);
