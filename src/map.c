/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#define MAP_CHUNK_DIM 16
typedef struct {
   // int Positions[MAP_CHUNK_DIM][MAP_CHUNK_DIM];

   int Entity_Count;
   int Entity_Indices[256 * 2];
} map_chunk;

typedef struct {
   int X, Y, Z;
   bool Exists;
} map_chunk_coordinate;

#define MAP_CHUNK_COUNT_POW2 12
typedef struct {
   map_chunk_coordinate Coordinates[1 << MAP_CHUNK_COUNT_POW2];
   map_chunk                 Chunks[1 << MAP_CHUNK_COUNT_POW2];
} map;

static int Debug_Map_Chunk[MAP_CHUNK_DIM][MAP_CHUNK_DIM] = {
   {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
   {2, 2, 2, 2, 2, 2, 2, 1, 1, 2, 2, 2, 2, 2, 2, 2},
   {2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 2, 2},
   {2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 2, 2},
   {2, 2, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2},
   {2, 2, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2},
   {2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2},
   {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2},
   {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2},
   {2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2},
   {2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 2, 2},
   {2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 2, 2},
   {2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2},
   {2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2},
   {2, 2, 2, 2, 2, 2, 2, 1, 1, 2, 2, 2, 2, 2, 2, 2},
   {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
};

static u64 Hash_Chunk_Coordinate(int Chunk_X, int Chunk_Y, int Chunk_Z)
{
   // TODO: Better hash function.
   u64 Result = 0x100;

   Result ^= (u8)(Chunk_X >>  0) & 255;
   Result *= 1111111111111111111;
   Result ^= (u8)(Chunk_X >>  8) & 255;
   Result *= 1111111111111111111;
   Result ^= (u8)(Chunk_X >> 16) & 255;
   Result *= 1111111111111111111;
   Result ^= (u8)(Chunk_X >> 24) & 255;
   Result *= 1111111111111111111;

   Result ^= (u8)(Chunk_Y >>  0) & 255;
   Result *= 1111111111111111111;
   Result ^= (u8)(Chunk_Y >>  8) & 255;
   Result *= 1111111111111111111;
   Result ^= (u8)(Chunk_Y >> 16) & 255;
   Result *= 1111111111111111111;
   Result ^= (u8)(Chunk_Y >> 24) & 255;
   Result *= 1111111111111111111;

   Result ^= (u8)(Chunk_Z >>  0) & 255;
   Result *= 1111111111111111111;
   Result ^= (u8)(Chunk_Z >>  8) & 255;
   Result *= 1111111111111111111;
   Result ^= (u8)(Chunk_Z >> 16) & 255;
   Result *= 1111111111111111111;
   Result ^= (u8)(Chunk_Z >> 24) & 255;
   Result *= 1111111111111111111;

   Result ^= (Result >> 32);
   return(Result);
}

static map_chunk *Get_Map_Chunk_By_Chunk(map *Map, int Chunk_X, int Chunk_Y, int Chunk_Z, bool Insert_If_Not_Found)
{
   map_chunk *Result = 0;

   u64 Hash = Hash_Chunk_Coordinate(Chunk_X, Chunk_Y, Chunk_Z);
   u32 Mask = (1 << MAP_CHUNK_COUNT_POW2) - 1;
   u32 Step = (Hash >> (64 - MAP_CHUNK_COUNT_POW2)) | 1;

   int Index = Hash;
   int Attempt_Count = 100;
   while(Attempt_Count--)
   {
      Index = (Index + Step) & Mask;

      map_chunk_coordinate *Coordinate = Map->Coordinates + Index;
      if(!Coordinate->Exists)
      {
         if(Insert_If_Not_Found)
         {
            Coordinate->X = Chunk_X;
            Coordinate->Y = Chunk_Y;
            Coordinate->Z = Chunk_Z;
            Coordinate->Exists = true;

            Result = Map->Chunks + Index;
         }
         break;
      }
      else if(Chunk_X == Coordinate->X && Chunk_Y == Coordinate->Y && Chunk_Z == Coordinate->Z)
      {
         Result = Map->Chunks + Index;
         break;
      }
   }

   return(Result);
}

static map_chunk *Get_Map_Chunk(map *Map, int X, int Y, int Z, bool Insert_If_Not_Found)
{
   int Chunk_X = (int)Floor((float)X / MAP_CHUNK_DIM);
   int Chunk_Y = (int)Floor((float)Y / MAP_CHUNK_DIM);
   int Chunk_Z = Z;

   map_chunk *Result = Get_Map_Chunk_By_Chunk(Map, Chunk_X, Chunk_Y, Chunk_Z, Insert_If_Not_Found);
   return(Result);
}

static map_chunk *Query_Map_Chunk(map *Map, int X, int Y, int Z)
{
   map_chunk *Result = Get_Map_Chunk(Map, X, Y, Z, false);
   return(Result);
}

static map_chunk *Query_Map_Chunk_By_Chunk(map *Map, int Chunk_X, int Chunk_Y, int Chunk_Z)
{
   map_chunk *Result = Get_Map_Chunk_By_Chunk(Map, Chunk_X, Chunk_Y, Chunk_Z, false);
   return(Result);
}

static map_chunk *Insert_Map_Chunk(map *Map, int X, int Y, int Z)
{
   map_chunk *Result = Get_Map_Chunk(Map, X, Y, Z, true);
   return(Result);
}
