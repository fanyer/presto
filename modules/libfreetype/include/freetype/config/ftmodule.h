/*
 *  This file registers the FreeType modules compiled into the library.
 *
 *  If you use GNU make, this file IS NOT USED!  Instead, it is created in
 *  the objects directory (normally `<topdir>/objs/') based on information
 *  from `<topdir>/modules.cfg'.
 *
 *  Please read `docs/INSTALL.ANY' and `docs/CUSTOMIZE' how to compile
 *  FreeType without GNU make.
 *
 */

/*
 * WONKO: removed include-protection that i introduced earlier - this
 * file is deliberately included several times from several places,
 * and it's hard to notice where when upgrading.
 */

#include "modules/libfreetype/include/freetype/config/opera_ftmodules.h"

#ifdef FT_USE_AUTOFIT
FT_USE_MODULE( FT_Module_Class, autofit_module_class )
#endif // FT_USE_AUTOFIT
#ifdef FT_USE_TT_FONTS
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
#endif // FT_USE_TT_FONTS
#ifdef FT_USE_T1_FONTS
FT_USE_MODULE( FT_Driver_ClassRec, t1_driver_class )
#endif // FT_USE_T1_FONTS
#ifdef FT_USE_CFF_FONTS
FT_USE_MODULE( FT_Driver_ClassRec, cff_driver_class )
#endif // FT_USE_CFF_FONTS
#ifdef FT_USE_T1CID_FONTS
FT_USE_MODULE( FT_Driver_ClassRec, t1cid_driver_class )
#endif // FT_USE_T1CID_FONTS
#ifdef FT_USE_PFR_FONTS
FT_USE_MODULE( FT_Driver_ClassRec, pfr_driver_class )
#endif // FT_USE_PFR_FONTS
#ifdef FT_USE_T42_FONTS
FT_USE_MODULE( FT_Driver_ClassRec, t42_driver_class )
#endif // FT_USE_T42_FONTS
#ifdef FT_USE_WINFONT_FONTS
FT_USE_MODULE( FT_Driver_ClassRec, winfnt_driver_class )
#endif // FT_USE_WINFONT_FONTS
#ifdef FT_USE_PCF_FONTS
FT_USE_MODULE( FT_Driver_ClassRec, pcf_driver_class )
#endif // FT_USE_PCF_FONTS
#ifdef FT_USE_PSAUX
FT_USE_MODULE( FT_Module_Class, psaux_module_class )
#endif // FT_USE_PSAUX
#ifdef FT_USE_PSNAMES
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
#endif // FT_USE_PSNAMES
#ifdef FT_USE_PSHINTER
FT_USE_MODULE( FT_Module_Class, pshinter_module_class )
#endif // FT_USE_PSHINTER
#ifdef FT_USE_RASTER1
FT_USE_MODULE( FT_Renderer_Class, ft_raster1_renderer_class )
#endif // FT_USE_RASTER1
#ifdef FT_USE_SFNT
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
#endif // FT_USE_SFNT
#ifndef MDE_DISABLE_FONT_ANTIALIASING
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )
#endif // MDE_DISABLE_FONT_ANTIALIASING
#ifdef FT_USE_SMOOTH_LCD_RENDERING
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_lcd_renderer_class )
#endif // FT_USE_SMOOTH_LCD_RENDERING
#ifdef FT_USE_SMOOTH_LCDV_RENDERING
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_lcdv_renderer_class )
#endif // FT_USE_SMOOTH_LCDV_RENDERING
#ifdef FT_USE_BDF_FONTS
FT_USE_MODULE( FT_Driver_ClassRec, bdf_driver_class )
#endif // FT_USE_BDF_FONTS

/* EOF */
