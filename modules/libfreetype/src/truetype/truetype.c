/***************************************************************************/
/*                                                                         */
/*  truetype.c                                                             */
/*                                                                         */
/*    FreeType TrueType driver component (body only).                      */
/*                                                                         */
/*  Copyright 1996-2001, 2004, 2006 by                                     */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


#define FT_MAKE_OPTION_SINGLE_OBJECT

#include "modules/libfreetype/include/ft2build.h"
#include "modules/libfreetype/src/truetype/ttpic.c"
#include "modules/libfreetype/src/truetype/ttdriver.c"   /* driver interface    */
#include "modules/libfreetype/src/truetype/ttpload.c"    /* tables loader       */
#include "modules/libfreetype/src/truetype/ttgload.c"    /* glyph loader        */
#include "modules/libfreetype/src/truetype/ttobjs.c"     /* object manager      */

#ifdef TT_USE_BYTECODE_INTERPRETER
#include "modules/libfreetype/src/truetype/ttinterp.c"
#endif

#ifdef TT_CONFIG_OPTION_GX_VAR_SUPPORT
#include "modules/libfreetype/src/truetype/ttgxvar.c"    /* gx distortable font */
#endif


/* END */
