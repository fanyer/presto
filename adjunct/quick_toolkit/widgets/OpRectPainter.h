/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef OP_RECT_PAINTER_H
#define OP_RECT_PAINTER_H

#include "modules/widgets/OpWidget.h"
#include "adjunct/desktop_util/adt/optightvector.h"

class QuickWidget;

/** @brief A widget that draws red rectangles
  * Used for debugging purposes in layouts
  */
class OpRectPainter : public OpWidget
{
public:
	struct Element
	{
		OpRect rect;
		unsigned min_width;
		unsigned min_height;
		unsigned nom_width;
		unsigned nom_height;
		unsigned pref_width;
		unsigned pref_height;

		Element(const OpRect& _rect) : rect(_rect) {}
	};

	/** Constructor
	  * @param layout Layout for which this painter is used
	  */
	explicit OpRectPainter(QuickWidget* layout);
	
	/** Add a rectangle that should be drawn
	  * @param rect Rectangle to draw, relative to the position of this widget
	  */
	OP_STATUS AddElement(const Element& elem) { return m_elems.Add(elem); }

	/** Remove any previously stored element
	  */
	void ClearElements() { m_elems.Clear(); }

	/** @param show whether to show measurement text
	  */
	void ShowText(BOOL show) { m_show_text = show; }

	/** @return Layout given in constructor
	  */
	QuickWidget* GetLayout() const { return m_layout; }

	// Override OpWidget
	virtual Type GetType() { return WIDGET_TYPE_RECT_PAINTER; }
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);

private:
	void GetSizeDescription(unsigned min, unsigned nom, unsigned pref, OpString& description);

	OpTightVector<Element> m_elems;
	QuickWidget* m_layout;
	BOOL m_show_text;
};

#endif // OP_RECT_PAINTER_H
