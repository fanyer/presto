/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DESKTOP_LABEL_H
#define DESKTOP_LABEL_H

#include "modules/widgets/OpEdit.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
#include "modules/accessibility/AccessibleOpWidgetLabel.h"
#endif // ACCESSIBILITY_EXTENSION_SUPPORT


/**
 * A simple, one-line label.
 *
 * @author Blazej Kazmierczak (bkazmierczak@opera.com)
 */
class DesktopLabel : public OpEdit
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, public AccessibleOpWidgetLabel
#endif // ACCESSIBILITY_EXTENSION_SUPPORT
{
public:
	static OP_STATUS Construct(DesktopLabel** obj);

	void SetSelectable(bool selectable) { SetDead(!selectable); }

	// OpWidget
	virtual Type GetType() { return WIDGET_TYPE_DESKTOP_LABEL; }
	virtual void OnSetCursor(const OpPoint &point) { SetCursor(CURSOR_DEFAULT_ARROW); }

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindStaticText;}
	virtual OpAccessibleItem* GetLabelExtension() { return this; }

	virtual int	GetAccessibleChildrenCount() { return 0; }
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child) { return Accessibility::NoSuchChild; }
	virtual OpAccessibleItem* GetAccessibleChild(int) { return NULL; }
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

protected:
	DesktopLabel();
};

#endif // DESKTOP_LABEL_H
