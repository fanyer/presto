/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DISPLAY_CAPABILITIES_H
#define DISPLAY_CAPABILITIES_H

/**
 * This capability signals that DecorationLineOut exists in
 * VisualDevice as a complement to LineOut. DecorationLineOut should
 * be used for underlining and similar lines.
 */
#define DISPLAY_CAP_DECORATION_LINE_OUT

/**
 * Core use CoreView, a platformindependent replacement for OpView.
*/
#define DISPLAY_CAP_COREVIEW

/** Has ApplyScaleRoundingNearestUp */
#define DISPLAY_CAP_SCALEROUNDING_NEARESTUP

/** Has DrawHighlightRect and UpdateHighlightRect */
#define DISPLAY_CAP_NEW_HIGHLIGHTING

/** Use GetNextTextFragment() with length argument */
#define DISPLAY_CAP_GET_NEXT_TEXT_FRAGMENT_LENGTH

/** Has the MouseListenerClickInfo::SetTitleElement() method */
#define DISPLAY_CAP_HAS_SETTITLEELEMENT

/** VisualDevice has public TransformText method */
#define DISPLAY_CAP_VISDEV_HAS_PUBLIC_TRANSFORMTEXT

/** The methods that return FontInfo is not const */
#define DISPLAY_CAP_FONTINFO_NOT_CONST

/** Has negative overflow API (in VisualDevice::SetDocumentSize, VisualDevice:SetScrollbars */
#define DISPLAY_CAP_NEGATIVE_OVERFLOW

/** Has #ifdefs enabling it to use the global selection_scroll_active from the doc_module */
#define DISPLAY_CAP_USES_MOVED_GLOBALS

/** Has SetPluginFixedPosition */
#define DISPLAY_CAP_FIXEDPOSITION_PLUGINS

/** Has CoreView::GetIntersectingChild */
#define DISPLAY_CAP_GETINTERSECTINGCHILD

/** Has StyleManager methods: UseSVGFonts, SetSVGFonts and GetSVGFontManager */
#define DISPLAY_CAP_SVGFONTS

/** The FontCache can handle SVGFonts **/
#define DISPLAY_CAP_SVGFONTS_IN_FONTCACHE

/** Support for internal text-shaper (to be used with e.g. the Arabic script) */
#define DISPLAY_CAP_TEXTSHAPER

/** Has penstyle parameter in rectangleout */
#define DISPLAY_CAP_RECTANGLEOUT_PENSTYLE

/** Has ScrollRegion (VisualDevice::AddFixedContent exists) */
#define DISPLAY_CAP_SCROLL_REGION

/** FontAtt knows how to serialize itself */
#define DISPLAY_CAP_FONTATT_SERIALIZATION

/** the m_charSet member is gone from FontAtt */
#define DISPLAY_CAP_FONTATT_NO_CHARSET

/** Has precise scroll position functions */
#define DISPLAY_CAP_PRECISE_POS

/** Has transparent parameter in VisualDevice::GetNewPluginWindow */
#define DISPLAY_CAP_NEWPLUGIN_TRANSPARENT

/** Has VisualDevice::RestorePage */
#define DISPLAY_CAP_RESTOREPAGE

/** ImageOutEffect() supports a ImageListener argument (like ImageOut() does) */
#define DISPLAY_CAP_IMAGEOUTEFFECT_WITH_LISTENER	

/** Has CoreView::SetReference */
#define DISPLAY_CAP_COREVIEW_REFERENCE_ELEMENT

/** Has VisualDevice::BeginPaintLock and EndPaintLock */
#define DISPLAY_CAP_PAINTLOCK

/** Makes sure the SVGManager isn't called with the same action more than once */
#define DISPLAY_CAP_HANDLES_SVGCONTENT_AS_INPUTCONTEXT

/** FontInfo has CodePageFromScript function */
#define DISPLAY_CAP_CODEPAGEFROMSCRIPT 

/** Uses GetNextTextFramgnent which takes Codepage argument */
#define DISPLAY_CAP_USES_GNTF_WITH_CODEPAGE

/** Has VisualDevice::BeginPaint */
#define DISPLAY_CAP_BEGINPAINT

/** Codepage preference can be negative to dislike certain fonts for a codepage 
	(Arial Unicode MS is a rather complete but very ugly font for CJK, bug 185062) */ 
#define DISPLAY_CAP_NEGATIVE_CODEPAGE_PREFERENCE

/** Has VisualDevice::DrawSelection */
#define DISPLAY_CAP_DRAWSELECTION

/** Has VisualDevice::PushState/PopState */
#define DISPLAY_CAP_VD_STATE

/** Has parameter update_pluginwindow in ResizePluginWindow */
#define DISPLAY_CAP_RESIZEPLUGIN_UPDATE

/** Won't handle middle button clicks if logdoc does. */
#define DISPLAY_CAP_LOGDOC_HANDLES_MIDCLICK

/** VisualDevice can do text-shaping (needed for e.g. Arabic script on some
 * systems) if told to do so.
 */
#define DISPLAY_CAP_TEXTSHAPING

/** StyleManager wants to get called by prefsmanager when the latter has been initialised */
#define DISPLAY_CAP_STYLEMAN_PREFSLISTENER

/** ImageOutTiled has imgscale_x, imgscale_y */
#define DISPLAY_CAP_TILE_X_Y_FACTOR

/** Added OnFontInfo::OP_KOREAN_CODEPAGE and OpFontInfo::OP_CYRILLIC_CODEPAGE, to support fontswitching in korean and cyrillic pages */
#define DISPLAY_CAP_KOREAN_AND_CYRILLIC_CODEPAGE

/** Has DrawHighlightRects */
#define DISPLAY_CAP_DRAWHIGHLIGHTRECTS

/** CoreView::Paint has parameter paint_containers */
#define DISPLAY_CAP_PAINTCONTAINERS

/** VisualDeviceBackBuffer::BeginBackBuffer has copy_background parameter */
#define DISPLAY_CAP_VISDEVBB_HAS_COPY_BACKGROUND

#define DISPLAY_CAP_NOSTEP_SCROLL_WHEN_ZOOMED

/** Has struct BG_OUT_INFO */
#define DISPLAY_CAP_HAS_BG_OUT_INFO

/** Has VisualDevice::BeginEffect */
#define DISPLAY_CAP_HAS_BEGIN_EFFECT

/** Has VisualDeviceScrollListener */
#define DISPLAY_CAP_HAS_NEW_SCROLL_LISTENER

/** Has VisualDevice::SetTextScale && VisualDevice::GetTextScale */
#define DISPLAY_CAP_TEXT_SCALE

/** Has VisualDevice::DrawCaret */
#define DISPLAY_DRAWCARET

/** adaptive zoom can pan in both directions simultaneously */
#define DISPLAY_CAP_ADAPTIVE_ZOOM_PAN_XY_IN_ONE_CALL

/** Has OpFontInfo::MergeFontInfo */
#define DISPLAY_CAP_FONTINFO_HAS_MERGE

/** the text shaper can provide one shaped character at a time with TextShaper::GetShapedChar */
#define DISPLAY_CAP_FONTSWITCHING_THROUGH_TEXTSHAPER

/** Has replaced loads of Document pointers with FramesDocument pointers. */
#define DISPLAY_CAP_DOCUMENT_CLEANUP

/** The screeninfo-getters are not static members of VisualDevice anymore */
#define DISPLAY_CAP_NONSTATIC_SCREENINFO

/** Make sure the old version of SwitchFont (in layout) is declared static */
#define DISPLAY_CAP_SWITCHFONT_TAKES_STRING

/** Don't use const OpBitmap objects with GetPointer() / ReleasePointer(). */
#define DISPLAY_CAP_BITMAPPTR_ACCESS

/** visual device can provide per-glyph props for the current font. */
#define DISPLAY_CAP_HAS_PER_GLYPH_PROPS

/** Expose TextShaper::Get{Isolated,Initial,Medial,Final}Form. */
#define DISPLAY_CAP_EXPOSE_GLYPHSHAPING

/** VisualDevice now handles outline */
#define DISPLAY_CAP_POLYGON_OUTLINE

/** VisualDevice has SetContentFound. */
#define DISPLAY_CAP_SETCONTENTFOUND

/** visual device positions and draws diaritics, performing font switching for diacritical marks when necessary */
#define DISPLAY_CAP_HANDLES_DIACRITICS

/** visual device can be called from opwidget when generating mouse down, so panning can decide if it should be used on the widget */
#define DISPLAY_CAP_PAN_HOOKED_WIDGET

/** style manager can set preferred font for the relevant codepages given a unicode block number */
#define DISPLAY_CAP_SET_PREFERRED_FONT_FOR_CODEPAGE

/** Has a bugfix in ForcePendingPaintRect so the pending rect is remembered in OnBeforePaint. */
#define DISPLAY_CAP_HAS_FORCEPENDINGRECT_FIX

/** Has HTML_Element parameter in BeginOutline */
#define DISPLAY_CAP_BEGIN_OUTLINE_TAKES_ELEMENT

/** This module includes layout/cssprops.h and layout/layout_workplace.h where needed, instead of relying on header files in other modules doing it. */
#define DISPLAY_CAP_INCLUDE_CSSPROPS

/** VisualDevice has OnReflow */
#define DISPLAY_CAP_VD_HAS_ONREFLOW

/** VisualDevice handles resizing of iframes */
#define DISPLAY_CAP_FRAMES_SIZE_FIX

/** HTML_Element::HandleEvent takes care of some double click actions that
	used to be done in display before scripts had their say */
#define DISPLAY_CAP_NO_DBLCLICK_ACTIONS

/** Has scaled scroll (and delays scrolling until after scaling) */
#define DISPLAY_CAP_SCALED_SCROLL

#define DISPLAY_CAP_HAS_PER_GLYPH_PROPS ///< we can fetch glyph props if everyone else can

/** BeforePaint returns FALSE if it yielded. */
#define DISPLAY_CAP_BEFOREPAINT_RETURNS

/** VisualDevice has SetPosition that only set position. */
#define DISPLAY_CAP_SETPOSITION_SIMPLIFIED

/** VisualDevice has ResetKeepScrollbars, and AllowVScrollOff is deprecated. */
#define DISPLAY_CAP_RESET_KEEP_SCROLLBARS

/** CoreView has SetWantMouseEvents, GetWantMouseEvents */
#define DISPLAY_CAP_COREVIEW_HAS_SETWANTMOUSE

/** The display module can satisfy the requirements of the plug-in event API.
	May be used to toggle import of API_PI_USE_PLUGIN_EVENT_API. */
#define DISPLAY_CAP_PLUGIN_EVENT_API

/** VisualDevice has IsPaintingToScreen */
#define DISPLAY_CAP_IS_PAINTING_TO_SCREEN

/** VisualDevice has the SupportsAlpha function which reports if the visual 
    device can handle alpha or not. */
#define DISPLAY_CAP_VD_SUPPORTS_ALPHA

/** Supports all cursor types from the CSS3 Basic User Interface Module. */
#define DISPLAY_CAP_CSS3_CURSOR_TYPES

/** Has OpFontInfo::Type definition */
#define DISPLAY_CAP_HAS_FONT_TYPES

/** Has VisualDevice::SetVisible returns OP_STATUS */
#define DISPLAY_CAP_SETVISIBLE_RETURN_STATUS

/**
   if opentype support is enabled, text shaper doesn't
   change order of devanagari dependent vowel sign i, and
   TxtOut_Diacritics is not used to draw indic diacritics
 */
#define DISPLAY_CAP_INDIC_AWARE_TEXT_PROCESSING

/** VisualDevice::DrawMisspelling exists */
#define DISPLAY_CAP_DRAW_MISSPELLING

/** The display module offer OpRect VisualDevice::VisibleRect() */
#define DISPLAY_CAP_VISDEV_VISIBLERECT

/** Has new methods needed for ScrollableContainer to become a CoreView. */
#define DISPLAY_CAP_COREVIEW_CLIPPING_FIX

/** Has CoreView::SetIsLayoutBox */
#define DISPLAY_CAP_COREVIEW_LAYOUTBOX

/** Has VisualDevice::GetFontMetrics */
#define DISPLAY_CAP_GETFONTMETRICS

/** Has VisualDevice has AttachPluginCoreView and DetachPluginCoreView */
#define DISPLAY_CAP_ATTACH_DETACH_PLUGIN_COREVIEW

/** Has WebFontManager::AddCSSWebFont with FramesDocument */
#define DISPLAY_CAP_HAS_NEW_ADDCSSWEBFONT

/** display module has AddLocalizedFontName and GetWesternFontName */
#define DISPLAY_CAP_LOCALIZED_FONT_NAME_HASH_FROM_FONT

/** VisualDevice has BeginAccurateFontSize */
#define DISPLAY_CAP_BEGINACCURATEFONTSIZE

/** VisualDevice::ImageAltOut takes preferred codepage argument to support fontswitching. */
#define DISPLAY_CAP_IMAGEALTOUT_WITH_CODEPAGE

/** StyleManager has LookupFontNumber method */
#define DISPLAY_CAP_HAS_LOOKUP_FONT_NUMBER

/** VisualDevice::ImageAltOut takes preferred codepage argument to support fontswitching. */
#define DISPLAY_CAP_IMAGEALTOUT_WITH_CODEPAGE

/** StyleManager has CreateFontNumber */
#define DISPLAY_CAP_HAS_CREATE_FONT_NUMBER

/** VisualDevice has GetTxtExtentSplit */
#define DISPLAY_CAP_HAS_GETTXTEXTENTSPLIT

/** VisualDevice has extra parameters to DrawHighlightRects and is using the outline system for highlight painting */
#define DISPLAY_CAP_HAS_OUTLINE_HIGHLIGHT

/** Supports ASCII key strings for actions/menus/dialogs/skin */
#define DISPLAY_CAP_INI_KEYS_ASCII

/** VisualDevice::CheckCoreViewVisibility */
#define DISPLAY_CAP_COREVIEW_VISIBILITY

/** CoreView::Paint has clip_to_view */
#define DISPLAY_CAP_COREVIEW_PARAM_CLIPVIEW

/** CoreView::BeginOutline has offset parameter */
#define DISPLAY_CAP_BEGIN_OUTLINE_TAKES_OFFSET

/** PresentationAttr contains a list of PresentationFonts (one for
 * each script) instead of one FontAtt. API:s changed accordingly */
#define DISPLAY_CAP_MULTISTYLE

/** VisualDevice has PaintBorderImage */
#define DISPLAY_CAP_BORDERIMAGE

/** BG_OUT_INFO has border_image */
#define DISPLAY_CAP_BGOUTINFO_HAS_BORDERIMAGE

/** New context menu system which means that display does much less and doc/logdoc much more. */
#define DISPLAY_CAP_ONCONTEXTMENU

/** VDSpotlight has margin, border and padding properties */
#define DISPLAY_CAP_SPOTLIGHT_PROPS

/** VisualDevice::SetRenderingViewPos() takes an OpRect parameter that
	specifies the area that has already been scheduled for repaint. */
#define DISPLAY_CAP_SETRENDERINGVIEWPOS_TAKES_AREA

/**
   VisualDevice::ImageAltOut takes WritingSystem::Script instead of OpFontInfo::Codepage
 */
#define DISPLAY_CAP_IMAGEALTOUT_SCRIPT

/** VDSpotlight has SetProps */
#define DISPLAY_SPOTLIGHT_HASSETPROPS

/**
   StyleManager::GetGenericFontNumber exists
 */
#define DISPLAY_CAP_HAS_GETGENERICFONTNUMBER

/**
   StyleManager tries to figure out fallbacks for when passed a
   non-existing font name as a generic font.
 */
#define DISPLAY_CAP_GENERIC_FONT_FALLBACK

/**
   allows setting a different layout scale for truezoom
 */
#define DISPLAY_CAP_TRUEZOOM_LAYOUT_SCALE

/**
   TxtOut* takes optional argument word_width, used to adjust string width when truezoom is enabled
 */
#define DISPLAY_CAP_LINEAR_TEXT_SCALE

/** Has new VisualDevice::BeginPaint */
#define DISPLAY_CAP_BEGINPAINT2

/** display contains WritingSystem and provides heuristic to detect writing system from text */
#define DISPLAY_CAP_WRITING_SYSTEM_HEURISTIC

/** PaintBorderImage takes ImageEffect */
#define DISPLAY_CAP_BORDER_IMG_EFFECT

/** VisualDevice has DrawTextBgHighlight */
#define DISPLAY_CAP_TEXT_BG_HIGHLIGHT

/** Has CheckColorContrast method */
#define DISPLAY_CAP_HAS_CHECKCONTRAST

/** Uses UnicodePoint to reference code points */
#define DISPLAY_CAP_UNICODE_POINT_SUPPORT

/** VisualDevice has IsPaintingScaled, BeginScaledPainting and EndScaledPainting */
#define DISPLAY_CAP_HAS_SCALED_PAINTING

/**
   There is versions of BgImgOut, ImageOutTiled and BlitImageTiled
   that takes extra parameters to add space between tiled images.
 */
#define DISPLAY_CAP_BGIMGOUT_HAS_SPACE

/** VisualDevice has a ClearRect function. */
#define DISPLAY_CAP_VD_CLEAR_RECT

/** VisualDevice has DrawFocusRect */
#define DISPLAY_CAP_HAS_DRAW_FOCUS_RECT

/** The DISPLAY module extracts codepage info from a character set identifier */
#define DISPLAY_CAP_EXTRACTCODEPAGE

#endif // DISPLAY_CAPABILITIES_H
