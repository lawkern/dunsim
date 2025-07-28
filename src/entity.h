/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

typedef enum {
   Entity_Type_Null,
   Entity_Type_Floor,
   Entity_Type_Wall,
   Entity_Type_Stairs,
   Entity_Type_Camera,
   Entity_Type_Player,
   Entity_Type_Dragon,

   Entity_Type_Count
} entity_type;

typedef enum {
   Entity_Flag_Active   = 0x01,
   Entity_Flag_Visible  = 0x02,
   Entity_Flag_Collides = 0x04,
} entity_flag;

typedef enum {
   Direction_None,
   Direction_Up,
   Direction_Down,
   Direction_Left,
   Direction_Right,

   Direction_Count,
} direction;

typedef struct {
   direction Direction;
   float Offset_X;
   float Offset_Y;
} animation;

typedef struct {
   entity_type Type;
   u32 Flags;

   int Width;
   int Height;
   int3 Position;
   animation Animation;
} entity;
