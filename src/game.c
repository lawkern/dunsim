/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#include "game.h"

#include "math.c"
#include "random.c"
#include "assets.c"
#include "map.c"

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
   int X, Y, Z;
} position;

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

typedef struct {
   entity_type Type;
   string Name;

   int Width;
   int Height;
   position Position;
   animation Animation;

   bool Active;
} entity;

typedef struct {
   arena Arena;
   arena Scratch;
   random_entropy Entropy;

   text_font Varia_Font;
   text_font Fixed_Font;

   int Active_Textbox_Index;
   string Textbox_Dialogue[4];

   game_texture Upstairs;
   game_texture Downstairs;

   map Map;
   position Camera;

   int Entity_Count;
   entity Entities[1024*1024];
} game_state;

static void Clear(game_texture Destination, u32 Color)
{
   for(int Y = 0; Y < Destination.Height; ++Y)
   {
      for(int X = 0; X < Destination.Width; ++X)
      {
         Destination.Memory[(Destination.Width * Y) + X] = Color;
      }
   }
}

static void Draw_Rectangle(game_texture Destination, int X, int Y, int Width, int Height, u32 Color)
{
   int Min_X = Maximum(X, 0);
   int Min_Y = Maximum(Y, 0);
   int Max_X = Minimum(Destination.Width, X + Width);
   int Max_Y = Minimum(Destination.Height, Y + Height);

   for(int Y = Min_Y; Y < Max_Y; ++Y)
   {
      for(int X = Min_X; X < Max_X; ++X)
      {
         Destination.Memory[(Destination.Width * Y) + X] = Color;
      }
   }
}

static void Draw_Outline(game_texture Destination, int X, int Y, int Width, int Height, int Weight, u32 Color)
{
   Draw_Rectangle(Destination, X, Y, Width-Weight, Weight, Color); // Top
   Draw_Rectangle(Destination, X+Weight, Y+Height-Weight, Width-Weight, Weight, Color); // Bottom
   Draw_Rectangle(Destination, X, Y+Weight, Weight, Height-Weight, Color); // Left
   Draw_Rectangle(Destination, X+Width-Weight, Y, Weight, Height-Weight, Color); // Right
}

static void Draw_Bitmap(game_texture Destination, game_texture Source, float X, float Y)
{
   X += Source.Offset_X;
   Y += Source.Offset_Y;

   int Min_X = Maximum(X, 0);
   int Min_Y = Maximum(Y, 0);
   int Max_X = Minimum(Destination.Width, X + Source.Width);
   int Max_Y = Minimum(Destination.Height, Y + Source.Height);

   int Clip_X_Offset = Min_X - X;
   int Clip_Y_Offset = Min_Y - Y;

   u32 *Source_Row = Source.Memory + Clip_Y_Offset*Source.Width;
   for(int Destination_Y = Min_Y; Destination_Y < Max_Y; ++Destination_Y)
   {
      int X_Offset = Clip_X_Offset;
      u32 *Destination_Row = Destination.Memory + Destination.Width*Destination_Y;

      for(int Destination_X = Min_X; Destination_X < Max_X; ++Destination_X)
      {
         u32 Source_Pixel = Source_Row[X_Offset++];
         u32 *Destination_Pixel = Destination_Row + Destination_X;

         float SR = (float)((Source_Pixel >> 24) & 0xFF);
         float SG = (float)((Source_Pixel >> 16) & 0xFF);
         float SB = (float)((Source_Pixel >>  8) & 0xFF);
         float SA = (float)((Source_Pixel >>  0) & 0xFF) / 255.0f;

         float DR = (float)((*Destination_Pixel >> 24) & 0xFF);
         float DG = (float)((*Destination_Pixel >> 16) & 0xFF);
         float DB = (float)((*Destination_Pixel >>  8) & 0xFF);

         u32 R = (u32)((DR * (1.0f-SA) + SR) + 0.5f);
         u32 G = (u32)((DG * (1.0f-SA) + SG) + 0.5f);
         u32 B = (u32)((DB * (1.0f-SA) + SB) + 0.5f);

         *Destination_Pixel = (R<<24) | (G<<16) | (B<<8) | 0xFF;
      }

      Source_Row += Source.Width;
   }
}

static void Advance_Text_Line(text_font *Font, text_size Size, int *Y)
{
   float Scale = Font->Glyphs[Size].Scale;

   float Line_Advance = Scale * (Font->Ascent - Font->Descent + Font->Line_Gap);
   *Y += Line_Advance;
}

static int Get_Text_Width(text_font *Font, text_size Size, string Text)
{
   int Result = 0;

   float Scale = Font->Glyphs[Size].Scale;
   for(size Index = 0; Index < Text.Length; ++Index)
   {
      if(Index != Text.Length-1)
      {
         int C0 = Text.Data[Index + 0];
         int C1 = Text.Data[Index + 1];

         Result += (Scale * Font->Distances[(C0 * GLYPH_COUNT) + C1]);
      }
   }

   return(Result);
}

static void Draw_Text(game_texture Destination, text_font *Font, text_size Size, int X, int Y, string Text)
{
   if(Font->Loaded)
   {
      text_glyphs *Glyphs = Font->Glyphs + Size;
      float Scale = Glyphs->Scale;

      for(size Index = 0; Index < Text.Length; ++Index)
      {
         int Codepoint = Text.Data[Index];

         game_texture Glyph = Glyphs->Bitmaps[Codepoint];
         Draw_Bitmap(Destination, Glyph, X, Y);

         if(Index != Text.Length-1)
         {
            int Next_Codepoint = Text.Data[Index + 1];
            int Pair_Index = (Codepoint * GLYPH_COUNT) + Next_Codepoint;
            X += (Scale * Font->Distances[Pair_Index]);
         }
      }
   }
}

static void Draw_Textbox(game_state *Game_State, game_texture Destination, string Text)
{
   text_font *Font = &Game_State->Varia_Font;
   if(Font->Loaded)
   {
      text_size Size = Text_Size_Medium;
      text_glyphs *Glyphs = Font->Glyphs + Size;

      float Scale = Glyphs->Scale;
      float Line_Advance = Scale * (Font->Ascent - Font->Descent + Font->Line_Gap);

      int Margin = 10;
      int Padding = 25;

      int Line_Count = 1;
      int Max_Line_Count = 6;

      int Box_Width = Destination.Width - 2*Margin;
      int Box_Height = Max_Line_Count*Line_Advance + 2*Padding;
      int Box_X = Margin;
      int Box_Y = Destination.Height - Margin - Box_Height;
      Draw_Rectangle(Destination, Box_X, Box_Y, Box_Width, Box_Height, 0x000000FF);

      int Text_X = Margin + Padding;
      int Text_Y = Box_Y + Padding + Scale*Font->Ascent;

      cut Words = {0};
      Words.After = Text;
      while(Words.After.Length)
      {
         Words = Cut(Words.After, ' ');
         string Word = Words.Before;

         int Width = Get_Text_Width(Font, Size, Word);
         if((Box_X + Box_Width - Padding) < (Text_X + Width))
         {
            Text_X = Margin + Padding;
            Advance_Text_Line(Font, Size, &Text_Y);
            Line_Count++;
         }

         for(int Index = 0; Index < Word.Length; ++Index)
         {
            int Codepoint = Word.Data[Index];
            if(Codepoint == '\n')
            {
               Text_X = Margin + Padding;
               Advance_Text_Line(Font, Size, &Text_Y);
               Line_Count++;
            }
            else
            {
               game_texture Glyph = Glyphs->Bitmaps[Codepoint];
               Draw_Bitmap(Destination, Glyph, Text_X, Text_Y);

               int Next_Codepoint = (Index != Word.Length-1) ? Word.Data[Index + 1] : ' ';
               int Pair_Index = (Codepoint * GLYPH_COUNT) + Next_Codepoint;
               Text_X += (Scale * Font->Distances[Pair_Index]);
            }
         }

         if(Words.After.Length)
         {
            int Next_Codepoint = Words.After.Data[0];
            Text_X += (Scale * Font->Distances[(' ' * GLYPH_COUNT) + Next_Codepoint]);
         }
      }

      Assert(Line_Count <= Max_Line_Count);
   }
}

static bool Can_Move(game_state *Game_State, entity *Entity, int Delta_X, int Delta_Y)
{
   bool Result = true;

   position P = Entity->Position;
   rectangle Entity_Rect = To_Rectangle(P.X+Delta_X, P.Y+Delta_Y, Entity->Width, Entity->Height);

   // TODO: Handle cross-chunk boundaries.
   map_chunk *Chunk = Query_Map_Chunk(&Game_State->Map, P.X, P.Y, P.Z);
   for(int Index = 0; Index < Chunk->Entity_Count; ++Index)
   {
      int Entity_Index = Chunk->Entity_Indices[Index];
      entity *Test = Game_State->Entities + Entity_Index;
      if(Test != Entity && Test->Active)
      {
         rectangle Test_Rect = To_Rectangle(Test->Position.X, Test->Position.Y, Test->Width, Test->Height);
         if(Rectangles_Intersect(Entity_Rect, Test_Rect))
         {
            Result = false;
            break;
         }
      }
   }

   return(Result);
}

static bool Move(game_state *Game_State, entity *Entity, int Delta_X, int Delta_Y)
{
   bool Ok = false;

   animation *Animation = &Entity->Animation;
   position *Position = &Entity->Position;

   if(Delta_X && Delta_Y)
   {
      // TODO: This includes redundant checks just to support sliding along
      // walls when moving into them diagonally. This could be much simpler.
      if(Animation->Direction == Direction_Right || Animation->Direction == Direction_Left)
      {
         Ok = Can_Move(Game_State, Entity, 0, Delta_Y);
         if(Ok)
         {
            Delta_X = 0;
         }
         else
         {
            Ok = Can_Move(Game_State, Entity, Delta_X, 0);
            Delta_Y = 0;
         }
      }
      else
      {
         Ok = Can_Move(Game_State, Entity, Delta_X, 0);
         if(Ok)
         {
            Delta_Y = 0;
         }
         else
         {
            Ok = Can_Move(Game_State, Entity, 0, Delta_Y);
            Delta_X = 0;
         }
      }
   }
   else
   {
      Ok = Can_Move(Game_State, Entity, Delta_X, Delta_Y);
   }

   if     (Delta_X > 0) Animation->Direction = Direction_Right;
   else if(Delta_X < 0) Animation->Direction = Direction_Left;
   else if(Delta_Y > 0) Animation->Direction = Direction_Down;
   else if(Delta_Y < 0) Animation->Direction = Direction_Up;
   else                 Animation->Direction = Direction_None;

   if(Ok && (Delta_X || Delta_Y))
   {
      Animation->Offset_X = (float)Delta_X;
      Animation->Offset_Y = (float)Delta_Y;

      Position->X += Delta_X;
      Position->Y += Delta_Y;
   }

   return(Ok);
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

static entity *Create_Entity(game_state *Game_State, entity_type Type, string Name, int Width, int Height, int X, int Y, int Z)
{
   Assert(Game_State->Entity_Count != Array_Count(Game_State->Entities));
   int Index = Game_State->Entity_Count++;

   entity *Result = Game_State->Entities + Index;
   Result->Type = Type;
   Result->Name = Name;
   Result->Width = Width;
   Result->Height = Height;
   Result->Position.X = X;
   Result->Position.Y = Y;
   Result->Position.Z = Z;

   map_chunk *Chunk = Insert_Map_Chunk(&Game_State->Map, X, Y, Z);
   Assert(Chunk->Entity_Count < Array_Count(Chunk->Entity_Indices));
   Chunk->Entity_Indices[Chunk->Entity_Count++] = Index;

   return(Result);
}

UPDATE(Update)
{
   game_state *Game_State = (game_state *)Memory.Base;

   arena *Arena = &Game_State->Arena;
   arena *Scratch = &Game_State->Scratch;
   random_entropy *Entropy = &Game_State->Entropy;
   map *Map = &Game_State->Map;

   int Tile_Pixels = Backbuffer.Height / 32;

   if(!Arena->Begin)
   {
      Arena->Begin = Memory.Base + sizeof(*Game_State);
      Arena->End = Arena->Begin + Megabytes(64);

      Scratch->Begin = Arena->End;
      Scratch->End = Memory.Base + Memory.Size;

      Assert(Scratch->Begin < Scratch->End);

      Game_State->Entropy = Random_Seed(0x13);

      Game_State->Camera.X = 8;
      Game_State->Camera.Y = 8;

      Game_State->Entity_Count++; // Skip null entity.
      for(int Player_Index = 0; Player_Index < GAME_CONTROLLER_COUNT; ++Player_Index)
      {
         Create_Entity(Game_State, Entity_Type_Player, S("Player"), 2, 2,
                       Game_State->Camera.X,
                       Game_State->Camera.Y,
                       Game_State->Camera.Z);
      }

      entity *Dragon = Create_Entity(Game_State, Entity_Type_Dragon, S("Dragon"), 4, 4, 6, -8, 0);
      Dragon->Active = true;

      int Chunk_X = 0;
      int Chunk_Y = 0;
      int Chunk_Z = 0;
      movement_direction Direction_Towards_Previous_Room = Direction_None;
      for(int Chunk_Index = 0; Chunk_Index < 10; ++Chunk_Index)
      {
         int X = Chunk_X * MAP_CHUNK_DIM;
         int Y = Chunk_Y * MAP_CHUNK_DIM;
         int Z = Chunk_Z;

         map_chunk *Chunk = Insert_Map_Chunk(Map, X, Y, Z);
         for(int Offset_Y = 0; Offset_Y < MAP_CHUNK_DIM; ++Offset_Y)
         {
            for(int Offset_X = 0; Offset_X < MAP_CHUNK_DIM; ++Offset_X)
            {
               if(Debug_Map_Chunk[Offset_Y][Offset_X] == 2)
               {
                  entity *Wall = Create_Entity(Game_State, Entity_Type_Wall, S("Wall"), 1, 1, X+Offset_X, Y+Offset_Y, Z);
                  Wall->Active = true;
               }
            }
         }

         movement_direction Direction = Direction_Towards_Previous_Room;
         while(Direction == Direction_Towards_Previous_Room)
         {
            Direction = Random_Range(Entropy, Direction_None, Direction_Count-1);
         }

         switch(Direction)
         {
            case Direction_Up: {
               Chunk_Y -= 1;
               Direction_Towards_Previous_Room = Direction_Down;
            } break;

            case Direction_Down: {
               Chunk_Y += 1;
               Direction_Towards_Previous_Room = Direction_Up;
            } break;

            case Direction_Left: {
               Chunk_X -= 1;
               Direction_Towards_Previous_Room = Direction_Right;
            } break;

            case Direction_Right: {
               Chunk_X += 1;
               Direction_Towards_Previous_Room = Direction_Left;
            } break;

            case Direction_None: {
               Chunk_Z = (Chunk_Z) ? 0 : 1;
            } break;

            default: {
               Assert(0);
            } break;
         }
      }

      Load_Font(&Game_State->Varia_Font, Arena, *Scratch, "data/Inter.ttf", Tile_Pixels);
      Load_Font(&Game_State->Fixed_Font, Arena, *Scratch, "data/JetBrainsMono.ttf", Tile_Pixels);
      if(!Game_State->Varia_Font.Loaded)
      {
         Log("During development, make sure to run the program from the project root folder.");
      }

      Game_State->Upstairs = Load_Image(Arena, "data/upstairs.png");
      Game_State->Downstairs = Load_Image(Arena, "data/downstairs.png");

      Game_State->Textbox_Dialogue[1] = S(
         "THE FIRST WORLD\n"
         "Within the earth there was fire. "
         "Mankind succumbed to greed and touched the forbidden sun. "
         "The enslaved prayed, and the sun god appeared. "
         "The earth god raged, and with its serpent of hellfire, shrouded the world in death and darkness. "
         "And they will never meet."
         );

      Game_State->Textbox_Dialogue[2] = S(
         "THE SECOND WORLD\n"
         "Within the void there was breath. "
         "The forest god tamed demons and the sun spread the fires of war. "
         "Those of the half-moon dreamed. "
         "Those of the moon dreamed. "
         "Man killed the sun and became god, and the sea god stormed. "
         "And they will never meet."
         );

      Game_State->Textbox_Dialogue[3] = S(
         "THE THIRD WORLD\n"
         "Within the chaos there was emptiness. "
         "The inconvenient remnants recall the promised day and hear the voice of the half-moon. "
         "The sun god dances and laughs, guiding the world to its end. "
         "The sun returns and brings a new morning. "
         "And they will surely meet."
         );
   }

   // General Input Handling.
   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      game_controller *Controller = Input->Controllers + Controller_Index;
      if(Controller->Connected)
      {
         entity *Player = Game_State->Entities + Controller_Index + 1;
         Player->Active = Controller->Connected;


         if(Was_Pressed(Controller->Action_Up))
         {
            Game_State->Active_Textbox_Index++;
            if(Game_State->Active_Textbox_Index == Array_Count(Game_State->Textbox_Dialogue))
            {
               Game_State->Active_Textbox_Index = 0;
            }
         }

         bool Moving_Camera = Is_Held(Controller->Shoulder_Right);
         if(Moving_Camera)
         {
            int Camera_Delta = 4;
            if(Is_Held(Controller->Shoulder_Right))
            {
               if(Was_Pressed(Controller->Move_Up))    Game_State->Camera.Y -= Camera_Delta;
               if(Was_Pressed(Controller->Move_Down))  Game_State->Camera.Y += Camera_Delta;
               if(Was_Pressed(Controller->Move_Left))  Game_State->Camera.X -= Camera_Delta;
               if(Was_Pressed(Controller->Move_Right)) Game_State->Camera.X += Camera_Delta;
            }
         }

         int Camera_Stick_Delta = 2;
         Game_State->Camera.X += (Camera_Stick_Delta * Controller->Stick_Right_X);
         Game_State->Camera.Y += (Camera_Stick_Delta * Controller->Stick_Right_Y);

         if(Was_Pressed(Controller->Start))
         {
            Game_State->Camera.X = Player->Position.X;
            Game_State->Camera.Y = Player->Position.Y;
         }
      }
   }

   // Entity Update.
   for(int Entity_Index = 0; Entity_Index < Game_State->Entity_Count; ++Entity_Index)
   {
      entity *Entity = Game_State->Entities + Entity_Index;
      if(Entity->Active)
      {
         switch(Entity->Type)
         {
            case Entity_Type_Player: {
               if(Entity->Animation.Offset_X == 0.0f && Entity->Animation.Offset_Y == 0.0f)
               {
                  game_controller *Controller = Input->Controllers + Entity_Index - 1;
                  bool Dash = Is_Held(Controller->Action_Down);
                  int Delta = (Dash) ? 2 : 1;

                  bool Up    = Is_Held(Controller->Move_Up);
                  bool Down  = Is_Held(Controller->Move_Down);
                  bool Left  = Is_Held(Controller->Move_Left);
                  bool Right = Is_Held(Controller->Move_Right);

                  int Delta_X = 0;
                  int Delta_Y = 0;

                  if(Up)    Delta_Y -= Delta;
                  if(Down)  Delta_Y += Delta;
                  if(Left)  Delta_X -= Delta;
                  if(Right) Delta_X += Delta;

                  if((Delta_X || Delta_Y) && Move(Game_State, Entity, Delta_X, Delta_Y))
                  {
                     Game_State->Camera.Z = Entity->Position.Z;
                  }
               }
               Advance_Animation(&Entity->Animation, Frame_Seconds, 10.0f);
            } break;

            case Entity_Type_Dragon: {
               if(Entity->Animation.Offset_X == 0.0f && Entity->Animation.Offset_Y == 0.0f)
               {
                  int Delta_X = 0;
                  int Delta_Y = 0;
                  switch(Random_Range(Entropy, Direction_None, Direction_Count-1))
                  {
                     case Direction_Up:    Delta_Y--; break;
                     case Direction_Down:  Delta_Y++; break;
                     case Direction_Left:  Delta_X--; break;
                     case Direction_Right: Delta_X++; break;
                     default: break;
                  }
                  if((Delta_X || Delta_Y) && Move(Game_State, Entity, Delta_X, Delta_Y))
                  {
                     // ...
                  }
               }
               Advance_Animation(&Entity->Animation, Frame_Seconds, 5.0f);
            } break;

            default: {
            } break;
         }
      }
   }

   position Camera = Game_State->Camera;

   // Rendering
   u32 Palettes[2][4] = {
      {0x000088FF, 0x0000CCFF, 0x0000FFFF, 0x008800FF},
      {0x008800FF, 0x00CC00FF, 0x00FF00FF, 0x000088FF},
   };
   u32 *Palette = Palettes[Camera.Z];

   Clear(Backbuffer, Palette[0]);

   // TODO: Loop over the surrounding chunks instead of tiles so that we don't
   // have to query for the chunk on each iteration.
   int Min_X = Camera.X - MAP_CHUNK_DIM*2;
   int Max_X = Camera.X + MAP_CHUNK_DIM*2;

   int Min_Y = Camera.Y - MAP_CHUNK_DIM*2;
   int Max_Y = Camera.Y + MAP_CHUNK_DIM*2;

   int Border_Pixels = Maximum(1, Tile_Pixels / 16);

#if 0
   for(int Y = Min_Y; Y < Max_Y; ++Y)
   {
      for(int X = Min_X; X < Max_X; ++X)
      {
         int Tile = Get_Map_Position_Value(Map, X, Y, Camera.Z);
         int Pixel_X = Tile_Pixels*(X - Camera.X) + Backbuffer.Width/2;
         int Pixel_Y = Tile_Pixels*(Y - Camera.Y) + Backbuffer.Height/2;

         if(Tile == 3)
         {
            game_texture Bitmap = (Camera.Z == 0) ? Game_State->Upstairs : Game_State->Downstairs;
            Draw_Bitmap(Backbuffer, Bitmap, Pixel_X, Pixel_Y);
         }
         else
         {
            Draw_Rectangle(Backbuffer, Pixel_X, Pixel_Y, Tile_Pixels, Tile_Pixels, Palette[Tile]);
            Draw_Outline(Backbuffer, Pixel_X, Pixel_Y, Tile_Pixels, Tile_Pixels, 1, Palette[1]);
         }
      }
   }
#endif

   for(int Chunk_Y = -1; Chunk_Y <= 1; ++Chunk_Y)
   {
      for(int Chunk_X = -1; Chunk_X <= 1; ++Chunk_X)
      {
         map_chunk *Chunk = Query_Map_Chunk_By_Chunk(Map, Chunk_X, Chunk_Y, Camera.Z);
         if(Chunk)
         {
            for(int Index = 0; Index < Chunk->Entity_Count; ++Index)
            {
               int Entity_Index = Chunk->Entity_Indices[Index];

               entity *Entity = Game_State->Entities + Entity_Index;
               if(Entity->Active && Entity->Position.Z == Camera.Z)
               {
                  int Pixel_Width  = Entity->Width * Tile_Pixels;
                  int Pixel_Height = Entity->Height * Tile_Pixels;
#if 1
                  int Offset_X = Tile_Pixels * Entity->Animation.Offset_X;
                  int Offset_Y = Tile_Pixels * Entity->Animation.Offset_Y;
#else
                  int Offset_X = 0;
                  int Offset_Y = 0;
#endif

                  int Pixel_X = Tile_Pixels * (Entity->Position.X - Camera.X) + Backbuffer.Width/2 - Offset_X;
                  int Pixel_Y = Tile_Pixels * (Entity->Position.Y - Camera.Y) + Backbuffer.Height/2 - Offset_Y;

                  int Nose_Dim = 4 * Border_Pixels;
                  int Nose_X = Pixel_X + Pixel_Width/2  - Nose_Dim/2;
                  int Nose_Y = Pixel_Y + Pixel_Height/2 - Nose_Dim/2;
                  switch(Entity->Animation.Direction)
                  {
                     case Direction_Up:    { Nose_Y -= (Pixel_Height/2 - Nose_Dim/2); } break;
                     case Direction_Down:  { Nose_Y += (Pixel_Height/2 - Nose_Dim/2); } break;
                     case Direction_Left:  { Nose_X -= (Pixel_Width/2  - Nose_Dim/2); } break;
                     case Direction_Right: { Nose_X += (Pixel_Width/2  - Nose_Dim/2); } break;
                     default: {} break;
                  }

                  switch(Entity->Type)
                  {
                     case Entity_Type_Player: {
                        Draw_Rectangle(Backbuffer, Pixel_X, Pixel_Y, Pixel_Width, Pixel_Height, 0x000088FF);
                        Draw_Outline(Backbuffer, Pixel_X, Pixel_Y, Pixel_Width, Pixel_Height, 4*Border_Pixels, 0xFFFFFFFF);

                        Draw_Rectangle(Backbuffer, Nose_X, Nose_Y, Nose_Dim, Nose_Dim, 0x0000FFFF);
                     } break;

                     case Entity_Type_Dragon: {
                        Draw_Rectangle(Backbuffer, Pixel_X, Pixel_Y, Pixel_Width, Pixel_Height, 0x880000FF);
                        Draw_Outline(Backbuffer, Pixel_X, Pixel_Y, Pixel_Width, Pixel_Height, 4*Border_Pixels, 0xFF0000FF);

                        Draw_Rectangle(Backbuffer, Nose_X, Nose_Y, Nose_Dim, Nose_Dim, 0xFFFF00FF);
                     } break;

                     case Entity_Type_Wall: {
                        // int Tile = Get_Map_Position_Value(Map, Entity->Position.X, Entity->Position.Y, Entity->Position.Z);
                        // if(Tile == 3)
                        // {
                        //    game_texture Bitmap = (Entity->Position.Z == 0) ? Game_State->Upstairs : Game_State->Downstairs;
                        //    Draw_Bitmap(Backbuffer, Bitmap, Pixel_X, Pixel_Y);
                        // }
                        // else
                        // {
                           Draw_Rectangle(Backbuffer, Pixel_X, Pixel_Y, Tile_Pixels, Tile_Pixels, Palette[2]);
                           Draw_Outline(Backbuffer, Pixel_X, Pixel_Y, Tile_Pixels, Tile_Pixels, 1, Palette[1]);
                        // }
                     } break;

                     default: {
                     } break;
                  }
               }
            }
         }
      }
   }

   // User Interface.
   int Text_X = Tile_Pixels/2;
   int Text_Y = 0;
   text_size Text_Size = Text_Size_Large;
   text_font *Font = &Game_State->Varia_Font;

   Advance_Text_Line(Font, Text_Size, &Text_Y);
   Draw_Text(Backbuffer, Font, Text_Size, Text_X, Text_Y, S("Dungeon Simulator"));

   char Data[128];
   string Text = {0};
   Text.Data = (u8 *)Data;
   Text_Size = Text_Size_Medium;
   Font = &Game_State->Fixed_Font;

   Advance_Text_Line(Font, Text_Size, &Text_Y);
   Text.Length = snprintf(Data, sizeof(Data), "Frame Time: %3.3fms", Frame_Seconds*1000.0f);
   Draw_Text(Backbuffer, Font, Text_Size, Text_X, Text_Y, Text);

   Advance_Text_Line(Font, Text_Size, &Text_Y);
   Text.Length = snprintf(Data, sizeof(Data), "Camera: {%d, %d, %d}", Camera.X, Camera.Y, Camera.Z);
   Draw_Text(Backbuffer, Font, Text_Size, Text_X, Text_Y, Text);

   for(int Entity_Index = 0; Entity_Index < Game_State->Entity_Count; ++Entity_Index)
   {
      entity *Entity = Game_State->Entities + Entity_Index;
      if(Entity->Active)
      {
         switch(Entity->Type)
         {
            case Entity_Type_Camera:
            case Entity_Type_Player:
            case Entity_Type_Dragon:
            {
               Advance_Text_Line(Font, Text_Size, &Text_Y);
               Text.Length = snprintf(Data, sizeof(Data), "%.*s: {%d, %d, %d}",
                                      (int)Entity->Name.Length,
                                      Entity->Name.Data,
                                      Entity->Position.X,
                                      Entity->Position.Y,
                                      Entity->Position.Z);

               Draw_Text(Backbuffer, Font, Text_Size, Text_X, Text_Y, Text);
            } break;

            default: {} break;
         }
      }
   }

   if(Game_State->Active_Textbox_Index)
   {
      Draw_Textbox(Game_State, Backbuffer, Game_State->Textbox_Dialogue[Game_State->Active_Textbox_Index]);
   }

   int Gui_Dim = Tile_Pixels;
   int Gui_X = Backbuffer.Width - 2*Gui_Dim*GAME_CONTROLLER_COUNT;
   int Gui_Y = Gui_Dim;

   for(int Controller_Index = 0; Controller_Index < GAME_CONTROLLER_COUNT; ++Controller_Index)
   {
      game_controller *Controller = Input->Controllers + Controller_Index;

      if(Controller->Connected)
      {
         Draw_Rectangle(Backbuffer, Gui_X, Gui_Y, Gui_Dim, Gui_Dim, 0x00FF00FF);
      }
      else
      {
         Draw_Rectangle(Backbuffer, Gui_X, Gui_Y, Gui_Dim, Gui_Dim, 0x004400FF);
         Draw_Outline(Backbuffer, Gui_X, Gui_Y, Gui_Dim, Gui_Dim, 2*Border_Pixels, 0x00FF00FF);
      }
      Gui_X += (2 * Gui_Dim);
   }
}
