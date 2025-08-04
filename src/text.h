/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

// NOTE: Currently we are using world units (meters) for text positioning so
// that text scales with screen size. We may decouple this in the future.

typedef enum {
   Text_Size_Small,
   Text_Size_Medium,
   Text_Size_Large,

   Text_Size_Count,
} text_size;

#define GLYPH_COUNT 128
typedef struct {
   float Pixel_Scale;
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

typedef struct {
   renderer *Renderer;
   float X;
   float Y;

   text_font *Font;
   text_size Size;
} text_context;
