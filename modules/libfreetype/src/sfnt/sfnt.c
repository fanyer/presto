/***************************************************************************/
/*                                                                         */
/*  sfnt.c                                                                 */
/*                                                                         */
/*    Single object library component.                                     */
/*                                                                         */
/*  Copyright 1996-2001, 2002, 2003, 2004, 2005, 2006 by                   */
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
#include "modules/libfreetype/src/sfnt/sfntpic.c"
#include "modules/libfreetype/src/sfnt/ttload.c"
#include "modules/libfreetype/src/sfnt/ttmtx.c"
#include "modules/libfreetype/src/sfnt/ttcmap.c"
#include "modules/libfreetype/src/sfnt/ttkern.c"
#include "modules/libfreetype/src/sfnt/sfobjs.c"
#include "modules/libfreetype/src/sfnt/sfdriver.c"

#ifdef TT_CONFIG_OPTION_EMBEDDED_BITMAPS
#include "modules/libfreetype/src/sfnt/ttsbit.c"
#endif

#ifdef TT_CONFIG_OPTION_POSTSCRIPT_NAMES
#include "modules/libfreetype/src/sfnt/ttpost.c"
#endif

#ifdef TT_CONFIG_OPTION_BDF
#include "modules/libfreetype/src/sfnt/ttbdf.c"
#endif

/* END */
