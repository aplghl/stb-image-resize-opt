/* Rename the public stb_image_resize2 symbols for an ISA variant build.
 *
 * Define RESIZE_SUFFIX (e.g. _base or _avx2) before including this header, then
 * define STB_IMAGE_RESIZE_IMPLEMENTATION and include stb_image_resize2.h. The
 * object then exports stbir_*_base / stbir_*_avx2 instead of the canonical
 * names, so several variants can be linked together. resize_dispatch.c
 * provides the canonical names and selects a variant at runtime.
 *
 * Keep this list in sync with the STBIRDEF public functions in the header.
 */
#ifdef RESIZE_SUFFIX
#define RESIZE_CAT2_(a, b) a##b
#define RESIZE_CAT_(a, b) RESIZE_CAT2_(a, b)
#define RESIZE_RN_(n) RESIZE_CAT_(n, RESIZE_SUFFIX)

#define stbir_resize_uint8_srgb                       RESIZE_RN_(stbir_resize_uint8_srgb)
#define stbir_resize_uint8_linear                     RESIZE_RN_(stbir_resize_uint8_linear)
#define stbir_resize_float_linear                     RESIZE_RN_(stbir_resize_float_linear)
#define stbir_resize                                  RESIZE_RN_(stbir_resize)
#define stbir_resize_init                             RESIZE_RN_(stbir_resize_init)
#define stbir_set_datatypes                           RESIZE_RN_(stbir_set_datatypes)
#define stbir_set_pixel_callbacks                     RESIZE_RN_(stbir_set_pixel_callbacks)
#define stbir_set_user_data                           RESIZE_RN_(stbir_set_user_data)
#define stbir_set_buffer_ptrs                         RESIZE_RN_(stbir_set_buffer_ptrs)
#define stbir_set_pixel_layouts                       RESIZE_RN_(stbir_set_pixel_layouts)
#define stbir_set_edgemodes                           RESIZE_RN_(stbir_set_edgemodes)
#define stbir_set_filters                             RESIZE_RN_(stbir_set_filters)
#define stbir_set_filter_callbacks                    RESIZE_RN_(stbir_set_filter_callbacks)
#define stbir_set_pixel_subrect                       RESIZE_RN_(stbir_set_pixel_subrect)
#define stbir_set_input_subrect                       RESIZE_RN_(stbir_set_input_subrect)
#define stbir_set_output_pixel_subrect                RESIZE_RN_(stbir_set_output_pixel_subrect)
#define stbir_set_non_pm_alpha_speed_over_quality     RESIZE_RN_(stbir_set_non_pm_alpha_speed_over_quality)
#define stbir_build_samplers                          RESIZE_RN_(stbir_build_samplers)
#define stbir_free_samplers                           RESIZE_RN_(stbir_free_samplers)
#define stbir_resize_extended                         RESIZE_RN_(stbir_resize_extended)
#define stbir_build_samplers_with_splits              RESIZE_RN_(stbir_build_samplers_with_splits)
#define stbir_resize_extended_split                   RESIZE_RN_(stbir_resize_extended_split)
#else
#error "define RESIZE_SUFFIX (e.g. _base or _avx2) before including resize_rename.h"
#endif
