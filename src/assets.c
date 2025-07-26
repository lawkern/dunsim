/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef enum {
   Text_Size_Small,
   Text_Size_Medium,
   Text_Size_Large,

   Text_Size_Count,
} text_size;

#define GLYPH_COUNT 128
typedef struct {
   float Scale;
   game_texture Bitmaps[GLYPH_COUNT];
} text_glyphs;

typedef struct {
   float Ascent;
   float Descent;
   float Line_Gap;

   text_glyphs Glyphs[Text_Size_Count];
   float *Distances;

   bool Loaded;
} text_font;

static void Load_Font(text_font *Result, arena *Arena, arena Scratch, char *Path, int Pixel_Height)
{
   string Font = Read_Entire_File(&Scratch, Path);
   if(Font.Length)
   {
      stbtt_fontinfo Info;
      stbtt_InitFont(&Info, Font.Data, stbtt_GetFontOffsetForIndex(Font.Data, 0));

      int Ascent, Descent, Line_Gap;
      stbtt_GetFontVMetrics(&Info, &Ascent, &Descent, &Line_Gap);

      Result->Ascent = Ascent;
      Result->Descent = Descent;
      Result->Line_Gap = Line_Gap;

      for(int Size_Index = 0; Size_Index < Text_Size_Count; ++Size_Index)
      {
         text_glyphs *Glyphs = Result->Glyphs + Size_Index;
         Glyphs->Scale = stbtt_ScaleForPixelHeight(&Info, Pixel_Height);
         Pixel_Height += 8;

         for(int Codepoint = ' '; Codepoint <= '~'; ++Codepoint)
         {
            int Width, Height, Offset_X, Offset_Y;
            u8 *Bitmap = stbtt_GetCodepointBitmap(&Info, 0, Glyphs->Scale, Codepoint, &Width, &Height, &Offset_X, &Offset_Y);

            game_texture *Glyph = Glyphs->Bitmaps + Codepoint;
            Glyph->Width    = Width;
            Glyph->Height   = Height;
            Glyph->Offset_X = Offset_X;
            Glyph->Offset_Y = Offset_Y;

            size Pixel_Count = Width * Height;
            Glyph->Memory = Allocate(Arena, u32, Pixel_Count);
            for(int Index = 0; Index < Pixel_Count; ++Index)
            {
               u8 Value = Bitmap[Index];
               Glyph->Memory[Index] = (Value << 0) | (Value << 8) | (Value << 16) | (Value << 24);
            }
            stbtt_FreeBitmap(Bitmap, 0);
         }
      }

      int Distance_Count = GLYPH_COUNT * GLYPH_COUNT;
      Result->Distances = Allocate(Arena, float, Distance_Count);

      for(int C0 = ' '; C0 <= '~'; ++C0)
      {
         int Advance_Width, Left_Side_Bearing;
         stbtt_GetCodepointHMetrics(&Info, C0, &Advance_Width, &Left_Side_Bearing);

         for(int C1 = ' '; C1 <= '~'; ++C1)
         {
            int Kerning_Distance = stbtt_GetCodepointKernAdvance(&Info, C0, C1);
            Result->Distances[C0 * GLYPH_COUNT + C1] = (float)(Advance_Width + Kerning_Distance);
         }
      }

      Result->Loaded = true;
   }
}

static game_texture Load_Image(arena *Arena, char *Path)
{
   game_texture Result = {0};
   int Width, Height, Bytes_Per_Pixel;

   u8 *Data = stbi_load(Path, &Width, &Height, &Bytes_Per_Pixel, 0);
   if(Data)
   {
      Assert(Bytes_Per_Pixel == 4);

      Result.Memory = Allocate(Arena, u32, Width*Height);
      Result.Width = Width;
      Result.Height = Height;

      u32 *Source_Pixels = (u32 *)Data;
      for(int Index = 0; Index < Width*Height; ++Index)
      {
         u32 Source_Pixel = *Source_Pixels++;

         u32 A = (Source_Pixel >> 24) & 0xFF;
         u32 B = (Source_Pixel >> 16) & 0xFF;
         u32 G = (Source_Pixel >>  8) & 0xFF;
         u32 R = (Source_Pixel >>  0) & 0xFF;

         Result.Memory[Index] = (R << 24) | (G << 16) | (B << 8) | A;
      }

      stbi_image_free(Data);
   }

   return(Result);
}

#pragma pack(push, 1)
typedef struct {
   u32 Chunk_ID;
   u32 Chunk_Size;
} wave_header;

typedef struct {
   wave_header Header;
   u32 Wave_ID;
} wave_riff_chunk;

typedef struct {
   wave_header Header;
   u16 Format;
   u16 Channel_Count;
   u32 Samples_Per_Second;
   u32 Average_Bytes_Per_Second;
   u16 Block_Align;
   u16 Bits_Per_Sample;
   u16 Extension_Size;
   u16 Valid_Bits_Per_Sample;
} wave_format_chunk;

typedef struct {
   wave_header Header;
   s16 Data[];
} wave_data_chunk;
#pragma pack(pop)

#define WAVE_FORMAT_PCM 0x0001

static game_sound Load_Wave(arena *Arena, arena Scratch, char *Path)
{
   game_sound Result = {0};

   string File = Read_Entire_File(&Scratch, Path);
   Assert(File.Length);

   u8 *At = File.Data;
   u8 *End = File.Data + File.Length;

   while(At < End)
   {
      wave_header *Header = (wave_header *)At;
      switch(Header->Chunk_ID)
      {
         // TODO: I don't think multi-character literals are supported on
         // MSVC. Maybe we'll care about that some day.

         case 'FFIR': { // RIFF
            wave_riff_chunk *Chunk = (wave_riff_chunk *)At;
            assert(Chunk->Wave_ID == 'EVAW'); // WAVE

            At += sizeof(*Chunk);
         } break;

         case ' tmf': { // fmt
            wave_format_chunk *Chunk = (wave_format_chunk *)At;

            Assert(Chunk->Format == WAVE_FORMAT_PCM);
            Assert(Chunk->Samples_Per_Second == GAME_AUDIO_FREQUENCY);
            Assert(Chunk->Channel_Count == GAME_AUDIO_CHANNEL_COUNT);
            Assert((Chunk->Block_Align / Chunk->Channel_Count) == sizeof(*Result.Samples[0]));

            At += sizeof(*Header) + Header->Chunk_Size;
         } break;

         case 'atad': { // data
            wave_data_chunk *Chunk = (wave_data_chunk *)At;
            Header->Chunk_Size = (Header->Chunk_Size + 1) & ~1;

            Result.Sample_Count = Header->Chunk_Size / (GAME_AUDIO_CHANNEL_COUNT * sizeof(*Result.Samples[0]));

            s16 *Source = Chunk->Data;
            s16 *Destination = Allocate(Arena, s16, Result.Sample_Count*GAME_AUDIO_CHANNEL_COUNT);

            for(int Channel_Index = 0; Channel_Index < GAME_AUDIO_CHANNEL_COUNT; ++Channel_Index)
            {
               Result.Samples[Channel_Index] = Destination + Channel_Index*Result.Sample_Count;
            }

            for(int Sample_Index = 0; Sample_Index < Result.Sample_Count; ++Sample_Index)
            {
               for(int Channel_Index = 0; Channel_Index < GAME_AUDIO_CHANNEL_COUNT; ++Channel_Index)
               {
                  Result.Samples[Channel_Index][Sample_Index] = *Source++;
               }
            }

            At += sizeof(*Header) + Header->Chunk_Size;
         } break;

         default: {
            At += sizeof(*Header) + Header->Chunk_Size;
         } break;
      }
   }

   return(Result);
}
