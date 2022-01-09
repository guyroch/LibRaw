/* -*- C++ -*-
 * Copyright 2019-2021 LibRaw LLC (info@libraw.org)
 *
 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

//#include "../../internal/libraw_cxx_defs.h"

#include "internal/libraw_cxx_defs.h"


void gamma_curve_raw(unsigned short *curve, double value);
void write_ppm(unsigned width, unsigned height, unsigned short *bitmap, const char *basename);
void write_tiff(int width, int height, unsigned short *bitmap, const char *basename);

int LibRaw::dcraw_unprocessed_raw(void)
{
  if (!imgdata.rawdata.raw_image)
    return LIBRAW_OUT_OF_ORDER_CALL;

  is_unprocessed_raw = 1;

  // Copy area size
  int copyheight = MAX(0, MIN(int(S.height), int(S.raw_height) - int(S.top_margin)));
  int copywidth = MAX(0, MIN(int(S.width), int(S.raw_width) - int(S.left_margin)));

  // free and re-allocate image_unprocessed bitmap
  if (imgdata.image_unprocessed)
  {
    imgdata.image_unprocessed = (ushort(*))realloc(imgdata.image_unprocessed, copyheight * copywidth * sizeof(*imgdata.image_unprocessed));
    memset(imgdata.image_unprocessed, 0, copyheight * copywidth * sizeof(*imgdata.image_unprocessed));
  }
  else
  {
    imgdata.image_unprocessed = (ushort(*))calloc(copyheight * copywidth, sizeof(*imgdata.image_unprocessed));
  }

  imgdata.image_unprocessed_width = copywidth;
  imgdata.image_unprocessed_height = copyheight;

  int row, col;
  for (row = 0; row < copyheight; row++)
  {
    for (col = 0; col < copywidth; col++)
    {
      imgdata.image_unprocessed[row * copywidth + col] = imgdata.rawdata.raw_image[(row + S.top_margin) * S.raw_pitch / 2 + (col + S.left_margin)];
    }
  }

     if (!imgdata.params.no_auto_scale && imgdata.params.output_bps == 16)
  {
      if (imgdata.params.input_bps > 8 && imgdata.params.input_bps < imgdata.params.output_bps)
      {
          int bitdepth = imgdata.params.input_bps;
          int scaling = 16 - bitdepth; 

          for (int j = 0; j < imgdata.image_unprocessed_width * imgdata.image_unprocessed_height; j++)
          {
            double w = (double)imgdata.image_unprocessed[j] / pow(2, bitdepth);
            double n = bitdepth == 16 ? 65535.0 : (((65536.0) / pow(2,scaling) - 1)); 
            double m = 65536.0 / pow(2, scaling) - 1;

            if (imgdata.image_unprocessed[j] < 65535.0)
            {
              imgdata.image_unprocessed[j] = (int)(imgdata.image_unprocessed[j] / m * 65535.0 * 1.0); 
            }
          }
      }
  }
   
    //if (!imgdata.params.no_auto_scale && imgdata.params.output_bps == 16)
    //{
    //    if (imgdata.params.input_bps > 8 && imgdata.params.input_bps < imgdata.params.output_bps)
    //    {
    //        int bitdepth = imgdata.params.input_bps;
    //        int scalling = 16 - bitdepth; // imgdata.rawdata.iparam.bits; // bitDepth;

    //        for (int j = 0; j < imgdata.image_unprocessed_width * imgdata.image_unprocessed_height; j++)
    //        {
    //          double w = (double)imgdata.image_unprocessed[j] / pow(2, bitdepth);
    //          //double m = bitdepth == 16 ? 65535.0 : (((65536.0) / pow(2, scalling) - 1));
    //          double m = 65536.0 / pow(2, scalling) - 1;

    //          if (imgdata.image_unprocessed[j] < 65535.0)
    //          {
    //            imgdata.image_unprocessed[j] = (int)(imgdata.image_unprocessed[j] / m * 65535.0 * 1.0); // * w);
    //          }
    //        }
    //    }
    //}

  return 0;
}


void write_ppm(unsigned width, unsigned height, unsigned short *bitmap,
               const char *fname)
{
  if (!bitmap)
    return;

  FILE *f = fopen(fname, "wb");
  if (!f)
    return;
  int bits = 16;
  fprintf(f, "P5\n%d %d\n%d\n", width, height, (1 << bits) - 1);
  unsigned char *data = (unsigned char *)bitmap;
  unsigned data_size = width * height * 2;
#define SWAP(a, b)                                                             \
  {                                                                            \
    a ^= b;                                                                    \
    a ^= (b ^= a);                                                             \
  }
  for (unsigned i = 0; i < data_size; i += 2)
    SWAP(data[i], data[i + 1]);
#undef SWAP
  fwrite(data, data_size, 1, f);
  fclose(f);
}

#define SQR(x) ((x) * (x))

void gamma_curve_raw(unsigned short *curve, double value)
{

  double pwr = 1.0 / value;
  double ts = 0.0;
  int imax = 0xffff;
  int mode = 2;
  int i;
  double g[6], bnd[2] = {0, 0}, r;

  g[0] = pwr;
  g[1] = ts;
  g[2] = g[3] = g[4] = 0;
  bnd[g[1] >= 1] = 1;
  if (g[1] && (g[1] - 1) * (g[0] - 1) <= 0)
  {
    for (i = 0; i < 48; i++)
    {
      g[2] = (bnd[0] + bnd[1]) / 2;
      if (g[0])
        bnd[(pow(g[2] / g[1], -g[0]) - 1) / g[0] - 1 / g[2] > -1] = g[2];
      else
        bnd[g[2] / exp(1 - 1 / g[2]) < g[1]] = g[2];
    }
    g[3] = g[2] / g[1];
    if (g[0])
      g[4] = g[2] * (1 / g[0] - 1);
  }
  if (g[0])
    g[5] = 1 / (g[1] * SQR(g[3]) / 2 - g[4] * (1 - g[3]) +
                (1 - pow(g[3], 1 + g[0])) * (1 + g[4]) / (1 + g[0])) -
           1;
  else
    g[5] = 1 / (g[1] * SQR(g[3]) / 2 + 1 - g[2] - g[3] -
                g[2] * g[3] * (log(g[3]) - 1)) -
           1;
  for (i = 0; i < 0x10000; i++)
  {
    curve[i] = 0xffff;
    if ((r = (double)i / imax) < 1)
      curve[i] =
          0x10000 *
          (mode ? (r < g[3] ? r * g[1]
                            : (g[0] ? pow(r, g[0]) * (1 + g[4]) - g[4]
                                    : log(r) * g[2] + 1))
                : (r < g[2] ? r / g[1]
                            : (g[0] ? pow((r + g[4]) / (1 + g[4]), 1 / g[0])
                                    : exp((r - 1) / g[2]))));
  }
}

void tiff_set(ushort *ntag, ushort tag, ushort type, int count, int val)
{
  struct libraw_tiff_tag *tt;
  int c;

  tt = (struct libraw_tiff_tag *)(ntag + 1) + (*ntag)++;
  tt->tag = tag;
  tt->type = type;
  tt->count = count;
  if ((type < LIBRAW_EXIFTAG_TYPE_SHORT) && (count <= 4))
    for (c = 0; c < 4; c++)
      tt->val.c[c] = val >> (c << 3);
  else if (tagtypeIs(LIBRAW_EXIFTAG_TYPE_SHORT) && (count <= 2))
    for (c = 0; c < 2; c++)
      tt->val.s[c] = val >> (c << 4);
  else
    tt->val.i = val;
}
#define TOFF(ptr) ((char *)(&(ptr)) - (char *)th)

void tiff_head(int width, int height, struct tiff_hdr *th)
{
  int c;
  time_t timestamp = time(NULL);
  struct tm *t;

  memset(th, 0, sizeof *th);
  th->t_order = htonl(0x4d4d4949) >> 16;
  th->magic = 42;
  th->ifd = 10;
  tiff_set(&th->ntag, 254, 4, 1, 0);
  tiff_set(&th->ntag, 256, 4, 1, width);
  tiff_set(&th->ntag, 257, 4, 1, height);
  tiff_set(&th->ntag, 258, 3, 1, 16);
  for (c = 0; c < 4; c++)
    th->bps[c] = 16;
  tiff_set(&th->ntag, 259, 3, 1, 1);
  tiff_set(&th->ntag, 262, 3, 1, 1);
  tiff_set(&th->ntag, 273, 4, 1, sizeof *th);
  tiff_set(&th->ntag, 277, 3, 1, 1);
  tiff_set(&th->ntag, 278, 4, 1, height);
  tiff_set(&th->ntag, 279, 4, 1, height * width * 2);
  tiff_set(&th->ntag, 282, 5, 1, TOFF(th->rat[0]));
  tiff_set(&th->ntag, 283, 5, 1, TOFF(th->rat[2]));
  tiff_set(&th->ntag, 284, 3, 1, 1);
  tiff_set(&th->ntag, 296, 3, 1, 2);
  tiff_set(&th->ntag, 306, 2, 20, TOFF(th->date));
  th->rat[0] = th->rat[2] = 300;
  th->rat[1] = th->rat[3] = 1;
  t = localtime(&timestamp);
  if (t)
    sprintf(th->date, "%04d:%02d:%02d %02d:%02d:%02d", t->tm_year + 1900,
            t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
}

void write_tiff(int width, int height, unsigned short *bitmap, const char *fn)
{
  struct tiff_hdr th;

  FILE *ofp = fopen(fn, "wb");
  if (!ofp)
    return;
  tiff_head(width, height, &th);
  fwrite(&th, sizeof th, 1, ofp);
  fwrite(bitmap, 2, width * height, ofp);
  fclose(ofp);
}
