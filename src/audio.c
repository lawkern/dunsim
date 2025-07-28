/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

static void Play_Sound(game_state *Game_State, audio_sound *Sound, audio_playback Playback)
{
   audio_track *Track;
   if(Game_State->Free_Audio_Tracks)
   {
      Track = Game_State->Free_Audio_Tracks;
      Game_State->Free_Audio_Tracks = Game_State->Free_Audio_Tracks->Next;
   }
   else
   {
      Track = Allocate(&Game_State->Permanent, audio_track, 1);
   }

   Track->Sound = Sound;
   Track->Next = Game_State->Audio_Tracks;
   Track->Sample_Index = 0;
   Track->Playback = Playback;

   Game_State->Audio_Tracks = Track;
}
