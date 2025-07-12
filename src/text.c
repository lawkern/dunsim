/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

typedef struct {
   float Scale;
   float Ascent;
   float Descent;
   float Line_Gap;

   game_texture Glyphs[128];
   float *Distances;
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
   }
}
