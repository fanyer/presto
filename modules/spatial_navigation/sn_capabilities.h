/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file sn_capabilities.h
 *
 * This file contains the capabilities exported by the spatial_navigation module.
 *
 * @author h2@opera.com
*/

#ifndef SN_CAPABILITIES_H
#define SN_CAPABILITIES_H

#define SN_CAP_FORM_FOCUSING
#define SN_CAP_SCROLL_PAGE
#define SN_CAP_ON_SCROLL
#define SN_CAP_ON_LOADING_FINISHED
#define SN_CAP_NAVFILTER
#define SN_CAP_SCROLL_PARAMETER
#define SN_CAP_ELEMENT_TYPE_API
#define SN_CAP_FORGET_DELETED_ELEMENTS
#define SN_CAP_EXTENDED_ELEMENT_TYPE_API
#define SN_CAP_HANDLES_ACTION
#define SN_CAP_ELEMENT_DESTROYED

// in magellan_3 spatnav stopped caching all links
#define SN_CAP_NO_LINK_CACHE
// in magellan_3 spatnav doesn't have platform specific handlers anymore
#define SN_CAP_NO_PLATFORM_HANDLERS

// This module includes layout/layout_workplace.h where needed, instead of relying on doc/frm_doc.h doing it.
#define SN_CAP_INCLUDE_CSSPROPS

// Module uses GetProps() instead of accessing .props directly
#define SN_CAP_USE_LAYOUTPROPERTIES_GETPROPS

// Doesn't call HTML_Document::GetSubWinId anymore. yay.
#define SN_CAP_NO_HTML_DOC_SUBWINID_CALLING

// Not calling HTML_Document::SetFocus(), calling FramesDocument::SetFocus()
#define SN_CAP_CALLING_FRM_DOC_SET_FOCUS

#endif // SN_CAPABILITIES_H
