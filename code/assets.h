/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

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
