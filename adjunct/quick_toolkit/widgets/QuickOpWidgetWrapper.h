/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_OP_WIDGET_WRAPPER_H
#define QUICK_OP_WIDGET_WRAPPER_H

#include "adjunct/quick_toolkit/widgets/QuickWidget.h"

class OpWidgetListener;


/** @brief Base class for QuickOpWidgetWrapper, see below */
class GenericQuickOpWidgetWrapper : public QuickWidget
{
public:
	GenericQuickOpWidgetWrapper(OpWidget* widget = 0);
	virtual ~GenericQuickOpWidgetWrapper();

	// Overriding QuickWidget
	virtual BOOL HeightDependsOnWidth() { return FALSE; }
	virtual OP_STATUS Layout(const OpRect& rect);
	virtual void SetParentOpWidget(OpWidget* widget);
	virtual void Show();
	virtual void Hide();
	virtual BOOL IsVisible();
	virtual void SetEnabled(BOOL enabled);
	virtual void SetZ(OpWidget::Z z) { if (m_opwidget) m_opwidget->SetZ(z); }
	
	/**
	 * Registers a listener to be notified of events concerning the wrapped
	 * OpWidget.
	 *
	 * @note Refer to, and extend if necessary, OpWidgetEventForwarder to make
	 * sure the events you're interested in are actually forwarded.
	 *
	 * @param listener the listener
	 * @return status
	 *
	 * @see OpWidgetEventForwarder
	 * @see RemoveOpWidgetListener
	 */
	OP_STATUS AddOpWidgetListener(OpWidgetListener& listener);

	/**
	 * Unregisters a listener so that it is not notified of events concerning
	 * the wrapped OpWidget anymore.
	 *
	 * @param listener the listener
	 * @return status
	 *
	 * @see AddOpWidgetListener
	 */
	OP_STATUS RemoveOpWidgetListener(OpWidgetListener& listener);

	/** Set the skin this widget should use (traditionally 'border skin')
	 * Also sets padding and margins for widget
	 * @param skin_element Skin element to use
	 * @param fallback Fallback skin element to use if skin_element is unavailable
	 */
	void SetSkin(const OpStringC8& skin_element, const OpStringC8& fallback = 0);

protected:
	// Overriding QuickWidget
	virtual unsigned GetDefaultMinimumWidth();
	virtual unsigned GetDefaultMinimumHeight(unsigned width);
	virtual unsigned GetDefaultNominalWidth();
	virtual unsigned GetDefaultNominalHeight(unsigned width);
	virtual unsigned GetDefaultPreferredWidth();
	virtual unsigned GetDefaultPreferredHeight(unsigned width);
	virtual void GetDefaultMargins(WidgetSizes::Margins& margins);

	/** @return Preferred width as indicated by the OpWidget system
	  */
	unsigned GetOpWidgetPreferredWidth() { return MAX(GetOpWidgetPreferredSize().width, 0); }

	/** @return Preferred height as indicated by the OpWidget system
	  */
	unsigned GetOpWidgetPreferredHeight() { return MAX(GetOpWidgetPreferredSize().height, 0); }

	/** @return Preferred size (width and height) as indicated by the OpWidget system
	  */
	OpRect GetOpWidgetPreferredSize();

	OpWidget* GetOpWidget() const { return m_opwidget; }
	void SetOpWidget(OpWidget* opwidget);

private:
	class OpWidgetEventForwarder;

	OpWidget* m_opwidget;
	OpWidgetEventForwarder* m_opwidget_event_forwarder;
};


/*
 * If you are unable to use QuickOpWidgetWrapper directly (e.g. because your widget class requires parameters)
 * subclass QuickOpWidgetWrapperNoInit and do the initialization/construction yourself.
 */
template <typename OpWidgetType>
class QuickOpWidgetWrapperNoInit : public GenericQuickOpWidgetWrapper
{
	IMPLEMENT_TYPEDOBJECT(GenericQuickOpWidgetWrapper);
public:
	/** Default constructor
	  */
	QuickOpWidgetWrapperNoInit() {}

	/** Constructor that initializes from an existing widget */
	QuickOpWidgetWrapperNoInit(OpWidgetType* widget)  { SetOpWidget(widget); }

	/** @return The underlying OpWidget for this QuickWidget
	  */
	OpWidgetType* GetOpWidget() const { return static_cast<OpWidgetType*>(GenericQuickOpWidgetWrapper::GetOpWidget()); }
};


/** @brief A QuickWidget based on an OpWidget
  * @param OpWidgetType OpWidget to use as base
  * eg.
  *   class MyQuickWidget : public QuickOpWidgetWrapper<MyOpWidget>
  * or
  *   QuickWidget* widget = OP_NEW(QuickOpWidgetWrapper<MyOpWidget>, ());
  *
  * @note Please use predefined wrappers from QuickWidgetDecls.h, and feel free
  * to add new ones there
  *
  * This is a simple wrapper for OpWidgets that provides all the information
  * needed in the Quick layout system. It will generate a fixed-size
  * widget (preferred size equal to minimum size). Override the standard
  * QuickWidget size functions (GetDefault*(), see QuickWidget.h) if you need
  * a custom sizing policy for a widget. For an example of a widget with custom
  * sizing, see QuickButton.h.
  */
template <typename OpWidgetType>
class QuickOpWidgetWrapper : public QuickOpWidgetWrapperNoInit<OpWidgetType>
{
	IMPLEMENT_TYPEDOBJECT(QuickOpWidgetWrapperNoInit<OpWidgetType>);

public:
	typedef QuickOpWidgetWrapper<OpWidgetType> Type;

	/** Default constructor
	  */
	QuickOpWidgetWrapper() {}

	/** Initialize widget, must be run before running other functions (except
	  * when constructing from an existing OpWidget)
	  */
	virtual inline OP_STATUS Init();
};


/** Utility function to wrap an OpWidget
  * @param widget OpWidget to wrap, takes ownership
  * @return a QuickWidget that wraps an OpWidget or NULL on OOM
  *
  * ex. usage:
  *   OpWidget* opwidget = ...;
  *   QuickWidget* widget = QuickWrap(opwidget);
  */
template<typename OpWidgetType> inline
QuickOpWidgetWrapperNoInit<OpWidgetType>* QuickWrap(OpWidgetType* widget)
{
	QuickOpWidgetWrapperNoInit<OpWidgetType>* quickwidget = OP_NEW(QuickOpWidgetWrapperNoInit<OpWidgetType>, (widget));
	if (!quickwidget)
		widget->Delete();

	return quickwidget;
}


// Implementations of template functions //

template <typename OpWidgetType>
OP_STATUS QuickOpWidgetWrapper<OpWidgetType>::Init()
{
	OpWidgetType* widget;
	RETURN_IF_ERROR(OpWidgetType::Construct(&widget));
	this->SetOpWidget(widget);

	return OpStatus::OK;
}

#endif // QUICK_OP_WIDGET_WRAPPER_H
