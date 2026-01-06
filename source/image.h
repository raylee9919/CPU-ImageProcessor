// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

struct bitmap 
{
    int Width;
    int Height;
    int BytesPerPixel;
    int Pitch;
    u8 *Data;
};

struct u8x4 
{
    u8 R,G,B,A;
};

union s32x4
{
    s32 E[4];
    __m128i V;
};

struct work_param
{
    bitmap *Dst;
    bitmap *Src;
    bitmap *Data1;
    bitmap *Data2;
    int XMin;
    int XMaxPastOne;
    int YMin;
    int YMaxPastOne;
};

#define KERNEL_RADIUS 4
// Sum: 65536
static read_only s32 Kernel[KERNEL_RADIUS*2 + 1][KERNEL_RADIUS*2 + 1] =
{
    {50, 120, 224, 326, 370, 326, 224, 120, 50},
    {120, 288, 538, 783, 887, 783, 538, 288, 120},
    {224, 538, 1005, 1462, 1657, 1462, 1005, 538, 224},
    {326, 783, 1462, 2127, 2411, 2127, 1462, 783, 326},
    {370, 887, 1657, 2411, 2732, 2411, 1657, 887, 370},
    {326, 783, 1462, 2127, 2411, 2127, 1462, 783, 326},
    {224, 538, 1005, 1462, 1657, 1462, 1005, 538, 224},
    {120, 288, 538, 783, 887, 783, 538, 288, 120},
    {50, 120, 224, 326, 370, 326, 224, 120, 50},
};

static read_only u8 GlobalThreshold = 0x4;
