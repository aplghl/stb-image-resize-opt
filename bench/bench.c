/* bench.c - resize throughput over a regime matrix.
 *
 * Prints CSV to stdout:
 *   row_id,input_w,input_h,out_w,out_h,type,layout,edge,filter,iters,ns_per_resize,ns_per_outpixel,Mpx_per_s
 *
 * The input pixels are generated once (outside the timed region). Each timed
 * iteration constructs a fresh STBIR_RESIZE and calls stbir_resize_extended,
 * which includes the sampler/coefficient build cost (the typical single-shot
 * usage). Prebuilt-sampler reuse is measured separately by --reuse.
 *
 * Build against the oracle (upstream/) or the candidate (src/) by -I; the two
 * binaries emit the same row_ids so harness/bench_vs_upstream.sh can join them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "gen.h"

#ifndef RESIZE_LINK_LIB
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#endif
#include "stb_image_resize2.h"

static double now_ns(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

typedef struct {
   int iw, ih, ow, oh;
   int type;      /* STBIR_TYPE_UINT8 | STBIR_TYPE_FLOAT */
   int layout;
   int edge;
   int filter;
   int max_iters;
} row_t;

/* Representative regime matrix. Compute-bound rows only (no output callbacks). */
static const row_t ROWS[] = {
   /* input 256x256 */
   {  256,  256,  128,  128, STBIR_TYPE_UINT8, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 400 }, /* down 2x */
   {  256,  256,  256,  256, STBIR_TYPE_UINT8, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 400 }, /* same   */
   {  256,  256,  512,  512, STBIR_TYPE_UINT8, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 150 }, /* up 2x  */
   {  256,  256,  512,  512, STBIR_TYPE_UINT8, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_POINT_SAMPLE, 300 },
   {  256,  256,  512,  512, STBIR_TYPE_UINT8, STBIR_RGB,      STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 150 },
   {  256,  256,  512,  512, STBIR_TYPE_UINT8, STBIR_1CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 150 },
   {  256,  256,  512,  512, STBIR_TYPE_FLOAT, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 100 },
   {  256,  256,  128,  128, STBIR_TYPE_FLOAT, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 250 },
   /* input 1024x1024 */
   { 1024, 1024,   64,   64, STBIR_TYPE_UINT8, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 250 }, /* extreme down */
   { 1024, 1024,  256,  256, STBIR_TYPE_UINT8, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 200 }, /* down 4x */
   { 1024, 1024,  512,  512, STBIR_TYPE_UINT8, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 120 }, /* down 2x */
   { 1024, 1024, 2048, 2048, STBIR_TYPE_UINT8, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT,  20 }, /* up 2x  */
   { 1024, 1024, 1024, 1024, STBIR_TYPE_FLOAT, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT,  30 }, /* same   */
   /* half output falls back to the baseline (signed-zero exactness), so these
    * rows measure only the baseline improvement; uint16 uses the AVX2 path. */
   {  256,  256,  512,  512, STBIR_TYPE_HALF_FLOAT, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 100 },
   {  256,  256,  128,  128, STBIR_TYPE_HALF_FLOAT, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 250 },
   {  256,  256,  512,  512, STBIR_TYPE_UINT16, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 100 },
   {  256,  256,  128,  128, STBIR_TYPE_UINT16, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 250 },
   /* non-premultiplied alpha (alpha weighting/unweighting) and premultiplied */
   {  256,  256,  512,  512, STBIR_TYPE_UINT8, STBIR_RGBA, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT,  80 },
   {  256,  256,  128,  128, STBIR_TYPE_UINT8, STBIR_RGBA, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 200 },
   {  256,  256,  512,  512, STBIR_TYPE_UINT8, STBIR_RGBA_PM, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 120 },
   {  256,  256,  128,  128, STBIR_TYPE_UINT8, STBIR_RGBA_PM, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 250 },
   {  256,  256,  512,  512, STBIR_TYPE_UINT8_SRGB, STBIR_RGBA, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 80 },
   {  256,  256,  128,  128, STBIR_TYPE_UINT8_SRGB, STBIR_RGBA, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 200 },
};

static const char *type_name(int t)
{
   switch (t) {
      case STBIR_TYPE_FLOAT:      return "float";
      case STBIR_TYPE_HALF_FLOAT: return "half";
      case STBIR_TYPE_UINT16:     return "uint16";
      default:                    return "uint8";
   }
}
static const char *layout_name(int l)
{
   switch (l) {
      case STBIR_1CHANNEL: return "1ch";
      case STBIR_2CHANNEL: return "2ch";
      case STBIR_RGB:      return "rgb";
      case STBIR_4CHANNEL: return "4ch";
      case STBIR_RGBA:     return "rgba";
      case STBIR_BGRA:     return "bgra";
      case STBIR_RGBA_PM:  return "rgba_pm";
      case STBIR_BGRA_PM:  return "bgra_pm";
      default:             return "?";
   }
}
static const char *filter_name(int f)
{
   switch (f) {
      case STBIR_FILTER_POINT_SAMPLE: return "point";
      default:                        return "default";
   }
}

/* Held-out PGO training set: sizes / filters / layouts deliberately disjoint
 * from the measured ROWS. Used by --train so the profile does not overfit the
 * benchmark rows. */
static const row_t TRAIN_ROWS[] = {
   {  320,  240,  111,   97, STBIR_TYPE_UINT8, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 3 },
   {  200,  200,  800,  800, STBIR_TYPE_UINT8, STBIR_RGB,      STBIR_EDGE_CLAMP, STBIR_FILTER_MITCHELL, 3 },
   {  300,  180,   75,   45, STBIR_TYPE_FLOAT, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 3 },
   {  128,  128,  384,  384, STBIR_TYPE_UINT8, STBIR_1CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_TRIANGLE, 3 },
   {  512,  512,  128,  128, STBIR_TYPE_UINT8, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_CATMULLROM, 3 },
   {  700,  500,  350,  250, STBIR_TYPE_FLOAT, STBIR_4CHANNEL, STBIR_EDGE_REFLECT, STBIR_FILTER_DEFAULT, 3 },
   {  450,  300,  900,  600, STBIR_TYPE_UINT8, STBIR_RGBA,     STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 3 },
};

static int type_elem(int t)
{
   return (t == STBIR_TYPE_UINT16 || t == STBIR_TYPE_HALF_FLOAT) ? 2 : (t == STBIR_TYPE_FLOAT ? 4 : 1);
}

static void fill_pixels(void *buf, int type, int w, int h, int ch)
{
   size_t n = (size_t)w * h * ch, i;
   if (type == STBIR_TYPE_FLOAT) {
      float *f = gen_f32("plasma", w, h, ch); memcpy(buf, f, n * 4); free(f);
   } else if (type == STBIR_TYPE_HALF_FLOAT) {
      uint16_t *hp = (uint16_t *)buf; for (i = 0; i < n; ++i) hp[i] = 0x3800; /* 0.5h */
   } else if (type == STBIR_TYPE_UINT16) {
      unsigned char *u = gen_u8("plasma", w, h, ch); uint16_t *hp = (uint16_t *)buf;
      for (i = 0; i < n; ++i) { int v = u[i]; hp[i] = (uint16_t)((v << 8) | v); }
      free(u);
   } else {
      unsigned char *u = gen_u8("plasma", w, h, ch); memcpy(buf, u, n); free(u);
   }
}

static int run_train(void)
{
   int i, rep, k;
   for (rep = 0; rep < 3; ++rep)
   for (i = 0; i < (int)(sizeof(TRAIN_ROWS) / sizeof(TRAIN_ROWS[0])); ++i) {
      const row_t *r = &TRAIN_ROWS[i];
      int ch = (r->layout == STBIR_1CHANNEL) ? 1 : (r->layout == STBIR_RGB ? 3 : 4);
      int esz = type_elem(r->type);
      size_t in_bytes = (size_t)r->iw * r->ih * ch * esz;
      size_t out_bytes = (size_t)r->ow * r->oh * ch * esz;
      void *inbuf = malloc(in_bytes), *outbuf = malloc(out_bytes);
      if (!inbuf || !outbuf) return 1;
      fill_pixels(inbuf, r->type, r->iw, r->ih, ch);
      for (k = 0; k < r->max_iters; ++k) {
         STBIR_RESIZE rr;
         stbir_resize_init(&rr, inbuf, r->iw, r->ih, 0, outbuf, r->ow, r->oh, 0, r->layout, r->type);
         stbir_set_edgemodes(&rr, r->edge, r->edge);
         stbir_set_filters(&rr, r->filter, r->filter);
         if (!stbir_resize_extended(&rr)) return 1;
      }
      free(inbuf); free(outbuf);
   }
   return 0;
}

int main(int argc, char **argv)
{
   int reuse = 0, i;
   if (argc > 1 && !strcmp(argv[1], "--train")) return run_train();
   if (argc > 1 && !strcmp(argv[1], "--reuse")) reuse = 1;

   printf("row_id,input_w,input_h,out_w,out_h,type,layout,edge,filter,iters,ns_per_resize,ns_per_outpixel,Mpx_per_s\n");

   for (i = 0; i < (int)(sizeof(ROWS) / sizeof(ROWS[0])); ++i) {
      const row_t *r = &ROWS[i];
      int ch = (r->layout == STBIR_1CHANNEL) ? 1 : (r->layout == STBIR_RGB ? 3 : 4);
      void *inbuf, *outbuf;
      int esz = type_elem(r->type);
      size_t in_bytes = (size_t)r->iw * r->ih * ch * esz;
      size_t out_bytes = (size_t)r->ow * r->oh * ch * esz;
      int iters = r->max_iters, k;
      double best = 1e30, ns_per_resize, ns_per_outpixel, mpx;

      inbuf = malloc(in_bytes);
      outbuf = malloc(out_bytes);
      if (!inbuf || !outbuf) { fprintf(stderr, "oom\n"); return 1; }
      fill_pixels(inbuf, r->type, r->iw, r->ih, ch);

      if (reuse) {
         /* prebuild samplers once, then time only the resample */
         STBIR_RESIZE r0;
         stbir_resize_init(&r0, inbuf, r->iw, r->ih, 0, outbuf, r->ow, r->oh, 0, r->layout, r->type);
         stbir_set_edgemodes(&r0, r->edge, r->edge);
         stbir_set_filters(&r0, r->filter, r->filter);
         if (!stbir_build_samplers(&r0)) { fprintf(stderr, "build failed\n"); return 1; }
         for (k = 0; k < iters; ++k) {
            double a = now_ns();
            if (!stbir_resize_extended(&r0)) { fprintf(stderr, "resize failed\n"); return 1; }
            double dt = now_ns() - a;
            if (dt < best) best = dt;
         }
         stbir_free_samplers(&r0);
      } else {
         for (k = 0; k < iters; ++k) {
            STBIR_RESIZE rr;
            double a, dt;
            stbir_resize_init(&rr, inbuf, r->iw, r->ih, 0, outbuf, r->ow, r->oh, 0, r->layout, r->type);
            stbir_set_edgemodes(&rr, r->edge, r->edge);
            stbir_set_filters(&rr, r->filter, r->filter);
            a = now_ns();
            if (!stbir_resize_extended(&rr)) { fprintf(stderr, "resize failed\n"); return 1; }
            dt = now_ns() - a;
            if (dt < best) best = dt;
         }
      }

      ns_per_resize = best;
      ns_per_outpixel = best / ((double)r->ow * r->oh);
      mpx = ((double)r->ow * r->oh) / best * 1000.0; /* Mpixel/s */

      printf("r%d,%d,%d,%d,%d,%s,%s,%s,%s,%d,%.1f,%.4f,%.1f\n",
             i, r->iw, r->ih, r->ow, r->oh, type_name(r->type), layout_name(r->layout),
             r->edge == STBIR_EDGE_CLAMP ? "clamp" : "?", filter_name(r->filter),
             iters, ns_per_resize, ns_per_outpixel, mpx);
      fflush(stdout);

      free(inbuf);
      free(outbuf);
   }
   return 0;
}
