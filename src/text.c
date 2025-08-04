/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

static text_context Begin_Text(renderer *Renderer, float X, float Y, text_font *Font, text_size Size)
{
   text_context Result = {0};
   Result.Renderer = Renderer;
   Result.X = X;
   Result.Y = Y;
   Result.Font = Font;
   Result.Size = Size;

   return(Result);
}

static void Advance_Text_Line(text_font *Font, text_size Size, float Meters_Per_Pixel, float *Y)
{
   float Scale = Font->Glyphs[Size].Pixel_Scale * Meters_Per_Pixel;

   float Line_Advance = Scale * (Font->Ascent - Font->Descent + Font->Line_Gap);
   *Y += Line_Advance;
}

static float Get_Text_Width_Pixels(text_font *Font, text_size Size, string Text)
{
   float Result = 0;

   float Scale = Font->Glyphs[Size].Pixel_Scale;
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
