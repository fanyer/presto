/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#include "core/pch.h"

#ifdef WEBP_SUPPORT

#ifndef HAS_COMPLEX_GLOBALS
# error SYSTEM_COMPLEX_GLOBALS is required for WebP support.
#endif

// Less intrusive, and this file is no-jumbo.
//
// Simply doing
//
// #undef memcmp
// #define memcmp op_memcmp
//
// apparently causes problems for Bream, hence the helper
// functions.
static inline int memcmp_wrap(const void *a, const void *b, size_t c) { return op_memcmp(a, b, c); }
# undef  memcmp
# define memcmp memcmp_wrap

static inline void *memcpy_wrap(void *a, const void *b, size_t c) { return op_memcpy(a, b, c); }
# undef  memcpy
# define memcpy memcpy_wrap

static inline void *memset_wrap(void *a, int b, size_t c) { return op_memset(a, b, c); }
# undef  memset
# define memset memset_wrap

static inline void *memmove_wrap(void *dest, const void *src, size_t n) { return op_memmove(dest, src, n); }
# undef  memmove
# define memmove memmove_wrap

# undef  assert
# define assert OP_ASSERT

# undef  uintptr_t
# define uintptr_t UINTPTR

# include "modules/webp/lib/webp/types.h"
# include "modules/webp/lib/dec/vp8i.h"

#  define vp8_malloc op_malloc
#  define vp8_calloc op_calloc
#  define vp8_free   op_free

#  define vp8_log    op_log
#  define vp8_abs    op_abs

// webp source files go here
# include "modules/webp/lib/dec/alpha.c"
# include "modules/webp/lib/dec/buffer.c"
# include "modules/webp/lib/dec/frame.c"
# include "modules/webp/lib/dec/idec.c"
# include "modules/webp/lib/dec/io.c"
# include "modules/webp/lib/dec/layer.c"
# include "modules/webp/lib/dec/quant.c"
# include "modules/webp/lib/dec/tree.c"
# include "modules/webp/lib/dec/vp8.c"
# include "modules/webp/lib/dec/vp8l.c"
# include "modules/webp/lib/dec/webp.c"

# include "modules/webp/lib/dsp/dec.c"
# include "modules/webp/lib/dsp/upsampling.c"
# include "modules/webp/lib/dsp/yuv.c"
# include "modules/webp/lib/dsp/lossless.c"

#include "modules/webp/lib/utils/bit_reader.c"
#include "modules/webp/lib/utils/color_cache.c"
#include "modules/webp/lib/utils/filters.c"
#include "modules/webp/lib/utils/huffman.c"
#include "modules/webp/lib/utils/rescaler.c"
#include "modules/webp/lib/utils/thread.c"
#include "modules/webp/lib/utils/quant_levels.c"
#include "modules/webp/lib/utils/utils.c"

#endif // WEBP_SUPPORT
