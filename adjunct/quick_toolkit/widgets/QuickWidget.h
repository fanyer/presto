/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_WIDGET_H
#define QUICK_WIDGET_H

#include "adjunct/desktop_util/adt/typedobject.h"
#include "adjunct/quick_toolkit/widgets/WidgetSizes.h"
#include "modules/widgets/OpWidget.h"

class QuickWidgetContainer;


/** @brief An object that can be used by layouters
  * The QuickWidget interface includes all functions necessary for the
  * layouters to do their business.
  *
  * The size of a widget as determined by a layouter is calculated by taking
  * these values into account:
  * - Minimum size: Widget should never be smaller than this size.
  *   eg. a single-line text label should always be big enough to fit its text.
  *
  * - Nominal size: A good initial size for a widget
  *   Often equal to the minimum size, but doesn't have to be. This can be used
  *   to determine the initial size to use when creating a window to fit
  *   widgets in.
  *
  * - Preferred size: If possible, the widget should be this big (also called
  *   maximum size). Can be Infinity or Fill (see above). If the preferred
  *   size is equal to the minimum size, the widget has a fixed size (size
  *   never changes).
  *   eg. a single-line text label has a fixed size, but a text input field can
  *   grow to make input easier.
  *
  * - Margins: If the widget is placed next to another widget, at least this
  *   distance should be maintained between the widgets.
  *   eg. a radio button on MacOS X should leave 6 pixels above and below it.
  *
  * Current layouters that will respect these values include QuickGrid and
  * QuickStackLayout. Each layouter is a QuickWidget itself, so that it can
  * be included in complex layouts, to create for example a 3x2 grid with a
  * horizontal stack in each cell.
  *
  * == Is an OpWidget needed to make a QuickWidget?
  * No. An OpWidget is needed when there is something to be painted onto the
  * screen or when interaction with a widget is necessary. Most layouting
  * widgets don't need to do that. If it's not necessary, try avoiding using
  * an OpWidget, since it takes a lot of memory and includes a lot of
  * functionality.
  *
  * == How to make a QuickWidget from an OpWidget?
  * Use the predefined widgets in QuickWidgetDecls.h, or create your own
  * QuickOpWidgetWrapper if necessary. Simple widgets can be wrapped using
  * QuickWrap().
  */
class QuickWidget : public TypedObject
{
	IMPLEMENT_TYPEDOBJECT(TypedObject);

public:
	QuickWidget();
	virtual ~QuickWidget() {}

	/** @return Minimum width at which the widget is still usable, in pixels
	  */
	unsigned GetMinimumWidth();

	/** @param width Width for which minimum height is required, or WidgetSizes::UseDefault
	  * @return Minimum height at which the widget is still usable, in pixels
	  */
	unsigned GetMinimumHeight(unsigned width = WidgetSizes::UseDefault);

	/** @return Nominal width for this widget, can be used as initial size
	  *
	  * GetMinimumWidth <= GetNominalWidth() can be assumed
	  */
	unsigned GetNominalWidth();

	/** @param width Width for which nominal height is required, or WidgetSizes::UseDefault
	  * @return Nominal height for this widget, can be used as initial size
	  *
	  * GetMinimumHeight <= GetNominalHeight() can be assumed
	  */
	unsigned GetNominalHeight(unsigned width = WidgetSizes::UseDefault);

	/** @return Width that widget would need if infinite width was available, in pixels
	  * or WidgetSizes::Infinity / WidgetSizes::Fill
	  *
	  * GetNominalWidth() <= GetPreferredWidth() can be assumed
	  */
	unsigned GetPreferredWidth();

	/** @param width Width for which preferred height is required, or WidgetSizes::UseDefault
	  * @return Height that widget would need if infinite height was available, in pixels
	  * or WidgetSizes::Infinity / WidgetSizes::Fill
	  *
	  * GetNominalHeight() <= GetPreferredHeight() can be assumed
	  */
	unsigned GetPreferredHeight(unsigned width = WidgetSizes::UseDefault);

	/** @return Margins this widget requires on its sides
	  */
	WidgetSizes::Margins GetMargins();

	/** @param minimum_width Desired minimum width in pixels, or WidgetSizes::UseDefault
	  */
	void SetMinimumWidth(unsigned minimum_width) { SetIfChanged(m_minimum_width, minimum_width); }

	/** @param minimum_height Desired minimum height in pixels, or WidgetSizes::UseDefault
	  */
	void SetMinimumHeight(unsigned minimum_height) { SetIfChanged(m_minimum_height, minimum_height); }

	/** @param nominal_width Desired nominal width in pixels, or WidgetSizes::UseDefault
	  */
	void SetNominalWidth(unsigned nominal_width) { SetIfChanged(m_nominal_width, nominal_width); }

	/** @param nominal_height Desired nominal height in pixels, or WidgetSizes::UseDefault
	  */
	void SetNominalHeight(unsigned nominal_height) { SetIfChanged(m_nominal_height, nominal_height); }

	/** @param preferred_width Desired preferred width in pixels, or
	  * WidgetSizes::UseDefault, WidgetSizes::Fill or WidgetSizes::Infinity
	 */
	void SetPreferredWidth(unsigned preferred_width) { SetIfChanged(m_preferred_width, preferred_width); }

	/** @param preferred_height Desired preferred height in pixels, or
	  * WidgetSizes::UseDefault, WidgetSizes::Fill or WidgetSizes::Infinity
	  */
	void SetPreferredHeight(unsigned preferred_height) { SetIfChanged(m_preferred_height, preferred_height); }

	/** @param fixed_width Desired fixed width in pixels
	 */
	void SetFixedWidth(unsigned fixed_width) { SetMinimumWidth(fixed_width); SetNominalWidth(fixed_width); SetPreferredWidth(fixed_width); }

	/** @param fixed_height Desired fixed height in pixels
	 */
	void SetFixedHeight(unsigned fixed_height) { SetMinimumHeight(fixed_height); SetNominalHeight(fixed_height); SetPreferredHeight(fixed_height); }

	/** Set margins needed on sides of widget
	  * @param margins List with desired margins, or WidgetSizes::UseDefault
	  */
	void SetMargins(const WidgetSizes::Margins& margins) { SetIfChanged(m_margins, margins); }

	/** Set container for this widget
	  */
	virtual void SetContainer(QuickWidgetContainer* container) { m_container = container; }

	/** Get container for this widget
	  */
	QuickWidgetContainer* GetContainer() const { return m_container; }

	/** @return Whether widget has height that depends on width
	  * @note Using this will make your layout slower. Use widgets that
	  * have this property with care.
	  */
	virtual BOOL HeightDependsOnWidth() = 0;

	/** Layout this widget in a given rectangle
	  * @param rect Rectangle in which widget should be layed out
	  */
	virtual OP_STATUS Layout(const OpRect& rect) = 0;

	/** Layout this widget in a given rectangle within a container rectangle
	  *
	  * Call this version of Layout() from a QuickWidgetContainer to lay out
	  * contained widgets.  It will account for UI direction automatically.
	  *
	  * @param rect Rectangle in which widget should be layed out
	  * @param container_rect Rectangle of the container the widget is in
	  */
	OP_STATUS Layout(const OpRect& rect, const OpRect& container_rect);

	/** Set an OpWidget that should be the parent of this widget 
	  * If your widget can be represented as an OpWidget, 
	  * @param parent The OpWidget that serves as a parent to this object, or
	  * NULL to reset the parent
	  */
	virtual void SetParentOpWidget(OpWidget* parent) = 0;
	
	/** Show widget */
	virtual void Show() = 0;

	/** Hide widget */
	virtual void Hide() = 0;

	/** @return Whether widget is currently visible */
	virtual BOOL IsVisible() = 0;

	/** Set whether widget should be enabled (can receive input events)
	  * @param enabled Whether widget should be enabled
	  */
	virtual void SetEnabled(BOOL enabled) = 0;

	/** Set Z-order of widget */
	virtual void SetZ(OpWidget::Z z) {}

protected:
	// See explanation above about these measurements
	virtual unsigned GetDefaultMinimumWidth() = 0;
	virtual unsigned GetDefaultMinimumHeight(unsigned width) = 0;
	virtual unsigned GetDefaultNominalWidth() = 0;
	virtual unsigned GetDefaultNominalHeight(unsigned width) = 0;
	virtual unsigned GetDefaultPreferredWidth() = 0;
	virtual unsigned GetDefaultPreferredHeight(unsigned width) = 0;
	virtual void GetDefaultMargins(WidgetSizes::Margins& margins) = 0;

	void BroadcastContentsChanged();

private:
	template<class T>
	void SetIfChanged(T& property, T value) { if (property != value) { property = value; BroadcastContentsChanged(); } }

	unsigned m_minimum_width;
	unsigned m_minimum_height;
	unsigned m_nominal_width;
	unsigned m_nominal_height;
	unsigned m_preferred_width;
	unsigned m_preferred_height;
	WidgetSizes::Margins m_margins;
	QuickWidgetContainer* m_container;
};

#endif // QUICK_WIDGET_H
