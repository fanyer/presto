/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OPDOCUMENTEDIT_LAYOUT_MODIFIER_H
#define OPDOCUMENTEDIT_LAYOUT_MODIFIER_H

#ifdef DOCUMENT_EDIT_SUPPORT

#include "modules/documentedit/OpDocumentEdit.h"

/**
	OpDocumentEditLayoutModifier handles interaction with misc stuff by mouse.
	F.ex. resizing/moving of replaced elements (images/forms/iframes..), tables and positioned boxes.
	It paints necessary handles and overrides mouseinput.
*/

class OpDocumentEditLayoutModifier
#ifdef _DOCEDIT_DEBUG
	: public OpDocumentEditDebugCheckerObject
#endif
{
public:
				OpDocumentEditLayoutModifier(OpDocumentEdit* edit);
	virtual		~OpDocumentEditLayoutModifier();

	void		Paint(VisualDevice* vis_dev);
	void		UpdateRect();

	BOOL		IsLayoutModifiable(HTML_Element* helm);
	CursorType	GetCursorType(HTML_Element* helm, int x, int y);
	BOOL		HandleMouseEvent(HTML_Element* helm, DOM_EventType event, int x, long y, MouseButton button);

	void		Delete();

	void		Unactivate();
	BOOL		IsActive() { return m_helm ? TRUE : FALSE; }
public:
	OpDocumentEdit* m_edit;
	HTML_Element* m_helm;
	OpRect m_rect;
};

#endif // DOCUMENT_EDIT_SUPPORT
#endif // OPDOCUMENTEDIT_LAYOUT_MODIFIER_H
