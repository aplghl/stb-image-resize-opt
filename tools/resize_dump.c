/* resize_dump - differential resize driver for stb_image_resize2.
 *
 * Compiles against either the pristine upstream oracle or the candidate header
 * (selected by -I). It runs ONE resize over a deterministic input and writes
 * the entire raw output buffer (including row padding) to stdout. Two builds
 * are byte-compared by harness/diff.sh; differences in any output value, and
 * any stray write into padding, show up as a diff.
 *
 * usage:
 *   resize_dump <input> --out WxH [options]
 *
 *   <input>            gen:grad|gen:plasma|gen:random|gen:flat
 *                      | <path to image loaded with stb_image.h>
 *   --out WxH          output dimensions (required)
 *   --in-dims WxH      input dimensions for gen: inputs (default 64x48)
 *   --in-comp N        channels for gen:/file input (default 4)
 *   --in-type T        uint8|uint8_srgb|uint8_srgb_alpha|uint16|float|half (default uint8)
 *   --out-type T       output datatype (default = --in-type)
 *   --in-layout L      1ch|2ch|rgb|bgr|4ch|rgba|bgra|argb|abgr|ra|ar
 *                      |rgba_pm|bgra_pm|argb_pm|abgr_pm|ra_pm|ar_pm (default 4ch)
 *   --out-layout L     output layout (default = --in-layout)
 *   --edge E           clamp|reflect|zero|wrap (default clamp)
 *   --filter F         default|box|triangle|cubic|catmullrom|mitchell|point|other (default mitchell)
 *   --stride-in N      input row stride in bytes (default packed)
 *   --stride-out N     output row stride in bytes (default packed)
 *   --in-subrect s0 t0 s1 t1      stbir_set_input_subrect
 *   --out-subrect x y w h         stbir_set_output_pixel_subrect
 *   --splits N         build_samplers_with_splits(N) + extended_split loop (default 1 sequential)
 *   --fast-alpha N     stbir_set_non_pm_alpha_speed_over_quality(N)
 *   --quiet            no diagnostics on stderr
 *
 * Exit code 0 on success. Only the raw output bytes go to stdout.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "gen.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* NOTE: STBIR_PROFILE is deliberately NOT defined here. It changes the size /
 * layout of internal structs and masks a pre-existing upstream uninitialized
 * read (see docs/MEASUREMENT.md); the shipped library is built without it, so
 * the oracle must be too for a like-for-like comparison. */
#ifndef RESIZE_LINK_LIB
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#endif
#include "stb_image_resize2.h"

/* ---- deterministic fill patterns ---- */
#define IN_FILL 0x3C
#define OUT_FILL 0xA5

static void die(const char *msg)
{
   fprintf(stderr, "resize_dump: %s\n", msg);
   exit(2);
}

static int parse_dims(const char *s, int *w, int *h)
{
   char *end = NULL;
   long a = strtol(s, &end, 10);
   if (!end || (*end != 'x' && *end != 'X')) return 0;
   long b = strtol(end + 1, &end, 10);
   if (*end != 0) return 0;
   *w = (int)a; *h = (int)b; return 1;
}

static int type_size(int t)
{
   switch (t) {
      case STBIR_TYPE_UINT8:
      case STBIR_TYPE_UINT8_SRGB:
      case STBIR_TYPE_UINT8_SRGB_ALPHA: return 1;
      case STBIR_TYPE_UINT16:
      case STBIR_TYPE_HALF_FLOAT: return 2;
      case STBIR_TYPE_FLOAT: return 4;
      default: return 1;
   }
}

static int parse_type(const char *s)
{
   if (!strcmp(s, "uint8"))             return STBIR_TYPE_UINT8;
   if (!strcmp(s, "uint8_srgb"))        return STBIR_TYPE_UINT8_SRGB;
   if (!strcmp(s, "uint8_srgb_alpha"))  return STBIR_TYPE_UINT8_SRGB_ALPHA;
   if (!strcmp(s, "uint16"))            return STBIR_TYPE_UINT16;
   if (!strcmp(s, "float"))             return STBIR_TYPE_FLOAT;
   if (!strcmp(s, "half"))              return STBIR_TYPE_HALF_FLOAT;
   die("bad --in-type/--out-type"); return 0;
}

static int layout_channels(int l)
{
   switch (l) {
      case STBIR_1CHANNEL: return 1;
      case STBIR_2CHANNEL: return 2;
      case STBIR_RGB:
      case STBIR_BGR:      return 3;
      case STBIR_4CHANNEL:
      case STBIR_RGBA:
      case STBIR_BGRA:
      case STBIR_ARGB:
      case STBIR_ABGR:     return 4;
      case STBIR_RA:
      case STBIR_AR:       return 2;
      case STBIR_RGBA_PM:
      case STBIR_BGRA_PM:
      case STBIR_ARGB_PM:
      case STBIR_ABGR_PM:  return 4;
      case STBIR_RA_PM:
      case STBIR_AR_PM:    return 2;
      default: return 4;
   }
}

static int parse_layout(const char *s)
{
   if (!strcmp(s, "1ch"))      return STBIR_1CHANNEL;
   if (!strcmp(s, "2ch"))      return STBIR_2CHANNEL;
   if (!strcmp(s, "rgb"))      return STBIR_RGB;
   if (!strcmp(s, "bgr"))      return STBIR_BGR;
   if (!strcmp(s, "4ch"))      return STBIR_4CHANNEL;
   if (!strcmp(s, "rgba"))     return STBIR_RGBA;
   if (!strcmp(s, "bgra"))     return STBIR_BGRA;
   if (!strcmp(s, "argb"))     return STBIR_ARGB;
   if (!strcmp(s, "abgr"))     return STBIR_ABGR;
   if (!strcmp(s, "ra"))       return STBIR_RA;
   if (!strcmp(s, "ar"))       return STBIR_AR;
   if (!strcmp(s, "rgba_pm"))  return STBIR_RGBA_PM;
   if (!strcmp(s, "bgra_pm"))  return STBIR_BGRA_PM;
   if (!strcmp(s, "argb_pm"))  return STBIR_ARGB_PM;
   if (!strcmp(s, "abgr_pm"))  return STBIR_ABGR_PM;
   if (!strcmp(s, "ra_pm"))    return STBIR_RA_PM;
   if (!strcmp(s, "ar_pm"))    return STBIR_AR_PM;
   die("bad layout"); return 0;
}

static int parse_edge(const char *s)
{
   if (!strcmp(s, "clamp"))   return STBIR_EDGE_CLAMP;
   if (!strcmp(s, "reflect")) return STBIR_EDGE_REFLECT;
   if (!strcmp(s, "zero"))    return STBIR_EDGE_ZERO;
   if (!strcmp(s, "wrap"))    return STBIR_EDGE_WRAP;
   die("bad edge"); return 0;
}

static int parse_filter(const char *s)
{
   if (!strcmp(s, "default"))    return STBIR_FILTER_DEFAULT;
   if (!strcmp(s, "box"))        return STBIR_FILTER_BOX;
   if (!strcmp(s, "triangle"))   return STBIR_FILTER_TRIANGLE;
   if (!strcmp(s, "cubic"))      return STBIR_FILTER_CUBICBSPLINE;
   if (!strcmp(s, "catmullrom")) return STBIR_FILTER_CATMULLROM;
   if (!strcmp(s, "mitchell"))   return STBIR_FILTER_MITCHELL;
   if (!strcmp(s, "point"))      return STBIR_FILTER_POINT_SAMPLE;
   if (!strcmp(s, "other"))      return STBIR_FILTER_OTHER;
   die("bad filter"); return 0;
}

/* ---- user filter callbacks (for --filter other) ---- */
static float cb_kernel(float x, float scale, void *user_data)
{
   float sum;
   (void)user_data;
   x = x * scale;
   if (x < -1.0f) x = -1.0f; else if (x > 1.0f) x = 1.0f;
   sum = 1.0f - x * x;
   if (sum < 0) sum = 0;
   return sum * sum;
}
static float cb_support(float scale, void *user_data)
{
   (void)user_data;
   return 1.0f / scale;
}

/* ---- float -> half (bit-exact, no dependency on library internals) ---- */
static uint16_t f2h(float f)
{
   uint32_t x;
   int32_t e;
   uint32_t m;
   memcpy(&x, &f, 4);
   e = (int32_t)((x >> 23) & 0xff) - 112;
   m = x & 0x007fffffu;
   if (e <= 0) {
      if (e < -10) return (uint16_t)((x >> 16) & 0x8000);
      m = (m | 0x00800000u) >> (1 - e);
      return (uint16_t)(((x >> 16) & 0x8000) | (m >> 13));
   } else if (e == 143) {
      if (m == 0) return (uint16_t)(((x >> 16) & 0x8000) | 0x7c00);
      return (uint16_t)(((x >> 16) & 0x8000) | 0x7c00 | (m >> 13) | 1);
   }
   if (e > 30) return (uint16_t)(((x >> 16) & 0x8000) | 0x7c00);
   return (uint16_t)(((x >> 16) & 0x8000) | ((uint32_t)e << 10) | (m >> 13));
}

/* Fill an input buffer of (w x h x comp) elements for the given datatype.
 * `row_stride` is in bytes. Buffer prefilled with IN_FILL. */
static void fill_input(void *buf, const char *mode, int w, int h, int comp, int type, int row_stride)
{
   int x, y, c;
   if (type == STBIR_TYPE_UINT8 || type == STBIR_TYPE_UINT8_SRGB ||
       type == STBIR_TYPE_UINT8_SRGB_ALPHA) {
      unsigned char *u8 = (unsigned char *)gen_u8(mode, w, h, comp);
      for (y = 0; y < h; ++y)
         for (x = 0; x < w * comp; ++x)
            ((unsigned char *)buf)[(size_t)y * row_stride + x] = u8[(size_t)y * w * comp + x];
      free(u8);
   } else if (type == STBIR_TYPE_UINT16) {
      unsigned char *u8 = (unsigned char *)gen_u8(mode, w, h, comp);
      for (y = 0; y < h; ++y)
         for (x = 0; x < w * comp; ++x) {
            int v = u8[(size_t)y * w * comp + x];
            ((uint16_t *)buf)[((size_t)y * row_stride) / 2 + x] = (uint16_t)((v << 8) | v);
         }
      free(u8);
   } else if (type == STBIR_TYPE_FLOAT) {
      float *f = gen_f32(mode, w, h, comp);
      for (y = 0; y < h; ++y)
         for (x = 0; x < w * comp; ++x)
            ((float *)buf)[((size_t)y * row_stride) / 4 + x] = f[(size_t)y * w * comp + x];
      free(f);
   } else { /* half */
      float *f = gen_f32(mode, w, h, comp);
      for (y = 0; y < h; ++y)
         for (x = 0; x < w * comp; ++x)
            ((uint16_t *)buf)[((size_t)y * row_stride) / 2 + x] = f2h(f[(size_t)y * w * comp + x]);
      free(f);
   }
}

int main(int argc, char **argv)
{
   const char *input = NULL;
   const char *mode = "plasma";
   int is_file = 0;
   int in_w = 64, in_h = 48, in_comp = 4;
   int out_w = 0, out_h = 0;
   int in_type = STBIR_TYPE_UINT8, out_type = -1;
   int in_layout = STBIR_4CHANNEL, out_layout = -1;
   int edge = STBIR_EDGE_CLAMP, filter = STBIR_FILTER_MITCHELL;
   int stride_in = 0, stride_out = 0;
   int have_in_sub = 0; double is0 = 0, it0 = 0, is1 = 0, it1 = 0;
   int have_out_sub = 0; int osx = 0, osy = 0, osw = 0, osh = 0;
   int splits = 1;
   int fast_alpha = 0, have_fast_alpha = 0;
   int quiet = 0;
   int i;

   for (i = 1; i < argc; ++i) {
      const char *a = argv[i];
      if (!strcmp(a, "--out"))          { if (++i >= argc || !parse_dims(argv[i], &out_w, &out_h)) die("--out WxH"); }
      else if (!strcmp(a, "--in-dims")) { if (++i >= argc || !parse_dims(argv[i], &in_w, &in_h)) die("--in-dims WxH"); }
      else if (!strcmp(a, "--in-comp")) { if (++i >= argc) die("--in-comp"); in_comp = atoi(argv[i]); }
      else if (!strcmp(a, "--in-type")) { if (++i >= argc) die("--in-type"); in_type = parse_type(argv[i]); }
      else if (!strcmp(a, "--out-type")){ if (++i >= argc) die("--out-type"); out_type = parse_type(argv[i]); }
      else if (!strcmp(a, "--in-layout")) { if (++i >= argc) die("--in-layout"); in_layout = parse_layout(argv[i]); }
      else if (!strcmp(a, "--out-layout")){ if (++i >= argc) die("--out-layout"); out_layout = parse_layout(argv[i]); }
      else if (!strcmp(a, "--edge"))    { if (++i >= argc) die("--edge"); edge = parse_edge(argv[i]); }
      else if (!strcmp(a, "--filter"))  { if (++i >= argc) die("--filter"); filter = parse_filter(argv[i]); }
      else if (!strcmp(a, "--stride-in"))  { if (++i >= argc) die("--stride-in"); stride_in = atoi(argv[i]); }
      else if (!strcmp(a, "--stride-out")) { if (++i >= argc) die("--stride-out"); stride_out = atoi(argv[i]); }
      else if (!strcmp(a, "--in-subrect")) {
         if (i + 4 >= argc) die("--in-subrect s0 t0 s1 t1");
         is0 = atof(argv[++i]); it0 = atof(argv[++i]); is1 = atof(argv[++i]); it1 = atof(argv[++i]); have_in_sub = 1;
      }
      else if (!strcmp(a, "--out-subrect")) {
         if (i + 4 >= argc) die("--out-subrect x y w h");
         osx = atoi(argv[++i]); osy = atoi(argv[++i]); osw = atoi(argv[++i]); osh = atoi(argv[++i]); have_out_sub = 1;
      }
      else if (!strcmp(a, "--splits")) { if (++i >= argc) die("--splits"); splits = atoi(argv[i]); }
      else if (!strcmp(a, "--fast-alpha")) { if (++i >= argc) die("--fast-alpha"); fast_alpha = atoi(argv[i]); have_fast_alpha = 1; }
      else if (!strcmp(a, "--quiet")) quiet = 1;
      else if (a[0] == '-') die("unknown option");
      else input = a;
   }

   if (!input || !out_w || !out_h) die("need <input> and --out WxH");
   if (out_type < 0) out_type = in_type;
   if (out_layout < 0) out_layout = in_layout;

   if (!strncmp(input, "gen:", 4)) { mode = input + 4; is_file = 0; }
   else is_file = 1;

   int in_elem = type_size(in_type);
   int in_ch = layout_channels(in_layout);

   /* ---- file input: load first so dimensions are known before allocation ---- */
   unsigned char *filedata = NULL;
   if (is_file) {
      int fw = 0, fh = 0, fc = 0;
      filedata = stbi_load(input, &fw, &fh, &fc, in_ch);
      if (!filedata) die("stbi_load failed");
      in_w = fw; in_h = fh; in_comp = in_ch;
      in_elem = 1;
      in_type = STBIR_TYPE_UINT8;
   }

   /* ---- input buffer ---- */
   if (stride_in <= 0) stride_in = in_w * in_ch * in_elem;
   if (stride_in < in_w * in_ch * in_elem) stride_in = in_w * in_ch * in_elem;
   size_t in_bytes = (size_t)stride_in * (size_t)in_h + 64;
   void *inbuf = malloc(in_bytes);
   if (!inbuf) die("in malloc");
   memset(inbuf, IN_FILL, in_bytes);

   if (is_file) {
      int y;
      for (y = 0; y < in_h; ++y)
         memcpy((char *)inbuf + (size_t)y * stride_in, filedata + (size_t)y * in_w * in_ch, (size_t)in_w * in_ch);
      stbi_image_free(filedata);
   } else {
      fill_input(inbuf, mode, in_w, in_h, in_ch, in_type, stride_in);
   }

   /* ---- output buffer ---- */
   int out_elem = type_size(out_type);
   int out_ch = layout_channels(out_layout);
   if (stride_out <= 0) stride_out = out_w * out_ch * out_elem;
   size_t out_bytes = (size_t)stride_out * (size_t)out_h + 64;
   void *outbuf = malloc(out_bytes);
   if (!outbuf) die("out malloc");
   memset(outbuf, OUT_FILL, out_bytes);

   /* ---- resize ---- */
   STBIR_RESIZE r;
   stbir_resize_init(&r, inbuf, in_w, in_h, stride_in,
                         outbuf, out_w, out_h, stride_out,
                     (stbir_pixel_layout)in_layout, (stbir_datatype)in_type);
   stbir_set_datatypes(&r, (stbir_datatype)in_type, (stbir_datatype)out_type);
   stbir_set_pixel_layouts(&r, (stbir_pixel_layout)in_layout, (stbir_pixel_layout)out_layout);
   stbir_set_edgemodes(&r, (stbir_edge)edge, (stbir_edge)edge);
   if (filter == STBIR_FILTER_OTHER) {
      /* upstream rejects STBIR_FILTER_OTHER in stbir_set_filters; install
       * callbacks and let it fall back to the default builtin enum. */
      stbir_set_filters(&r, STBIR_FILTER_DEFAULT, STBIR_FILTER_DEFAULT);
      stbir_set_filter_callbacks(&r, cb_kernel, cb_support, cb_kernel, cb_support);
   } else {
      stbir_set_filters(&r, (stbir_filter)filter, (stbir_filter)filter);
   }
   if (have_in_sub)  stbir_set_input_subrect(&r, is0, it0, is1, it1);
   if (have_out_sub) stbir_set_output_pixel_subrect(&r, osx, osy, osw, osh);
   if (have_fast_alpha) stbir_set_non_pm_alpha_speed_over_quality(&r, fast_alpha);

   int ok = 0;
   if (splits > 1) {
      int ns = stbir_build_samplers_with_splits(&r, splits);
      if (ns < 1) die("build_samplers_with_splits");
      for (i = 0; i < ns; ++i) {
         if (!stbir_resize_extended_split(&r, i, 1)) die("resize_extended_split");
      }
      stbir_free_samplers(&r);
      ok = 1;
   } else {
      ok = stbir_resize_extended(&r);
   }
   if (!ok) die("resize failed");

   if (!quiet)
      fprintf(stderr, "ok in=%dx%d c%d t%d -> out=%dx%d c%d t%d layout %d->%d edge %d filter %d splits %d\n",
              in_w, in_h, in_ch, in_type, out_w, out_h, out_ch, out_type,
              in_layout, out_layout, edge, filter, splits);

   /* Dump the whole output buffer including padding. */
   if (fwrite(outbuf, 1, out_bytes, stdout) != out_bytes) die("fwrite");

   free(inbuf);
   free(outbuf);
   return 0;
}
