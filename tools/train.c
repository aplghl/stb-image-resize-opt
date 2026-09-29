/* train.c - held-out PGO training driver for the prebuilt resize library.
 *
 * Links against the (instrumented) static library and only uses the public API.
 * The workload set is deliberately disjoint from bench/bench.c's measured rows
 * (different sizes, filters, layouts, datatypes) so PGO is trained out-of-
 * sample. Run once with STBIR_CPU=base and once with STBIR_CPU=avx2 so both
 * variant objects collect a profile.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"
#include "stb_image_resize2.h"

typedef struct { int iw, ih, ow, oh, type, layout, edge, filter, reps; } trow;

static const trow TROWS[] = {
   { 320, 240,  111,  97, STBIR_TYPE_UINT8, STBIR_4CHANNEL, STBIR_EDGE_CLAMP,   STBIR_FILTER_DEFAULT,   2 },
   { 200, 200,  800, 800, STBIR_TYPE_UINT8, STBIR_RGB,      STBIR_EDGE_CLAMP,   STBIR_FILTER_MITCHELL,  2 },
   { 300, 180,   75,  45, STBIR_TYPE_FLOAT, STBIR_4CHANNEL, STBIR_EDGE_CLAMP,   STBIR_FILTER_DEFAULT,   2 },
   { 128, 128,  384, 384, STBIR_TYPE_UINT8, STBIR_1CHANNEL, STBIR_EDGE_CLAMP,   STBIR_FILTER_TRIANGLE,  2 },
   { 512, 512,  128, 128, STBIR_TYPE_UINT8, STBIR_4CHANNEL, STBIR_EDGE_CLAMP,   STBIR_FILTER_CATMULLROM,2 },
   { 700, 500,  350, 250, STBIR_TYPE_FLOAT, STBIR_4CHANNEL, STBIR_EDGE_REFLECT, STBIR_FILTER_DEFAULT,   2 },
   { 450, 300,  900, 600, STBIR_TYPE_UINT8, STBIR_RGBA,     STBIR_EDGE_CLAMP,   STBIR_FILTER_DEFAULT,   2 },
   { 360, 270,  180, 135, STBIR_TYPE_UINT16,STBIR_4CHANNEL, STBIR_EDGE_CLAMP,   STBIR_FILTER_DEFAULT,   2 },
   { 256, 256,  192, 192, STBIR_TYPE_HALF_FLOAT, STBIR_4CHANNEL, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 2 },
   { 640, 360,  320, 180, STBIR_TYPE_UINT8, STBIR_BGRA,     STBIR_EDGE_ZERO,    STBIR_FILTER_BOX,       2 },
   { 100, 100,  250, 250, STBIR_TYPE_UINT8, STBIR_2CHANNEL, STBIR_EDGE_WRAP,    STBIR_FILTER_TRIANGLE,  2 },
   { 800, 600,  100,  75, STBIR_TYPE_UINT8, STBIR_4CHANNEL, STBIR_EDGE_CLAMP,   STBIR_FILTER_MITCHELL,  2 },
};

int main(void)
{
   int rep, i, k;
   for (rep = 0; rep < 2; ++rep)
   for (i = 0; i < (int)(sizeof(TROWS)/sizeof(TROWS[0])); ++i) {
      const trow *r = &TROWS[i];
      int ch = (r->layout == STBIR_1CHANNEL) ? 1 : (r->layout == STBIR_2CHANNEL ? 2 : (r->layout == STBIR_RGB ? 3 : 4));
      int esz = (r->type == STBIR_TYPE_FLOAT) ? 4 : (r->type == STBIR_TYPE_UINT16 || r->type == STBIR_TYPE_HALF_FLOAT) ? 2 : 1;
      size_t ib = (size_t)r->iw * r->ih * ch * esz;
      size_t ob = (size_t)r->ow * r->oh * ch * esz;
      void *in = malloc(ib), *out = malloc(ob);
      if (!in || !out) return 1;
      if (r->type == STBIR_TYPE_FLOAT) { float *f = gen_f32("plasma", r->iw, r->ih, ch); memcpy(in, f, ib); free(f); }
      else {
         unsigned char *u = gen_u8("plasma", r->iw, r->ih, ch);
         size_t n = (size_t)r->iw * r->ih * ch;
         if (r->type == STBIR_TYPE_HALF_FLOAT) {
            uint16_t *h = (uint16_t *)in;                 /* 0x3800 == 0.5h */
            for (k = 0; k < (int)n; ++k) h[k] = 0x3800;
         } else if (r->type == STBIR_TYPE_UINT16) {
            uint16_t *h = (uint16_t *)in;
            for (k = 0; k < (int)n; ++k) { int v = u[k]; h[k] = (uint16_t)((v << 8) | v); }
         } else {
            memcpy(in, u, ib);
         }
         free(u);
      }
      for (k = 0; k < r->reps; ++k) {
         STBIR_RESIZE rr;
         stbir_resize_init(&rr, in, r->iw, r->ih, 0, out, r->ow, r->oh, 0, r->layout, r->type);
         stbir_set_edgemodes(&rr, r->edge, r->edge);
         stbir_set_filters(&rr, r->filter, r->filter);
         if (!stbir_resize_extended(&rr)) return 1;
      }
      free(in); free(out);
   }
   return 0;
}
