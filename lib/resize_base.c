/* Baseline (SSE2 on x86-64; scalar elsewhere) variant of stb_image_resize2.
 * Public symbols are renamed to stbir_*_base; resize_dispatch.c provides the
 * canonical API. */
#define RESIZE_SUFFIX _base
#include "resize_rename.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
