/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef QUICKTK_CAPABILITIES_H
#define QUICKTK_CAPABILITIES_H

#define QUICKTK_CAP_HAS_STATEBUTTON
///< Has OpStateButton and related classes (WidgetStateListener, WidgetStateModifier, WidgetState)

#define QUICKTK_CAP_HAS_DIALOG_PAGE_COUNT
///< Dialog has GetPageCount

#define QUICKTK_CAP_HAS_MODIFIER_TYPE
///< WidgetStateModifier has Type (for selftests/debug checks)

#define QUICK_TOOLKIT_CAP_HAS_PAGE_VIEW
///< The OpPageView and OpPage has been split out of OpBrowserView

#endif // QUICKTK_CAPABILITIES_H
