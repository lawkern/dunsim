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

static string Entity_Type_Names[Entity_Type_Count] =
{
   [Entity_Type_Null]   = S("Null"),
   [Entity_Type_Floor]  = S("Floor"),
   [Entity_Type_Wall]   = S("Wall"),
   [Entity_Type_Stairs] = S("Stairs"),
   [Entity_Type_Camera] = S("Camera"),
   [Entity_Type_Player] = S("Player"),
   [Entity_Type_Dragon] = S("Dragon"),
};

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
} movement_direction;

typedef struct {
   movement_direction Direction;
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

static bool Has_Collision(entity *Entity)
{
   u32 Flags = Entity_Flag_Active | Entity_Flag_Collides;

   bool Result = (Entity->Flags & Flags) == Flags;
   return(Result);
}

static bool Is_Visible(entity *Entity)
{
   u32 Flags = Entity_Flag_Active | Entity_Flag_Visible;

   bool Result = (Entity->Flags & Flags) == Flags;
   return(Result);
}

static bool Is_Active(entity *Entity)
{
   bool Result = (Entity->Flags & Entity_Flag_Active);
   return(Result);
}

static void Activate_Entity(entity *Entity)
{
   Entity->Flags |= Entity_Flag_Active;
}

static void Deactivate_Entity(entity *Entity)
{
   Entity->Flags &= ~(u32)Entity_Flag_Active;
}

static bool Is_Animating(entity *Entity)
{
   bool Result = (Entity->Animation.Offset_X != 0.0f || Entity->Animation.Offset_Y != 0.0f);
   return(Result);
}
