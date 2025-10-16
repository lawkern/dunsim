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
   for(int Channel_Index = 0; Channel_Index < AUDIO_CHANNEL_COUNT; ++Channel_Index)
   {
      Track->Volume[Channel_Index] = 0.5f;
   }

   Game_State->Audio_Tracks = Track;
}

MIX_AUDIO_OUTPUT(Mix_Audio_Output)
{
   game_state *Game_State = (game_state *)Memory.Base;

   // NOTE: Audio_Output should get its own arena if we decide to handle sound
   // mixing on a different thread.

   arena Arena = Game_State->Scratch;
   Audio_Output->Samples = Allocate(&Arena, s16, Audio_Output->Sample_Count*AUDIO_CHANNEL_COUNT);

   s16 *Destination = Audio_Output->Samples;
   for(int Sample_Index = 0; Sample_Index < Audio_Output->Sample_Count; ++Sample_Index)
   {
      for(int Channel_Index = 0; Channel_Index < AUDIO_CHANNEL_COUNT; ++Channel_Index)
      {
         *Destination++ = 0;
      }
   }

   audio_track **Track_Ptr = &Game_State->Audio_Tracks;
   while(*Track_Ptr)
   {
      audio_track *Track = *Track_Ptr;

      audio_sound *Sound = Track->Sound;
      s16 *Destination = Audio_Output->Samples;

      int Samples_Left = Sound->Sample_Count - Track->Sample_Index;
      int Sample_Count = Audio_Output->Sample_Count;
      if(Track->Playback == Audio_Playback_Once && Samples_Left < Sample_Count)
      {
         Sample_Count = Samples_Left;
      }

      for(int Sample_Index = 0; Sample_Index < Sample_Count; ++Sample_Index)
      {
         for(int Channel_Index = 0; Channel_Index < AUDIO_CHANNEL_COUNT; ++Channel_Index)
         {
            float Volume = Track->Volume[Channel_Index];
            float Sample = (float)Sound->Samples[Channel_Index][Track->Sample_Index];
            *Destination++ += (s16)(Volume * Sample);
         }

         Track->Sample_Index++;
         if(Track->Playback == Audio_Playback_Loop)
         {
            Track->Sample_Index %= Sound->Sample_Count;
         }
      }

      if(Track->Playback == Audio_Playback_Once && Track->Sample_Index == Sound->Sample_Count)
      {
         *Track_Ptr = Track->Next;
         Track->Next = Game_State->Free_Audio_Tracks;
         Game_State->Free_Audio_Tracks = Track;
      }
      else
      {
         Track_Ptr = &Track->Next;
      }
   }
}
