/***************************************************************************/
/*                                                                         */
/*  ftbase.c                                                               */
/*                                                                         */
/*    Single object library component (body only).                         */
/*                                                                         */
/*  Copyright 1996-2001, 2002, 2003, 2004, 2006, 2007, 2008, 2009 by       */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


#include "modules/libfreetype/include/ft2build.h"

#define  FT_MAKE_OPTION_SINGLE_OBJECT

#include "modules/libfreetype/src/base/ftpic.c"
#include "modules/libfreetype/src/base/basepic.c"
#include "modules/libfreetype/src/base/ftadvanc.c"
#include "modules/libfreetype/src/base/ftcalc.c"
#include "modules/libfreetype/src/base/ftdbgmem.c"
#include "modules/libfreetype/src/base/ftgloadr.c"
#include "modules/libfreetype/src/base/ftobjs.c"
#include "modules/libfreetype/src/base/ftoutln.c"
#include "modules/libfreetype/src/base/ftrfork.c"
#include "modules/libfreetype/src/base/ftsnames.c"
#include "modules/libfreetype/src/base/ftstream.c"
#include "modules/libfreetype/src/base/fttrigon.c"
#include "modules/libfreetype/src/base/ftutil.c"

#if defined( FT_MACINTOSH ) && !defined ( DARWIN_NO_CARBON )
#include "modules/libfreetype/src/base/ftmac.c"
#endif

/* END */
