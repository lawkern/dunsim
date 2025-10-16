/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

typedef struct map_chunk_entities map_chunk_entities;
struct map_chunk_entities
{
   map_chunk_entities *Next;

   int Indices[32];
   int Index_Count;
};

#define MAP_CHUNK_DIM 16
typedef struct {
   map_chunk_entities *Entities;
} map_chunk;

typedef struct {
   int X, Y, Z;
   bool Exists;
} map_chunk_coordinate;

#define MAP_CHUNK_COUNT_POW2 12
typedef struct {
   arena Arena;

   map_chunk_coordinate Coordinates[1 << MAP_CHUNK_COUNT_POW2];
   map_chunk                 Chunks[1 << MAP_CHUNK_COUNT_POW2];
} map;
