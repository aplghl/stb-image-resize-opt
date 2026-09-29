/* Runtime dispatch layer for stb_image_resize2.
 *
 * Provides the canonical public API (same symbols/ABI as upstream) and
 * forwards each call to the compiled-in baseline variant, or to the AVX2
 * variant when the CPU supports the x86-64-v3 feature set.
 *
 * Exactness note: pristine upstream emits a different sign bit of zero for a
 * few half-float output filter cases between its SSE2 and AVX2 code paths
 * (numerically identical; documented in docs/MEASUREMENT.md). To keep the
 * default library byte-identical to the SSE2 oracle, the AVX2 variant is used
 * for every output datatype EXCEPT half-float, which falls back to baseline.
 * Set STBIR_CPU=base (or =avx2) to override the variant for testing.
 */
#include <stdlib.h>
#include <string.h>

#include "stb_image_resize2.h"   /* declarations only (canonical names/types) */

/* ---- variant prototypes (renamed public functions) ---- */
#define STBIR_VARIANT_PROTOS(SFX)                                                             \
  unsigned char * stbir_resize_uint8_srgb##SFX ( const unsigned char *, int, int, int,        \
      unsigned char *, int, int, int, stbir_pixel_layout );                                   \
  unsigned char * stbir_resize_uint8_linear##SFX ( const unsigned char *, int, int, int,      \
      unsigned char *, int, int, int, stbir_pixel_layout );                                   \
  float * stbir_resize_float_linear##SFX ( const float *, int, int, int,                      \
      float *, int, int, int, stbir_pixel_layout );                                           \
  void * stbir_resize##SFX ( const void *, int, int, int, void *, int, int, int,              \
      stbir_pixel_layout, stbir_datatype, stbir_edge, stbir_filter );                         \
  void stbir_resize_init##SFX ( STBIR_RESIZE *, const void *, int, int, int,                  \
      void *, int, int, int, stbir_pixel_layout, stbir_datatype );                            \
  void stbir_set_datatypes##SFX ( STBIR_RESIZE *, stbir_datatype, stbir_datatype );           \
  void stbir_set_pixel_callbacks##SFX ( STBIR_RESIZE *, stbir_input_callback *,               \
      stbir_output_callback * );                                                              \
  void stbir_set_user_data##SFX ( STBIR_RESIZE *, void * );                                   \
  void stbir_set_buffer_ptrs##SFX ( STBIR_RESIZE *, const void *, int, void *, int );         \
  int stbir_set_pixel_layouts##SFX ( STBIR_RESIZE *, stbir_pixel_layout, stbir_pixel_layout ); \
  int stbir_set_edgemodes##SFX ( STBIR_RESIZE *, stbir_edge, stbir_edge );                    \
  int stbir_set_filters##SFX ( STBIR_RESIZE *, stbir_filter, stbir_filter );                  \
  int stbir_set_filter_callbacks##SFX ( STBIR_RESIZE *, stbir__kernel_callback *,             \
      stbir__support_callback *, stbir__kernel_callback *, stbir__support_callback * );       \
  int stbir_set_pixel_subrect##SFX ( STBIR_RESIZE *, int, int, int, int );                    \
  int stbir_set_input_subrect##SFX ( STBIR_RESIZE *, double, double, double, double );        \
  int stbir_set_output_pixel_subrect##SFX ( STBIR_RESIZE *, int, int, int, int );             \
  int stbir_set_non_pm_alpha_speed_over_quality##SFX ( STBIR_RESIZE *, int );                 \
  int stbir_build_samplers##SFX ( STBIR_RESIZE * );                                           \
  void stbir_free_samplers##SFX ( STBIR_RESIZE * );                                           \
  int stbir_resize_extended##SFX ( STBIR_RESIZE * );                                          \
  int stbir_build_samplers_with_splits##SFX ( STBIR_RESIZE *, int );                          \
  int stbir_resize_extended_split##SFX ( STBIR_RESIZE *, int, int );

STBIR_VARIANT_PROTOS(_base)
#ifdef STBIR_DISPATCH_AVX2
STBIR_VARIANT_PROTOS(_avx2)
#endif

/* ---- CPU feature detection (self-contained; no __builtin_cpu_supports) ---- */
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

#include <cpuid.h>

static unsigned long long stbir__xgetbv0(void)
{
   unsigned int eax, edx;
   __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
   return ((unsigned long long)edx << 32) | eax;
}

/* x86-64-v3 == AVX2 + BMI1 + BMI2 + FMA + F16C + MOVBE + AVX (with OS YMM). */
static int stbir__cpu_has_v3(void)
{
   unsigned int eax, ebx, ecx, edx;
   if (!__get_cpuid_count(1, 0, &eax, &ebx, &ecx, &edx))
      return 0;
   if (!(ecx & (1u << 27)) || !(ecx & (1u << 28)))   /* OSXSAVE, AVX */
      return 0;
   if ((stbir__xgetbv0() & 0x6) != 0x6)              /* XCR0 XMM+YMM */
      return 0;
   if (!(ecx & (1u << 29)) || !(ecx & (1u << 12)) || !(ecx & (1u << 22)))  /* F16C, FMA, MOVBE */
      return 0;
   if (!__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx))
      return 0;
   if (!(ebx & (1u << 5)) || !(ebx & (1u << 3)) || !(ebx & (1u << 8)))     /* AVX2, BMI1, BMI2 */
      return 0;
   return 1;
}

#else
static int stbir__cpu_has_v3(void) { return 0; }
#endif

static int stbir__use_avx2(void)
{
   static int cached = -1;
   const char *force;
   if (cached >= 0)
      return cached;
#ifdef STBIR_FORCE_BASE
   cached = 0;
   return cached;
#else
   if (!stbir__cpu_has_v3()) { cached = 0; return cached; }
   force = getenv("STBIR_CPU");
   if (force && (!strcmp(force, "base") || !strcmp(force, "sse2") || !strcmp(force, "scalar"))) {
      cached = 0;
   } else {
      cached = 1;
   }
   return cached;
#endif
}

/* AVX2 is exact for every datatype except half-float output (signed zero). */
#define AVX2_T(t)  (stbir__use_avx2() && ((t) != STBIR_TYPE_HALF_FLOAT))
#define AVX2_R(r)  (stbir__use_avx2() && ((r)->output_data_type != STBIR_TYPE_HALF_FLOAT))

#ifdef STBIR_DISPATCH_AVX2
#define PICK(cond, base_call, avx2_call) ((cond) ? (avx2_call) : (base_call))
#else
#define PICK(cond, base_call, avx2_call) (base_call)
#endif

/* ---- canonical entry points ---- */

unsigned char * stbir_resize_uint8_srgb( const unsigned char *a, int b, int c, int d,
   unsigned char *e, int f, int g, int h, stbir_pixel_layout i )
{ return PICK( stbir__use_avx2(), stbir_resize_uint8_srgb_base(a,b,c,d,e,f,g,h,i),
                                   stbir_resize_uint8_srgb_avx2(a,b,c,d,e,f,g,h,i) ); }

unsigned char * stbir_resize_uint8_linear( const unsigned char *a, int b, int c, int d,
   unsigned char *e, int f, int g, int h, stbir_pixel_layout i )
{ return PICK( stbir__use_avx2(), stbir_resize_uint8_linear_base(a,b,c,d,e,f,g,h,i),
                                   stbir_resize_uint8_linear_avx2(a,b,c,d,e,f,g,h,i) ); }

float * stbir_resize_float_linear( const float *a, int b, int c, int d,
   float *e, int f, int g, int h, stbir_pixel_layout i )
{ return PICK( stbir__use_avx2(), stbir_resize_float_linear_base(a,b,c,d,e,f,g,h,i),
                                   stbir_resize_float_linear_avx2(a,b,c,d,e,f,g,h,i) ); }

void * stbir_resize( const void *a, int b, int c, int d, void *e, int f, int g, int h,
   stbir_pixel_layout i, stbir_datatype j, stbir_edge k, stbir_filter l )
{ return PICK( AVX2_T(j), stbir_resize_base(a,b,c,d,e,f,g,h,i,j,k,l),
                         stbir_resize_avx2(a,b,c,d,e,f,g,h,i,j,k,l) ); }

void stbir_resize_init( STBIR_RESIZE *r, const void *a, int b, int c, int d,
   void *e, int f, int g, int h, stbir_pixel_layout i, stbir_datatype j )
{ PICK( AVX2_T(j), stbir_resize_init_base(r,a,b,c,d,e,f,g,h,i,j),
                    stbir_resize_init_avx2(r,a,b,c,d,e,f,g,h,i,j) ); }

void stbir_set_datatypes( STBIR_RESIZE *r, stbir_datatype a, stbir_datatype b )
{ PICK( AVX2_T(b), stbir_set_datatypes_base(r,a,b), stbir_set_datatypes_avx2(r,a,b) ); }

void stbir_set_pixel_callbacks( STBIR_RESIZE *r, stbir_input_callback *a, stbir_output_callback *b )
{ PICK( stbir__use_avx2(), stbir_set_pixel_callbacks_base(r,a,b), stbir_set_pixel_callbacks_avx2(r,a,b) ); }

void stbir_set_user_data( STBIR_RESIZE *r, void *a )
{ PICK( stbir__use_avx2(), stbir_set_user_data_base(r,a), stbir_set_user_data_avx2(r,a) ); }

void stbir_set_buffer_ptrs( STBIR_RESIZE *r, const void *a, int b, void *c, int d )
{ PICK( stbir__use_avx2(), stbir_set_buffer_ptrs_base(r,a,b,c,d), stbir_set_buffer_ptrs_avx2(r,a,b,c,d) ); }

int stbir_set_pixel_layouts( STBIR_RESIZE *r, stbir_pixel_layout a, stbir_pixel_layout b )
{ return PICK( stbir__use_avx2(), stbir_set_pixel_layouts_base(r,a,b), stbir_set_pixel_layouts_avx2(r,a,b) ); }

int stbir_set_edgemodes( STBIR_RESIZE *r, stbir_edge a, stbir_edge b )
{ return PICK( stbir__use_avx2(), stbir_set_edgemodes_base(r,a,b), stbir_set_edgemodes_avx2(r,a,b) ); }

int stbir_set_filters( STBIR_RESIZE *r, stbir_filter a, stbir_filter b )
{ return PICK( stbir__use_avx2(), stbir_set_filters_base(r,a,b), stbir_set_filters_avx2(r,a,b) ); }

int stbir_set_filter_callbacks( STBIR_RESIZE *r, stbir__kernel_callback *a, stbir__support_callback *b,
   stbir__kernel_callback *c, stbir__support_callback *d )
{ return PICK( stbir__use_avx2(), stbir_set_filter_callbacks_base(r,a,b,c,d), stbir_set_filter_callbacks_avx2(r,a,b,c,d) ); }

int stbir_set_pixel_subrect( STBIR_RESIZE *r, int a, int b, int c, int d )
{ return PICK( stbir__use_avx2(), stbir_set_pixel_subrect_base(r,a,b,c,d), stbir_set_pixel_subrect_avx2(r,a,b,c,d) ); }

int stbir_set_input_subrect( STBIR_RESIZE *r, double a, double b, double c, double d )
{ return PICK( stbir__use_avx2(), stbir_set_input_subrect_base(r,a,b,c,d), stbir_set_input_subrect_avx2(r,a,b,c,d) ); }

int stbir_set_output_pixel_subrect( STBIR_RESIZE *r, int a, int b, int c, int d )
{ return PICK( stbir__use_avx2(), stbir_set_output_pixel_subrect_base(r,a,b,c,d), stbir_set_output_pixel_subrect_avx2(r,a,b,c,d) ); }

int stbir_set_non_pm_alpha_speed_over_quality( STBIR_RESIZE *r, int a )
{ return PICK( stbir__use_avx2(), stbir_set_non_pm_alpha_speed_over_quality_base(r,a),
                                   stbir_set_non_pm_alpha_speed_over_quality_avx2(r,a) ); }

int stbir_build_samplers( STBIR_RESIZE *r )
{ return PICK( AVX2_R(r), stbir_build_samplers_base(r), stbir_build_samplers_avx2(r) ); }

void stbir_free_samplers( STBIR_RESIZE *r )
{ PICK( AVX2_R(r), stbir_free_samplers_base(r), stbir_free_samplers_avx2(r) ); }

int stbir_resize_extended( STBIR_RESIZE *r )
{ return PICK( AVX2_R(r), stbir_resize_extended_base(r), stbir_resize_extended_avx2(r) ); }

int stbir_build_samplers_with_splits( STBIR_RESIZE *r, int a )
{ return PICK( AVX2_R(r), stbir_build_samplers_with_splits_base(r,a), stbir_build_samplers_with_splits_avx2(r,a) ); }

int stbir_resize_extended_split( STBIR_RESIZE *r, int a, int b )
{ return PICK( AVX2_R(r), stbir_resize_extended_split_base(r,a,b), stbir_resize_extended_split_avx2(r,a,b) ); }
