/* AVX2 variant of stb_image_resize2. Compiled with -march=x86-64-v3 and
 * selected at runtime only on CPUs that support the full v3 feature set.
 * Public symbols are renamed to stbir_*_avx2. */
#define RESIZE_SUFFIX _avx2
#include "resize_rename.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
