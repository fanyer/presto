/***************************************************************************/
/*                                                                         */
/*  autofit.c                                                              */
/*                                                                         */
/*    Auto-fitter module (body).                                           */
/*                                                                         */
/*  Copyright 2003-2007, 2011 by                                           */
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
#include "modules/libfreetype/src/autofit/afpic.c"
#include "modules/libfreetype/src/autofit/afangles.c"
#include "modules/libfreetype/src/autofit/afglobal.c"
#include "modules/libfreetype/src/autofit/afhints.c"

#include "modules/libfreetype/src/autofit/afdummy.c"
#include "modules/libfreetype/src/autofit/aflatin.c"
#ifdef FT_OPTION_AUTOFIT2
#include "modules/libfreetype/src/autofit/aflatin2.c"
#endif
#include "modules/libfreetype/src/autofit/afcjk.c"
#include "modules/libfreetype/src/autofit/afindic.c"

#include "modules/libfreetype/src/autofit/afloader.c"
#include "modules/libfreetype/src/autofit/afmodule.c"

#ifdef AF_CONFIG_OPTION_USE_WARPER
#include "modules/libfreetype/src/autofit/afwarp.c"
#endif

/* END */
