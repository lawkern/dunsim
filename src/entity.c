/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

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

static void Update_Entity_Chunk(int Entity_Index, map_chunk *Old, map_chunk *New, arena *Arena)
{
   if(Old)
   {
      // TODO: We can probably do better than scanning the blocks linearly.
      bool Removed = false;
      for(map_chunk_entities *Entities = Old->Entities; !Removed && Entities; Entities = Entities->Next)
      {
         for(int Index = 0; Index < Entities->Index_Count; ++Index)
         {
            if(Entities->Indices[Index] == Entity_Index)
            {
               Entities->Indices[Index] = Entities->Indices[Entities->Index_Count - 1];
               Entities->Index_Count--;

               Removed = true;
               break;
            }
         }
      }
      Assert(Removed);
   }

   if(New)
   {
      map_chunk_entities *Entities = New->Entities;
      if(!Entities || Entities->Index_Count == Array_Count(Entities->Indices))
      {
         map_chunk_entities *New_Entities = Allocate(Arena, map_chunk_entities, 1);
         New_Entities->Next = Entities;
         New->Entities = Entities = New_Entities;
      }
      Entities->Indices[Entities->Index_Count++] = Entity_Index;
   }
}

static int Create_Entity(game_state *Game_State, entity_type Type, int Width, int Height, int X, int Y, int Z, u32 Flags)
{
   Assert(Game_State->Entity_Count != Array_Count(Game_State->Entities));
   int ID = Game_State->Entity_Count++;

   entity *Entity = Game_State->Entities + ID;
   Entity->Type = Type;
   Entity->Width = Width;
   Entity->Height = Height;
   Entity->Position.X = X;
   Entity->Position.Y = Y;
   Entity->Position.Z = Z;
   Entity->Flags = Flags;

   map *Map = &Game_State->Map;
   map_chunk *Chunk = Insert_Map_Chunk(Map, X, Y, Z);
   Update_Entity_Chunk(ID, 0, Chunk, &Map->Arena);

   return(ID);
}

static entity *Get_Entity(game_state *Game_State, int ID)
{
   Assert(ID > 0);
   Assert(ID < Array_Count(Game_State->Entities));

   entity *Result = Game_State->Entities + ID;
   return(Result);
}

typedef struct {
   int3 Position;
   direction Direction;
   bool Ok;
} movement_result;

static movement_result Can_Move(game_state *Game_State, entity *Entity, int Delta_X, int Delta_Y)
{
   int3 Old_P = Entity->Position;
   int3 New_P = Entity->Position;
   New_P.X += Delta_X;
   New_P.Y += Delta_Y;

   movement_result Result = {0};
   Result.Ok = true;
   Result.Position = New_P;

   // TODO: Handle diagonal cross-chunk boundaries.
   map_chunk *Old_Chunk = Get_Map_Chunk(&Game_State->Map, Old_P.X, Old_P.Y, Old_P.Z);
   map_chunk *New_Chunk = Get_Map_Chunk(&Game_State->Map, New_P.X, New_P.Y, New_P.Z);

   if(Old_Chunk && New_Chunk)
   {
      rectangle Entity_Rect = To_Rectangle(New_P.X, New_P.Y, Entity->Width, Entity->Height);

      int Chunk_Count = (Old_Chunk == New_Chunk) ? 1 : 2;
      map_chunk *Chunks[] = {Old_Chunk, New_Chunk};

      for(int Chunk_Index = 0; Result.Ok && (Chunk_Index < Chunk_Count); ++Chunk_Index)
      {
         map_chunk *Chunk = Chunks[Chunk_Index];
         for(map_chunk_entities *Entities = Chunk->Entities; Entities; Entities = Entities->Next)
         {
            for(int Index = 0; Index < Entities->Index_Count; ++Index)
            {
               entity *Test = Game_State->Entities + Entities->Indices[Index];
               if(Test != Entity)
               {
                  bool Stairs = (Test->Type == Entity_Type_Stairs);
                  if(Has_Collision(Test) || Stairs)
                  {
                     rectangle Test_Rect = To_Rectangle(Test->Position.X, Test->Position.Y, Test->Width, Test->Height);
                     if(Rectangles_Intersect(Entity_Rect, Test_Rect))
                     {
                        if(Stairs)
                        {
                           Result.Position.Z = (Result.Position.Z) ? 0 : 1;
                        }
                        else
                        {
                           Result.Ok = false;
                        }
                        break;
                     }
                  }
               }
            }
         }
      }
   }
   else
   {
      Result.Ok = false;
   }

   return(Result);
}

static bool Move(game_state *Game_State, entity *Entity, int Delta_X, int Delta_Y)
{
   animation *Animation = &Entity->Animation;
   int3 *Position = &Entity->Position;

   movement_result Movement= {0};
   if(Delta_X && Delta_Y)
   {
      // TODO: This includes redundant checks just to support sliding along
      // walls when moving into them diagonally. This could be much simpler.
      if(Animation->Direction == Direction_Right || Animation->Direction == Direction_Left)
      {
         Movement = Can_Move(Game_State, Entity, 0, Delta_Y);
         if(Movement.Ok)
         {
            Delta_X = 0;
         }
         else
         {
            Movement = Can_Move(Game_State, Entity, Delta_X, 0);
            Delta_Y = 0;
         }
      }
      else
      {
         Movement = Can_Move(Game_State, Entity, Delta_X, 0);
         if(Movement.Ok)
         {
            Delta_Y = 0;
         }
         else
         {
            Movement = Can_Move(Game_State, Entity, 0, Delta_Y);
            Delta_X = 0;
         }
      }
   }
   else
   {
      Movement = Can_Move(Game_State, Entity, Delta_X, Delta_Y);
   }

   if     (Delta_X > 0) Animation->Direction = Direction_Right;
   else if(Delta_X < 0) Animation->Direction = Direction_Left;
   else if(Delta_Y > 0) Animation->Direction = Direction_Down;
   else if(Delta_Y < 0) Animation->Direction = Direction_Up;
   else                 Animation->Direction = Direction_None;

   if(Movement.Ok && (Delta_X || Delta_Y))
   {
      Animation->Offset_X = (float)Delta_X;
      Animation->Offset_Y = (float)Delta_Y;

      map_chunk *Old_Chunk = Get_Map_Chunk(&Game_State->Map, Position->X, Position->Y, Position->Z);
      *Position = Movement.Position;
      map_chunk *New_Chunk = Get_Map_Chunk(&Game_State->Map, Position->X, Position->Y, Position->Z);

      if(Old_Chunk != New_Chunk)
      {
         int ID = Entity - Game_State->Entities;
         Update_Entity_Chunk(ID, Old_Chunk, New_Chunk, &Game_State->Map.Arena);
      }
   }

   return(Movement.Ok);
}

static void Advance_Animation(animation *Animation, float Frame_Seconds, float Pixels_Per_Second)
{
   float Delta = Pixels_Per_Second * Frame_Seconds;

   if(Animation->Offset_X > 0)
   {
      Animation->Offset_X -= Delta;
      if(Animation->Offset_X < 0) Animation->Offset_X = 0;
   }
   else if(Animation->Offset_X < 0)
   {
      Animation->Offset_X += Delta;
      if(Animation->Offset_X > 0) Animation->Offset_X = 0;
   }

   if(Animation->Offset_Y > 0)
   {
      Animation->Offset_Y -= Delta;
      if(Animation->Offset_Y < 0) Animation->Offset_Y = 0;
   }
   else if(Animation->Offset_Y < 0)
   {
      Animation->Offset_Y += Delta;
      if(Animation->Offset_Y > 0) Animation->Offset_Y = 0;
   }
}
