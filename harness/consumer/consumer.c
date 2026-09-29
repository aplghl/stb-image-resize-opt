/* Downstream consumer: load a PNG with stb_image, resize with stb_image_resize2
 * through its public API, and print ns per output pixel for a few common
 * workloads. Benchmarked against stock upstream; the fork links the prebuilt
 * runtime-dispatched static library.
 *
 * usage: consumer <image.png>
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

/* Returns min ns per output pixel over `iters` runs. */
static double bench_case(const unsigned char *in, int iw, int ih, int comp,
                         int ow, int oh, int iters)
{
   unsigned char *out = (unsigned char *)malloc((size_t)ow * oh * comp);
   double best = 1e30;
   int i;
   if (!out) { fprintf(stderr, "oom\n"); exit(1); }
   for (i = 0; i < iters; ++i) {
      double a = now_ns();
      if (!stbir_resize_uint8_srgb(in, iw, ih, 0, out, ow, oh, 0,
                                   (stbir_pixel_layout)comp)) {
         fprintf(stderr, "resize failed\n"); exit(1);
      }
      double dt = now_ns() - a;
      if (dt < best) best = dt;
   }
   free(out);
   return best / ((double)ow * oh);
}

int main(int argc, char **argv)
{
   int w, h, comp;
   unsigned char *in;
   if (argc < 2) { fprintf(stderr, "usage: consumer <image.png>\n"); return 2; }
   in = stbi_load(argv[1], &w, &h, &comp, 4);
   if (!in) { fprintf(stderr, "load failed: %s\n", stbi_failure_reason()); return 1; }
   comp = 4;

   printf("down2,%.6f\n",  bench_case(in, w, h, comp, w / 2, h / 2, 30));
   printf("down4,%.6f\n",  bench_case(in, w, h, comp, w / 4, h / 4, 30));
   printf("same,%.6f\n",   bench_case(in, w, h, comp, w, h, 20));
   printf("up2,%.6f\n",    bench_case(in, w, h, comp, w * 2, h * 2, 8));
   printf("thumb,%.6f\n",  bench_case(in, w, h, comp, 96, 96, 60));
   stbi_image_free(in);
   return 0;
}
