/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_RICHTEXT_LABEL_H
#define OP_RICHTEXT_LABEL_H

#include "modules/widgets/OpMultiEdit.h"


/***********************************************************************************
 **
 **	OpRichTextLabel
 **
 ** Currently one can set text with following html tags:
 ** <b> bolded text </b>
 ** <i> italic text </i>
 ** <u> underscored text </u>
 ** <h> headline text </h>
 ** <a href="http://opera.com"> text which is a link to opera web page </a>
 ** <a href="opera:/action/xxx"> text which is an opera action </a> (where xxx is an action)
 **
 ** Using opera action in anchor example:
 **   "<a href=\"opera:/action/Open url in new page, &quot;http://www.google.com&quot;\">google</a>"
 **
 ***********************************************************************************/

class OpRichTextLabel : public OpRichtextEdit
{
public:
	static OP_STATUS	Construct(OpRichTextLabel** obj);
	
	INT32				GetTextWidth();
	INT32				GetTextHeight();

	virtual void		GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);	
	virtual void		OnResize(INT32* new_w, INT32* new_h);

	
	// == Hooks ======================
	virtual void		OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	
	
	virtual Type		GetType() {return WIDGET_TYPE_RICHTEXT_LABEL;}
	virtual BOOL		IsOfType(OpTypedObject::Type type) { return type == WIDGET_TYPE_RICHTEXT_LABEL || OpRichtextEdit::IsOfType(type); }

    // UI automation testing methods
	virtual OpScopeWidgetInfo* CreateScopeWidgetInfo();

protected:
	OpRichTextLabel();

private:
	class ScopeWidgetInfo;
	friend class ScopeWidgetInfo;
};

#endif // OP_RICHTEXT_LABEL_H
