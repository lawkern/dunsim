/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

typedef enum {
   Text_Size_Small,
   Text_Size_Medium,
   Text_Size_Large,

   Text_Size_Count,
} text_size;

#define GLYPH_COUNT 128
typedef struct {
   float Scale;
   texture Bitmaps[GLYPH_COUNT];
} text_glyphs;

typedef struct {
   float Ascent;
   float Descent;
   float Line_Gap;

   text_glyphs Glyphs[Text_Size_Count];
   float *Distances;

   bool Loaded;
} text_font;
