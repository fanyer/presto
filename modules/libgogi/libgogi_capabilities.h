/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LIBGOGI_CAPABILITIES_H
#define LIBGOGI_CAPABILITIES_H

#define LIBGOGI_CAP_HAS_INTERNAL_LEADING // MDE_FONT has internal_leading
#define LIBGOGI_CAP_BLIT_METHOD_SUPPORTED
#define LIBGOGI_CAP_HAS_SETTRANSPARENT // has MDE_View::SetTransparent instead of virtual IsTransparent
#define LIBGOGI_CAP_HAS_WHEEL_VERTICAL
#define LIBGOGI_CAP_HAS_SPRITEVIEWS // Sprites can have a view which will decide where it should be visible.
#define LIBGOGI_CAP_HAS_MODULESOURCES
#define LIBGOGI_CAP_HAS_METHOD_OPACITY // Has MDE_METHOD_OPACITY
#define LIBGOGI_CAP_HAS_THICK_LINES // Has MDE_DrawLineThick and MDE_DrawEllipseThick
#define LIBGOGI_CAP_HAS_ONBEFOREPAINT // Has OpView::OnBeforePaint
#define LIBGOGI_CAP_HAS_IGNOREIFHIDDEN // Has ignore_if_hidden parameter in Invalidate
#define LIBGOGI_CAP_HAS_ONPAINTEX // Has MDE_View::OnPaintEx
#define LIBGOGI_CAP_HAS_ONBEFOREPAINTEX // Has MDE_View::OnBeforePaintEx
#define LIBGOGI_CAP_HAS_ONBEFORERECTPAINT // Has MDE_Screen::OnBeforeRectPaint

#define LIBGOGI_CAP_PLATFORM_FONT_IMPLEMENTATIONS_MOVED // The platform specific font implementations have been moved to the platform code

#define LIBGOGI_CAP_HAS_PER_GLYPH_PROPS ///< we can fetch glyph props

#define LIBGOGI_CAP_ALPHA_SET_COLOR // SetColor can handle alpha colors
#define LIBGOGI_CAP_WEBFONTS // Has MDE_RemoveFont, and can manage non-sequential fontnumbers

#define LIBGOGI_CAP_INDIC_SCRIPTS ///< has g_indic_scripts and is_indic

#define LIBGOGI_CAP_HAS_PALPHA ///< Has MDE_METHOD_PALPHA

#define LIBGOGI_CAP_ONCURSORCHANGED ///< Has MDE_OpWindow::OnCursorChanged

#define LIBGOGI_CAP_MMAP_INTERFACE ///< Has an interface for mmap which the platform can implement

#define LIBGOGI_CAP_INDIC_GRAPHEME_CLUSTER_DETERMINATION ///< has IndicGlyphClass::IsGraphemeClusterBoundary

/**
 * MDE_GenericScreen has a method Init() - see bug#361578
 */
#define LIBGOGI_CAP_MDE_GENERIC_SCREEN_WITH_INIT

#define LIBGOGI_NO_OPFONTMANAGER_IS_API ///< The tweak TWEAK_LIBGOGI_NO_OPFONTMANAGER is changed to a API

#define LIBGOGI_CAP_MULTISTYLE_AWARE

#define LIBGOGI_CAP_STRETCH_METHOD ///< Has extra field stretch_method in MDE_BUFFER_INFO when TWEAK_LIBGOGI_BILINEAR_BLITSTRETCH is enabled

#endif // !LIBGOGI_CAPABILITIES_H
