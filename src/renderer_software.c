/* (c) copyright 2025 Lawrence D. Kern /////////////////////////////////////// */

static inline u32 Pack_Color(vec4 Color)
{
   u32 R = (u32)(255.0f*Color.R + 0.5f) << 24;
   u32 G = (u32)(255.0f*Color.G + 0.5f) << 16;
   u32 B = (u32)(255.0f*Color.B + 0.5f) << 8;
   u32 A = (u32)(255.0f*Color.A + 0.5f) << 0;

   u32 Result = (R | G | B | A);
   return(Result);
}

static inline vec4 Unpack_Color(u32 Color)
{
   float R = (float)((Color >> 24) & 0xFF);
   float G = (float)((Color >> 16) & 0xFF);
   float B = (float)((Color >>  8) & 0xFF);
   float A = (float)((Color >>  0) & 0xFF);

   vec4 Result = {R, G, B, A};
   return(Result);
}

static DRAW_CLEAR(Draw_Clear)
{
   BEGIN_PROFILE(Draw_Clear);

   u32 Pixel = Pack_Color(Color);

   for(int Y = 0; Y < Destination.Height; ++Y)
   {
      for(int X = 0; X < Destination.Width; ++X)
      {
         Destination.Memory[(Destination.Width * Y) + X] = Pixel;
      }
   }

   END_PROFILE(Draw_Clear);
}

static DRAW_RECTANGLE(Draw_Rectangle)
{
   BEGIN_PROFILE(Draw_Rectangle);

   // TODO: Subpixel precision?
   int Min_X = (int)(Maximum(X, 0.0f) + 0.5f);
   int Min_Y = (int)(Maximum(Y, 0.0f) + 0.5f);
   int Max_X = (int)(Minimum((float)Destination.Width, X + Width) + 0.5f);
   int Max_Y = (int)(Minimum((float)Destination.Height, Y + Height) + 0.5f);

   u32 Pixel = Pack_Color(Color);

   for(int Y = Min_Y; Y < Max_Y; ++Y)
   {
      for(int X = Min_X; X < Max_X; ++X)
      {
         Destination.Memory[(Destination.Width * Y) + X] = Pixel;
      }
   }

   END_PROFILE(Draw_Rectangle);
}

static DRAW_TEXTURE(Draw_Texture)
{
   BEGIN_PROFILE(Draw_Texture);

   X += Source.Offset_X;
   Y += Source.Offset_Y;

   // TODO: Subpixel precision.

   int Min_X = (int)(Maximum(X, 0.0f) + 0.5f);
   int Min_Y = (int)(Maximum(Y, 0.0f) + 0.5f);
   int Max_X = (int)(Minimum((float)Destination.Width, X + (float)Source.Width) + 0.5f);
   int Max_Y = (int)(Minimum((float)Destination.Height, Y + (float)Source.Height) + 0.5f);

   int Clip_X_Offset = Min_X - X;
   int Clip_Y_Offset = Min_Y - Y;

   u32 *Source_Row = Source.Memory + Clip_Y_Offset*Source.Width;
   for(int Destination_Y = Min_Y; Destination_Y < Max_Y; ++Destination_Y)
   {
      int X_Offset = Clip_X_Offset;
      u32 *Destination_Row = Destination.Memory + Destination.Width*Destination_Y;

      for(int Destination_X = Min_X; Destination_X < Max_X; ++Destination_X)
      {
         u32 Source_Pixel = Source_Row[X_Offset++];
         u32 *Destination_Pixel = Destination_Row + Destination_X;

         float SR = (float)((Source_Pixel >> 24) & 0xFF);
         float SG = (float)((Source_Pixel >> 16) & 0xFF);
         float SB = (float)((Source_Pixel >>  8) & 0xFF);
         float SA = (float)((Source_Pixel >>  0) & 0xFF) / 255.0f;

         float DR = (float)((*Destination_Pixel >> 24) & 0xFF);
         float DG = (float)((*Destination_Pixel >> 16) & 0xFF);
         float DB = (float)((*Destination_Pixel >>  8) & 0xFF);

         u32 R = (u32)((DR * (1.0f-SA) + SR) + 0.5f);
         u32 G = (u32)((DG * (1.0f-SA) + SG) + 0.5f);
         u32 B = (u32)((DB * (1.0f-SA) + SB) + 0.5f);

         *Destination_Pixel = (R<<24) | (G<<16) | (B<<8) | 0xFF;
      }

      Source_Row += Source.Width;
   }

   END_PROFILE(Draw_Texture);
}

static DRAW_TEXTURED_QUAD(Draw_Textured_Quad)
{
   BEGIN_PROFILE(Draw_Textured_Quad);

   // TODO: Stop storing Offsets in pixel space.
   // Origin.X += Source.Offset_X;
   // Origin.Y += Source.Offset_Y;

   int Width_Max = Destination.Width - 1;
   int Height_Max = Destination.Height - 1;

   int Min_X = Width_Max;
   int Min_Y = Height_Max;
   int Max_X = 0;
   int Max_Y = 0;

   vec2 Points[] = {Origin, Add2(Origin, X_Axis), Add2(Origin, Add2(X_Axis, Y_Axis)), Add2(Origin, Y_Axis)};
   for(int Point_Index = 0; Point_Index < Array_Count(Points); ++Point_Index)
   {
      vec2 Point = Points[Point_Index];

      int Floor_X = (int)Floor(Point.X);
      int Floor_Y = (int)Floor(Point.Y);
      int Ceiling_X = (int)Ceiling(Point.X);
      int Ceiling_Y = (int)Ceiling(Point.Y);

      if(Min_X > Floor_X) Min_X = Floor_X;
      if(Min_Y > Floor_Y) Min_Y = Floor_Y;
      if(Max_X < Ceiling_X) Max_X = Ceiling_X;
      if(Max_Y < Ceiling_Y) Max_Y = Ceiling_Y;
   }

   if(Min_X < 0) Min_X = 0;
   if(Min_Y < 0) Min_Y = 0;
   if(Max_X > Width_Max)  Max_X = Width_Max;
   if(Max_Y > Height_Max) Max_Y = Height_Max;

   float Inv_X_Axis_Sq = 1.0f / Length2_Squared(X_Axis);
   float Inv_Y_Axis_Sq = 1.0f / Length2_Squared(Y_Axis);

   for(int Y = Min_Y; Y <= Max_Y; ++Y)
   {
      for(int X = Min_X; X <= Max_X; ++X)
      {
         vec2 P = Sub2(Vec2(X, Y), Origin);
         float Edge_0 = Dot2(P, Perp2(X_Axis));
         float Edge_1 = Dot2(Sub2(P, X_Axis), Perp2(Y_Axis));
         float Edge_2 = Dot2(Sub2(P, Add2(X_Axis, Y_Axis)), Neg2(Perp2(X_Axis)));
         float Edge_3 = Dot2(Sub2(P, Y_Axis), Neg2(Perp2(Y_Axis)));

         if(Edge_0 > 0 && Edge_1 > 0 && Edge_2 > 0 && Edge_3 > 0)
         {
            float U = Dot2(P, X_Axis) * Inv_X_Axis_Sq;
            float V = Dot2(P, Y_Axis) * Inv_Y_Axis_Sq;

            float Texel_X_Fractional = Clamp_01(U) * (float)(Source.Width - 2);
            float Texel_Y_Fractional = Clamp_01(V) * (float)(Source.Height - 2);

            int Texel_X = (int)(Texel_X_Fractional);
            int Texel_Y = (int)(Texel_Y_Fractional);

            float TX = Texel_X_Fractional - (float)Texel_X;
            float TY = Texel_Y_Fractional - (float)Texel_Y;

            u32 Texel_A_Packed = Source.Memory[(Source.Width * Texel_Y) + Texel_X];
            u32 Texel_B_Packed = Source.Memory[(Source.Width * Texel_Y) + Texel_X + 1];
            u32 Texel_C_Packed = Source.Memory[(Source.Width * (Texel_Y + 1)) + Texel_X];
            u32 Texel_D_Packed = Source.Memory[(Source.Width * (Texel_Y + 1)) + Texel_X + 1];

            vec4 Texel_A = Unpack_Color(Texel_A_Packed);
            vec4 Texel_B = Unpack_Color(Texel_B_Packed);
            vec4 Texel_C = Unpack_Color(Texel_C_Packed);
            vec4 Texel_D = Unpack_Color(Texel_D_Packed);

            vec4 Texel_AB = Lerp4(Texel_A, Texel_B, TX);
            vec4 Texel_CD = Lerp4(Texel_C, Texel_D, TX);
            vec4 Texel = Lerp4(Texel_AB, Texel_CD, TY);

            float SR = Texel.R;
            float SG = Texel.G;
            float SB = Texel.B;
            float SA = Texel.A / 255.0f;

            u32 Destination_Pixel = Destination.Memory[(Destination.Width * Y) + X];

            float DR = (float)((Destination_Pixel >> 24) & 0xFF);
            float DG = (float)((Destination_Pixel >> 16) & 0xFF);
            float DB = (float)((Destination_Pixel >>  8) & 0xFF);
            float DA = (float)((Destination_Pixel >>  0) & 0xFF);

            u32 R = (u32)((DR * (1.0f-SA) + SR) + 0.5f);
            u32 G = (u32)((DG * (1.0f-SA) + SG) + 0.5f);
            u32 B = (u32)((DB * (1.0f-SA) + SB) + 0.5f);

            Destination.Memory[(Destination.Width * Y) + X] = (R<<24) | (G<<16) | (B<<8) | 0xFF;
         }
         else
         {
            Destination.Memory[(Destination.Width * Y) + X] = 0xFF00FFFF;
         }
      }
   }

   END_PROFILE(Draw_Textured_Quad);
}
