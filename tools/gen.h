/* Deterministic pixel generators shared by tools/write_dump.c and
 * bench/bench.c. No external dependencies; identical output on every platform
 * for the same (mode, w, h, comp, seed). */
#ifndef WRITE_OPT_GEN_H
#define WRITE_OPT_GEN_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

static uint32_t g_seed = 1;

static uint32_t xrand(void)
{
   uint32_t x = g_seed;
   x ^= x << 13; x ^= x >> 17; x ^= x << 5;
   return g_seed = x;
}

/* mode: grad | plasma | random | flat */
static unsigned char *gen_u8(const char *mode, int w, int h, int comp)
{
   unsigned char *p = (unsigned char *)malloc((size_t)w * h * comp);
   int x, y;
   if (!p) return NULL;
   for (y = 0; y < h; ++y) {
      for (x = 0; x < w; ++x) {
         unsigned char *q = p + ((size_t)y * w + x) * comp;
         double v;
         if (strcmp(mode, "grad") == 0) {
            q[0] = (unsigned char)(w > 1 ? x * 255 / (w - 1) : 0);
            if (comp > 1) q[1] = (unsigned char)(h > 1 ? y * 255 / (h - 1) : 0);
            if (comp > 2) q[2] = (unsigned char)(((x + y) * 255) / ((w + h) > 2 ? (w + h - 2) : 1));
            if (comp > 3) q[3] = 255;
         } else if (strcmp(mode, "plasma") == 0) {
            double fx = (double)x / w, fy = (double)y / h;
            v = sin(fx * 12.0) + sin(fy * 9.0) + sin((fx + fy) * 7.0) + sin(sqrt(fx * fx + fy * fy) * 20.0);
            q[0] = (unsigned char)((v + 4.0) * 31.8);
            if (comp > 1) q[1] = (unsigned char)((sin(fx * 20.0) + 1.0) * 127.5);
            if (comp > 2) q[2] = (unsigned char)((sin(fy * 17.0 + 1.0) + 1.0) * 127.5);
            if (comp > 3) q[3] = 255;
         } else if (strcmp(mode, "random") == 0) {
            int c;
            for (c = 0; c < comp; ++c) q[c] = (unsigned char)(xrand() >> 24);
         } else { /* flat */
            int c;
            for (c = 0; c < comp; ++c) q[c] = (unsigned char)(c == 3 ? 255 : 128);
         }
      }
   }
   return p;
}

static float *gen_f32(const char *mode, int w, int h, int comp)
{
   float *p = (float *)malloc((size_t)w * h * comp * sizeof(float));
   int x, y;
   if (!p) return NULL;
   for (y = 0; y < h; ++y) {
      for (x = 0; x < w; ++x) {
         float *q = p + ((size_t)y * w + x) * comp;
         double v;
         if (strcmp(mode, "plasma") == 0) {
            double fx = (double)x / w, fy = (double)y / h;
            v = sin(fx * 12.0) + sin(fy * 9.0) + sin((fx + fy) * 7.0);
            q[0] = (float)(v + 3.0);
            if (comp > 1) q[1] = (float)(sin(fx * 20.0) + 1.0);
            if (comp > 2) q[2] = (float)(sin(fy * 17.0 + 1.0) + 1.0);
         } else if (strcmp(mode, "random") == 0) {
            int c;
            for (c = 0; c < comp; ++c) q[c] = (float)(xrand() >> 8) / 16777216.0f * 4.0f;
         } else { /* flat / grad */
            q[0] = (float)x / (w > 1 ? w - 1 : 1) * 4.0f;
            if (comp > 1) q[1] = (float)y / (h > 1 ? h - 1 : 1) * 4.0f;
            if (comp > 2) q[2] = 0.5f;
         }
      }
   }
   return p;
}

#endif
