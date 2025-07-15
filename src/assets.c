/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef struct {
   float Scale;
   float Ascent;
   float Descent;
   float Line_Gap;

   game_texture Glyphs[128];
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

      float Scale = stbtt_ScaleForPixelHeight(&Info, Pixel_Height);

      Result->Scale = Scale;
      Result->Ascent = Ascent;
      Result->Descent = Descent;
      Result->Line_Gap = Line_Gap;

      for(int Codepoint = '!'; Codepoint <= '~'; ++Codepoint)
      {
         game_texture *Glyph = Result->Glyphs + Codepoint;
         u8 *Bitmap = stbtt_GetCodepointBitmap(&Info, 0, Scale, Codepoint, &Glyph->Width, &Glyph->Height, &Glyph->Offset_X, &Glyph->Offset_Y);

         size Pixel_Count = Glyph->Width * Glyph->Height;
         Glyph->Memory = Allocate(Arena, u32, Pixel_Count);
         for(int Index = 0; Index < Pixel_Count; ++Index)
         {
            u8 Value = Bitmap[Index];
            Glyph->Memory[Index] = (Value << 0) | (Value << 8) | (Value << 16) | (Value << 24);
         }
         stbtt_FreeBitmap(Bitmap, 0);
      }

      int Codepoint_Count = Array_Count(Result->Glyphs);
      int Distance_Count = Codepoint_Count * Codepoint_Count;
      Result->Distances = Allocate(Arena, float, Distance_Count);

      for(int C0 = 0; C0 < Codepoint_Count; ++C0)
      {
         int Advance_Width, Left_Side_Bearing;
         stbtt_GetCodepointHMetrics(&Info, C0, &Advance_Width, &Left_Side_Bearing);

         for(int C1 = 0; C1 < Codepoint_Count; ++C1)
         {
            int Kerning_Distance = stbtt_GetCodepointKernAdvance(&Info, C0, C1);
            Result->Distances[C0 * Codepoint_Count + C1] = Scale * (float)(Advance_Width + Kerning_Distance);
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
      if(Result.Memory)
      {
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
      }

      stbi_image_free(Data);
   }

   return(Result);
}
