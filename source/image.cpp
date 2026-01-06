// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

static u8
u8Round(f32 Val) 
{
    return (u8)(Val+0.5f);
}

static u32
PackRGBA(u8 R, u8 G, u8 B, u8 A) 
{
    u32 Result = (R | (G<<8) | (B<<16) | (A<<24));
    return Result;
}

static u8x4
UnpackRGBA(void *Pixel) 
{
    u32 Color = *(u32 *)Pixel;
    u8x4 Result = {};
    Result.R = (u8)(Color & 0xff);
    Result.G = (u8)((Color & 0xff00) >> 8);
    Result.B = (u8)((Color & 0xff0000) >> 16);
    Result.A = (u8)((Color & 0xff000000) >> 24);
    return Result;
}

static u8 *
PixelFromBitmap(bitmap *Bitmap, int X, int Y) 
{
    u8 *Result = (u8 *)Bitmap->Data + Y*Bitmap->Pitch + X*Bitmap->BytesPerPixel;
    return Result;
}

// ===============================
// Grayscale Conversion
//
static void
GrayscaleConversionScalar(bitmap *Dst, bitmap *Src, int X, int Y)
{
    u8x4 Pixel = UnpackRGBA((u32 *)PixelFromBitmap(Src, X, Y));
    f32 R = (f32)Pixel.R;
    f32 G = (f32)Pixel.G;
    f32 B = (f32)Pixel.B;

    u8 Gray = u8Round(R*0.299f + G*0.587f + B*0.114f);

    u32 *Out = (u32 *)PixelFromBitmap(Dst, X, Y);
    *Out = PackRGBA(Gray, Gray, Gray, 0xff);
}

static void
GrayscaleConversionWork(work_param *Param)
{
    bitmap *Dst     = Param->Dst;
    bitmap *Src     = Param->Src;
    int XMin        = Param->XMin;
    int YMin        = Param->YMin;
    int XMaxPastOne = Param->XMaxPastOne;
    int YMaxPastOne = Param->YMaxPastOne;

    // @Fix: SSE/Scalar produces different result.
    if (0)
    {
        for (int Y = YMin; Y < YMaxPastOne; ++Y) 
        {
            const int LaneWidth = 4;
            int X = XMin;
            for (;X < XMaxPastOne - LaneWidth; X += LaneWidth) 
            {
                __m128i *Addr = (__m128i *)PixelFromBitmap(Src, X, Y);
                __m128i Pixelx4 = _mm_loadu_si128(Addr);

                __m128 Pixel1 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(Pixelx4));
                __m128 Pixel2 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_shuffle_epi32(Pixelx4, 0x01)));
                __m128 Pixel3 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_shuffle_epi32(Pixelx4, 0x02)));
                __m128 Pixel4 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_shuffle_epi32(Pixelx4, 0x03)));

                __m128 const Weight = _mm_setr_ps(0.299f, 0.587f, 0.114f, 0.f);

                __m128 Mul1 = _mm_mul_ps(Pixel1, Weight);
                __m128 Mul2 = _mm_mul_ps(Pixel2, Weight);
                __m128 Mul3 = _mm_mul_ps(Pixel3, Weight);
                __m128 Mul4 = _mm_mul_ps(Pixel4, Weight);

                __m128 Sum1 = _mm_hadd_ps(Mul1, Mul2);
                __m128 Sum2 = _mm_hadd_ps(Mul3, Mul4);
                __m128i Sums = _mm_cvtps_epi32(_mm_add_ps(_mm_hadd_ps(Sum1, Sum2), _mm_set1_ps(0.5f)));

                __m128i Pack = _mm_or_si128(_mm_or_si128(_mm_or_si128(Sums, _mm_slli_epi32(Sums, 8)), _mm_slli_epi32(Sums, 16)), _mm_slli_epi32(Sums, 24));
                __m128i AlphaMask = _mm_set1_epi32(0xff000000);
                __m128i Result = _mm_or_si128(AlphaMask, Pack);

                __m128i *Out = (__m128i *)PixelFromBitmap(Dst, X, Y);
                _mm_storeu_si128(Out, Result);
            }

            // Scalar cleanup for the remainder.
            //
            for (;X < XMaxPastOne; ++X)
            {
                GrayscaleConversionScalar(Dst, Src, X, Y);
            }
        }
    }
    else
    {
        for (int Y = YMin; Y < YMaxPastOne; ++Y) 
        {
            for (int X = XMin; X < XMaxPastOne; ++X) 
            {
                GrayscaleConversionScalar(Dst, Src, X, Y);
            }
        }
    }
}

// ===============================
// Gaussian Blur
// @Todo: Really, we don't have to compute on full RBGBA once we've done grayscale conversion.
//
static void 
ConvolutionScalar(s32x4 *RGBA, bitmap *Src, int X, int Y, int dX, int dY)
{
    int Row = dY + KERNEL_RADIUS;
    int Col = dX + KERNEL_RADIUS;
    s32 Weight = Kernel[Row][Col];

    int SampleX = X + dX;
    int SampleY = Y + dY;

    u8x4 Sample = UnpackRGBA((u32 *)PixelFromBitmap(Src, SampleX, SampleY));

    RGBA->E[0] += Weight*(s32)Sample.R;
    RGBA->E[1] += Weight*(s32)Sample.G;
    RGBA->E[2] += Weight*(s32)Sample.B;
    RGBA->E[3] += Weight*(s32)Sample.A;
}

static void 
ConvolutionSIMD(s32x4 *RGBA, bitmap *Src, int X, int Y, int dX, int dY)
{
    int SampleX = X + dX;
    int SampleY = Y + dY;
    __m128i Samples = _mm_loadu_si128((__m128i *)PixelFromBitmap(Src, SampleX, SampleY));

    int Row = dY + KERNEL_RADIUS;
    int Col = dX + KERNEL_RADIUS;
    s32 *Weights = &Kernel[Row][Col];

    __m128i Zero = _mm_setzero_si128();
    __m128i Lo = _mm_unpacklo_epi8(Samples, Zero);
    __m128i Hi = _mm_unpackhi_epi8(Samples, Zero);

    __m128i P0 = _mm_unpacklo_epi16(Lo, Zero);
    __m128i P1 = _mm_unpackhi_epi16(Lo, Zero);
    __m128i P2 = _mm_unpacklo_epi16(Hi, Zero);
    __m128i P3 = _mm_unpackhi_epi16(Hi, Zero);

    __m128i W0 = _mm_set1_epi32(Weights[0]);
    __m128i W1 = _mm_set1_epi32(Weights[1]);
    __m128i W2 = _mm_set1_epi32(Weights[2]);
    __m128i W3 = _mm_set1_epi32(Weights[3]);

    __m128i PW0 = _mm_mullo_epi32(P0, W0);
    __m128i PW1 = _mm_mullo_epi32(P1, W1);
    __m128i PW2 = _mm_mullo_epi32(P2, W2);
    __m128i PW3 = _mm_mullo_epi32(P3, W3);

    __m128i Addend = _mm_add_epi32(_mm_add_epi32(PW0, PW1), _mm_add_epi32(PW2, PW3));

    RGBA->V = _mm_add_epi32(RGBA->V, Addend);
}

static void
ApplyKernelOnPixel(bitmap *Dst, bitmap *Src, int X, int Y)
{
    s32x4 RGBA = {};

    if (GlobalEnabledSSE)
    {
        const int LaneWidth = 4;
        for (int dY = -KERNEL_RADIUS; dY <= KERNEL_RADIUS; ++dY) 
        {
            int dX = -KERNEL_RADIUS;
            for (;dX <= KERNEL_RADIUS - LaneWidth; dX += LaneWidth) 
            {
                ConvolutionSIMD(&RGBA, Src, X, Y, dX, dY);
            }

            // Scalar Cleanup.
            for (;dX <= KERNEL_RADIUS; ++dX) 
            {
                ConvolutionScalar(&RGBA, Src, X, Y, dX, dY);
            }
        }
    }
    else
    {
        for (int dY = -KERNEL_RADIUS; dY <= KERNEL_RADIUS; ++dY) 
        {
            for (int dX = -KERNEL_RADIUS; dX <= KERNEL_RADIUS; ++dX) 
            {
                ConvolutionScalar(&RGBA, Src, X, Y, dX, dY);
            }
        }
    }

    u8 r = (u8)((RGBA.E[0] + 0x8000) >> 16);
    u8 g = (u8)((RGBA.E[1] + 0x8000) >> 16);
    u8 b = (u8)((RGBA.E[2] + 0x8000) >> 16);
    u8 a = (u8)((RGBA.E[3] + 0x8000) >> 16);

    u32 *Out = (u32 *)PixelFromBitmap(Dst, X, Y);
    *Out = PackRGBA(r, g, b, a);
}

static void
GaussianBlurWork(work_param *Param) 
{
    bitmap *Dst     = Param->Dst;
    bitmap *Src     = Param->Src;
    int XMin        = Param->XMin;
    int YMin        = Param->YMin;
    int XMaxPastOne = Param->XMaxPastOne;
    int YMaxPastOne = Param->YMaxPastOne;

    for (int Y = YMin; Y < YMaxPastOne; ++Y) 
    {
        for (int X = XMin; X < XMaxPastOne; ++X) 
        {
            ApplyKernelOnPixel(Dst, Src, X, Y);
        }
    }
}

// ===============================
// Pixel Subtraction
//
static void
DifferenceOfGaussianScalar(bitmap *Dst, bitmap *Bitmap1, bitmap *Bitmap2, int X, int Y) 
{
    u8 Intensity1 = *((u8 *)PixelFromBitmap(Bitmap1, X, Y));
    u8 Intensity2 = *((u8 *)PixelFromBitmap(Bitmap2, X, Y));
    u8 Diff = (Intensity1 > Intensity2) ? (Intensity1 - Intensity2) : (Intensity2 - Intensity1);

    u8 Value = (Diff < GlobalThreshold) ? 0x00 : 0xff;

    u32 *Out = (u32 *)PixelFromBitmap(Dst, X, Y);
    *Out = PackRGBA(Value, Value, Value, 0xff);
}

static void
DifferenceOfGaussianWork(work_param *Param) 
{
    bitmap *Dst     = Param->Dst;
    bitmap *Bitmap1 = Param->Data1;
    bitmap *Bitmap2 = Param->Data2;
    int XMin        = Param->XMin;
    int YMin        = Param->YMin;
    int XMaxPastOne = Param->XMaxPastOne;
    int YMaxPastOne = Param->YMaxPastOne;

    if (GlobalEnabledSSE) 
    {
        const int LaneWidth = 4;
        for (int Y = YMin; Y < YMaxPastOne; ++Y) 
        {
            int X = XMin;
            for (;X < XMaxPastOne - LaneWidth; X += LaneWidth) 
            {
                __m128i *Addr1 = (__m128i *)PixelFromBitmap(Bitmap1, X, Y);
                __m128i *Addr2 = (__m128i *)PixelFromBitmap(Bitmap2, X, Y);

                __m128i Lane1 = _mm_loadu_si128(Addr1);
                __m128i Lane2 = _mm_loadu_si128(Addr2);

                __m128i SubResult = _mm_abs_epi8(_mm_sub_epi8(Lane1, Lane2));
                __m128i Threshold = _mm_set1_epi8(GlobalThreshold);
                __m128i CmpResult = _mm_cmpgt_epi8(SubResult, _mm_sub_epi8(Threshold, _mm_set1_epi8(1)));

                __m128i AlphaMask = _mm_set1_epi32(0xff000000);
                __m128i Result = _mm_or_si128(AlphaMask, CmpResult);

                __m128i *Out = (__m128i *)PixelFromBitmap(Dst, X, Y);
                _mm_storeu_si128(Out, Result);
            }

            // Scalar cleanup for the remainder.
            //
            for (;X < XMaxPastOne; ++X)
            {
                DifferenceOfGaussianScalar(Dst, Bitmap1, Bitmap2, X, Y);
            }
        }
    }
    else
    {
        for (int Y = YMin; Y < YMaxPastOne; ++Y) 
        {
            for (int X = XMin; X < XMaxPastOne; ++X) 
            {
                DifferenceOfGaussianScalar(Dst, Bitmap1, Bitmap2, X, Y);
            }
        }
    }
}
