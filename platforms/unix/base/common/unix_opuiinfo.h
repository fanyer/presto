/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef UNIX_OPUIINFO_H
#define UNIX_OPUIINFO_H

#include "adjunct/desktop_pi/DesktopOpUiInfo.h"

class UnixOpUiInfo : public DesktopOpUiInfo
{
	/* Commented-out methods are defined in OpUiInfo but not implemented here;
	 * derived classes should implement them, since they're pure virtual.
	 */
protected:
	friend class UnixOpFactory;
	UnixOpUiInfo() {} // may need attention later ...

public:
	// virtual ~UnixOpUiInfo() {}

	// virtual UINT32 GetVerticalScrollbarWidth();
	// virtual UINT32 GetHorizontalScrollbarHeight();

	// virtual UINT32 GetSystemColor(OP_SYSTEM_COLOR color);

	/** Retrieve default font for system. This is used as the default setting
	  * and can be overridden using the preferences.
	  * @param font The font to retrieve.
	  * @param outfont FontAtt structure describing the font.
	  */
	// virtual void GetSystemFont(OP_SYSTEM_FONT font, FontAtt &outfont);

	// the implementation here should really be supplied by core !
	virtual void GetFont(OP_SYSTEM_FONT font, FontAtt &retval, BOOL use_system_default);

    // virtual COLORREF    GetUICSSColor(int css_color_value);
    // virtual BOOL        GetUICSSFont(int css_font_value, FontAtt &font);
};
#endif // UNIX_OPUIINFO_H
