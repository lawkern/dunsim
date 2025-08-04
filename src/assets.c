/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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
         Glyphs->Pixel_Scale = stbtt_ScaleForPixelHeight(&Info, Pixel_Height);
         Pixel_Height += 8;

         for(int Codepoint = ' '; Codepoint <= '~'; ++Codepoint)
         {
            int Width, Height, Offset_X, Offset_Y;
            u8 *Bitmap = stbtt_GetCodepointBitmap(&Info, 0, Glyphs->Pixel_Scale, Codepoint, &Width, &Height, &Offset_X, &Offset_Y);

            texture *Glyph = Glyphs->Bitmaps + Codepoint;
            Glyph->Width    = Width + 2;
            Glyph->Height   = Height + 2;
            Glyph->Offset_X = Offset_X + 1;
            Glyph->Offset_Y = Offset_Y + 1;

            size Pixel_Count = Glyph->Width * Glyph->Height;
            Glyph->Memory = Allocate(Arena, u32, Pixel_Count);

            for(int Source_Y = 0; Source_Y < Height; ++Source_Y)
            {
               int Destination_Y = Source_Y + 1;
               for(int Source_X = 0; Source_X < Width; ++Source_X)
               {
                  int Destination_X = Source_X + 1;

                  int Source_Index = (Width * Source_Y) + Source_X;
                  int Destination_Index = (Glyph->Width * Destination_Y) + Destination_X;

                  u8 Value = Bitmap[Source_Index];
                  Glyph->Memory[Destination_Index] = (Value << 0) | (Value << 8) | (Value << 16) | (Value << 24);
               }
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

static texture Load_Image(arena *Arena, char *Path)
{
   texture Result = {0};
   int Width, Height, Bytes_Per_Pixel;

   u8 *Data = stbi_load(Path, &Width, &Height, &Bytes_Per_Pixel, 0);
   if(Data)
   {
      Assert(Bytes_Per_Pixel == 4);

      Result.Memory = Allocate(Arena, u32, Width*Height);
      Result.Width = Width;
      Result.Height = Height;
      Result.Offset_X = -1;
      Result.Offset_Y = -1;

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

static audio_sound Load_Wave(arena *Arena, arena Scratch, char *Path)
{
   audio_sound Result = {0};

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
            Assert(Chunk->Samples_Per_Second == AUDIO_FREQUENCY);
            Assert(Chunk->Channel_Count == AUDIO_CHANNEL_COUNT);
            Assert((Chunk->Block_Align / Chunk->Channel_Count) == sizeof(*Result.Samples[0]));

            At += sizeof(*Header) + Header->Chunk_Size;
         } break;

         case 'atad': { // data
            wave_data_chunk *Chunk = (wave_data_chunk *)At;
            Header->Chunk_Size = (Header->Chunk_Size + 1) & ~1;

            Result.Sample_Count = Header->Chunk_Size / (AUDIO_CHANNEL_COUNT * sizeof(*Result.Samples[0]));

            s16 *Source = Chunk->Data;
            s16 *Destination = Allocate(Arena, s16, Result.Sample_Count*AUDIO_CHANNEL_COUNT);

            for(int Channel_Index = 0; Channel_Index < AUDIO_CHANNEL_COUNT; ++Channel_Index)
            {
               Result.Samples[Channel_Index] = Destination + Channel_Index*Result.Sample_Count;
            }

            for(int Sample_Index = 0; Sample_Index < Result.Sample_Count; ++Sample_Index)
            {
               for(int Channel_Index = 0; Channel_Index < AUDIO_CHANNEL_COUNT; ++Channel_Index)
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
