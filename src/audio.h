/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

#define AUDIO_CHANNEL_COUNT 2
#define AUDIO_FREQUENCY 48000

typedef struct {
   int Sample_Count;
   s16 *Samples[AUDIO_CHANNEL_COUNT];
} audio_sound;

typedef enum {
   Audio_Playback_Once,
   Audio_Playback_Loop,
} audio_playback;

typedef struct audio_track audio_track;
struct audio_track
{
   audio_sound *Sound;
   audio_track *Next;

   int Sample_Index;
   audio_playback Playback;
   float Volume[AUDIO_CHANNEL_COUNT];
};
