// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

// @Todo: Barrier

// [.h]
//
#include "base.h"
#include "win32.h"
#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)
#include "third_party/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_ASSERT(x)
#include "third_party/stb_image_write.h"

static b32 GlobalEnabledSSE = false;


// [.cpp]
//
#include "win32.cpp"
#include "image.cpp"

int main(int ArgCount, char **Args) 
{
    // Config
    //
    b32 IsMultithreaded = false;
    const char *filename = "C:\\dev\\swl\\image-processor\\data\\AnimeGirl.jpg";
    const char *out_filename = "C:\\dev\\swl\\image-processor\\data\\AnimeGirl_Out.png";

    for (int ArgIndex = 0; ArgIndex < ArgCount; ++ArgIndex) 
    {
        char *Option = Args[ArgIndex];
        if (!strcmp(Option, "-mt")) 
        {
            IsMultithreaded = true;
        }

        if (!strcmp(Option, "-sse")) 
        {
            GlobalEnabledSSE = true;
        }
    }


    if (IsMultithreaded) 
    {
        printf("Multi threaded.\n");
    }
    else 
    {
        printf("Single threaded.\n");
    }

    if (GlobalEnabledSSE) 
    {
        printf("SSE Enabled.\n");
    }
    printf("\n");


    // Image preparation
    //
    bitmap Image = {};
    {
        int ForcedBpp = 4;
        Image.Data = stbi_load(filename, &Image.Width, &Image.Height, &Image.BytesPerPixel, ForcedBpp);
        Image.BytesPerPixel = ForcedBpp;
        Image.Pitch = Image.Width*Image.BytesPerPixel;

        printf("[Image]\nFile Name: %s\nSize: %dx%d\nPixels: %d\n\n", filename, Image.Width, Image.Height, Image.Width*Image.Height);
    }

    int PadX = KERNEL_RADIUS*2;
    int PadY = KERNEL_RADIUS*2;
    PadX = ( PadX + 3 ) & ~3; // Align to 4-bytes boundary.
    PadY = ( PadY + 3 ) & ~3;
    int PaddedWidth  = Image.Width  + PadX;
    int PaddedHeight = Image.Height + PadY;

    bitmap GrayscaleBitmap = {};
    {
        bitmap *Bitmap = &GrayscaleBitmap;
        Bitmap->Width         = Image.Width;
        Bitmap->Height        = Image.Height;
        Bitmap->BytesPerPixel = 4;
        Bitmap->Pitch         = PaddedWidth*Bitmap->BytesPerPixel;
        u8 *Base              = (u8 *)malloc(Bitmap->Pitch*PaddedHeight);
        Bitmap->Data          = Base + Bitmap->Pitch*(PadY/2) + (PadX/2)*Bitmap->BytesPerPixel;
        memset(Base, 0, Bitmap->Pitch*PaddedHeight);
    }

    bitmap BlurredBitmap1 = {};
    {
        bitmap *Bitmap = &BlurredBitmap1;
        Bitmap->Width         = Image.Width;
        Bitmap->Height        = Image.Height;
        Bitmap->BytesPerPixel = 4;
        Bitmap->Pitch         = PaddedWidth*Bitmap->BytesPerPixel;
        u8 *Base              = (u8 *)malloc(Bitmap->Pitch*PaddedHeight);
        Bitmap->Data          = Base + Bitmap->Pitch*(PadY/2) + (PadX/2)*Bitmap->BytesPerPixel;
        memset(Base, 0, Bitmap->Pitch*PaddedHeight);
    }

    bitmap BlurredBitmap2 = {};
    {
        bitmap *Bitmap = &BlurredBitmap2;
        Bitmap->Width         = Image.Width;
        Bitmap->Height        = Image.Height;
        Bitmap->BytesPerPixel = 4;
        Bitmap->Pitch         = PaddedWidth*Bitmap->BytesPerPixel;
        u8 *Base              = (u8 *)malloc(Bitmap->Pitch*PaddedHeight);
        Bitmap->Data          = Base + Bitmap->Pitch*(PadY/2) + (PadX/2)*Bitmap->BytesPerPixel;
        memset(Base, 0, Bitmap->Pitch*PaddedHeight);
    }

    bitmap ResultBitmap = {};
    {
        bitmap *Bitmap = &ResultBitmap;
        Bitmap->Width         = Image.Width;
        Bitmap->Height        = Image.Height;
        Bitmap->BytesPerPixel = 4;
        Bitmap->Pitch         = Bitmap->Width*Bitmap->BytesPerPixel;
        Bitmap->Data          = (u8 *)malloc(Bitmap->Pitch*Bitmap->Height);
    }


    {
        ScopeClock("Total: %dms\n");

        if (IsMultithreaded) 
        {
            u32 CoreCount = GetLogicalCoreCount();
            work_queue *Queue = new work_queue;
            InitWorkQueue(Queue, CoreCount);

            u32 TileWidth  = Image.Width / CoreCount;
            u32 TileHeight = TileWidth;

            // @Temporary
            //TileWidth  = Image.Width;
            //TileHeight = Image.Height / (CoreCount*2);

            u32 TileCountX = (Image.Width  + TileWidth  - 1) / TileWidth;
            u32 TileCountY = (Image.Height + TileHeight - 1) / TileHeight;
            u32 TileCountTotal = TileCountX*TileCountY;

            printf("[Configuration]\n");
            printf("Logical Core Count: %d\n", CoreCount);
            printf("Tile Count: %d\n", TileCountTotal);
            printf("Tile Size: %dx%d\n", TileWidth, TileHeight);
            printf("\n");


            printf("[Processing]\n");
            // Grayscale Conversion Work.
            //
            {
                ScopeClock("Grayscale Conversion: %dms\n");
                for (u32 TileY = 0; TileY < TileCountY; ++TileY) 
                {
                    for (u32 TileX = 0; TileX < TileCountX; ++TileX) 
                    {
                        work_param *Param = new work_param;
                        {
                            Param->Dst          = &GrayscaleBitmap;
                            Param->Src          = &Image;
                            Param->XMin         = TileX*TileWidth;
                            Param->YMin         = TileY*TileHeight;
                            Param->XMaxPastOne  = Min((int)((TileX + 1)*TileWidth),  Image.Width);
                            Param->YMaxPastOne  = Min((int)((TileY + 1)*TileHeight), Image.Height);
                        }
                        AddWork(Queue, (work_callback *)GrayscaleConversionWork, Param);
                    }
                }
                CompleteAllWork(Queue);
            }

            // First Gaussian blur.
            //
            {
                ScopeClock("First Gaussian Blur: %dms\n");
                for (u32 TileY = 0; TileY < TileCountY; ++TileY) 
                {
                    for (u32 TileX = 0; TileX < TileCountX; ++TileX) 
                    {
                        // @Temporary
                        work_param *Param = new work_param;
                        {
                            Param->Dst          = &BlurredBitmap1;
                            Param->Src          = &GrayscaleBitmap;
                            Param->XMin         = TileX*TileWidth;
                            Param->YMin         = TileY*TileHeight;
                            Param->XMaxPastOne  = Min((int)((TileX + 1)*TileWidth),  Image.Width);
                            Param->YMaxPastOne  = Min((int)((TileY + 1)*TileHeight), Image.Height);
                        }
                        AddWork(Queue, (work_callback *)GaussianBlurWork, Param);
                    }
                }
                CompleteAllWork(Queue);
            }


            // Second Gaussian blur.
            //
            {
                ScopeClock("Second Gaussian Blur: %dms\n");
                for (u32 TileY = 0; TileY < TileCountY; ++TileY) 
                {
                    for (u32 TileX = 0; TileX < TileCountX; ++TileX) 
                    {
                        // @Temporary
                        work_param *Param = new work_param;
                        {
                            Param->Dst          = &BlurredBitmap2;
                            Param->Src          = &BlurredBitmap1;
                            Param->XMin         = TileX*TileWidth;
                            Param->YMin         = TileY*TileHeight;
                            Param->XMaxPastOne  = Min((int)((TileX + 1)*TileWidth),  Image.Width);
                            Param->YMaxPastOne  = Min((int)((TileY + 1)*TileHeight), Image.Height);
                        }
                        AddWork(Queue, (work_callback *)GaussianBlurWork, Param);
                    }
                }
                CompleteAllWork(Queue);
            }


            // Difference of Gaussian.
            //
            {
                ScopeClock("Difference of Gaussian: %dms\n");
                for (u32 TileY = 0; TileY < TileCountY; ++TileY) 
                {
                    for (u32 TileX = 0; TileX < TileCountX; ++TileX) 
                    {
                        // @Temporary
                        work_param *Param = new work_param;
                        {
                            Param->Dst          = &ResultBitmap;
                            Param->Data1        = &BlurredBitmap1;
                            Param->Data2        = &BlurredBitmap2;
                            Param->XMin         = TileX*TileWidth;
                            Param->YMin         = TileY*TileHeight;
                            Param->XMaxPastOne  = Min((int)((TileX + 1)*TileWidth),  Image.Width);
                            Param->YMaxPastOne  = Min((int)((TileY + 1)*TileHeight), Image.Height);
                        }
                        AddWork(Queue, (work_callback *)DifferenceOfGaussianWork, Param);
                    }
                }
                CompleteAllWork(Queue);
            }
        }
        else 
        {
            work_param Param = {};
            {
                Param.XMin         = 0;
                Param.YMin         = 0;
                Param.XMaxPastOne  = Image.Width;
                Param.YMaxPastOne  = Image.Height;
            }

            {
                ScopeClock("Grayscale Conversion: %dms\n");
                Param.Dst = &GrayscaleBitmap;
                Param.Src = &Image;
                GrayscaleConversionWork(&Param);
            }
            {
                ScopeClock("First Gaussian Blur: %dms\n");
                Param.Dst = &BlurredBitmap1;
                Param.Src = &GrayscaleBitmap;
                GaussianBlurWork(&Param);
            }
            {
                ScopeClock("Second Gaussian Blur: %dms\n");
                Param.Dst = &BlurredBitmap2;
                Param.Src = &BlurredBitmap1;
                GaussianBlurWork(&Param);
            }
            {
                ScopeClock("Difference of Gaussian: %dms\n");
                Param.Dst   = &ResultBitmap;
                Param.Data1 = &BlurredBitmap1;
                Param.Data2 = &BlurredBitmap2;
                DifferenceOfGaussianWork(&Param);
            }
        }
    }



    // Output file.
    {
        ScopeClock("\nFile Write: %dms\n");
        if (!stbi_write_png(out_filename, ResultBitmap.Width, ResultBitmap.Height, 4, ResultBitmap.Data, ResultBitmap.Pitch))
        {
            fprintf(stderr, "[ERROR] Couldn't write file %s.\n", filename);
            return -1;
        }
    }


    fprintf(stdout, "\nDone.\n");
    return 0;
}
