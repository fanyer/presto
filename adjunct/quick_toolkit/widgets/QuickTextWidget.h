/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_TEXT_WIDGET_H
#define QUICK_TEXT_WIDGET_H

#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"
#include "adjunct/desktop_util/adt/opproperty.h"
#include "modules/locale/locale-enum.h"
#include "modules/widgets/OpWidget.h"

class OpEdit;
class OpNumberEdit;
class OpAddressDropDown;

/** @brief Widget that knows how to display a text
  */
class QuickTextWidget
{
public:
	virtual ~QuickTextWidget() {}

	/** Set text this widget should display
	  * @param text Text to display
	  */
	virtual OP_STATUS SetText(const OpStringC& text) = 0;
	OP_STATUS SetText(const OpStringC8& text8);

	/** Set text this widget should display (translatable)
	  * @param id String id of text to display
	  */
	OP_STATUS SetText(Str::LocaleString id);

	/** Get text this widget is displaying
	  * @param text contains widget text on success
	  */
	virtual OP_STATUS GetText(OpString& text) const = 0;

	/** Size setters: similar to size setters in QuickWidget, but using a
	  * number of characters as measurement (eg. '5 characters wide' or
	  * '2 characters wide')
	  * @param count Number of characters to set size for
	  */
	virtual void SetMinimumCharacterWidth(unsigned count) = 0;
	virtual void SetMinimumCharacterHeight(unsigned count) = 0;
	virtual void SetNominalCharacterWidth(unsigned count) = 0;
	virtual void SetNominalCharacterHeight(unsigned count) = 0;
	virtual void SetPreferredCharacterWidth(unsigned count) = 0;
	virtual void SetPreferredCharacterHeight(unsigned count) = 0;
	virtual void SetFixedCharacterWidth(unsigned count) = 0;
	virtual void SetFixedCharacterHeight(unsigned count) = 0;
	virtual void SetCharacterMargins(const WidgetSizes::Margins& margins) = 0;
	virtual void SetRelativeSystemFontSize(unsigned int size) = 0;
	virtual void SetSystemFontWeight(QuickOpWidgetBase::FontWeight weight) = 0;
	virtual void SetEllipsis(ELLIPSIS_POSITION ellipsis_position) = 0;

protected:
	// helper functions for OpWidgets
	static unsigned GetCharWidth(OpEdit* edit, unsigned count);
	static unsigned GetCharWidth(OpNumberEdit* edit, unsigned count);
	static unsigned GetCharWidth(OpMultilineEdit* edit, unsigned count);
	static unsigned GetCharHeight(OpMultilineEdit* edit, unsigned count);
	static unsigned GetCharWidth(OpWidget* widget, unsigned count);
	static unsigned GetCharHeight(OpWidget* widget, unsigned count);
	static unsigned GetCharWidth(OpAddressDropDown* address_dropdown, unsigned count);
	static unsigned GetCharHeight(OpAddressDropDown* address_dropdown, unsigned count);
	template <typename OpWidgetType>
	static WidgetSizes::Margins GetCharMargins(OpWidgetType* widget, const WidgetSizes::Margins& margins);
};


template <typename OpWidgetType>
WidgetSizes::Margins QuickTextWidget::GetCharMargins(OpWidgetType* widget,
		const WidgetSizes::Margins& margins)
{
	WidgetSizes::Margins char_margins;
	char_margins.top = margins.top < WidgetSizes::UseDefault ? GetCharHeight(widget, margins.top) : margins.top;
	char_margins.bottom = margins.bottom < WidgetSizes::UseDefault ? GetCharHeight(widget, margins.bottom) : margins.top;
	char_margins.left = margins.left < WidgetSizes::UseDefault ? GetCharWidth(widget, margins.left) : margins.left;
	char_margins.right = margins.right < WidgetSizes::UseDefault ? GetCharWidth(widget, margins.right) : margins.right;
	return char_margins;
}


/** @brief Like QuickTextWidget, but with possibility to listen for text change
  */
class QuickEditableTextWidget : public QuickTextWidget
{
public:
	/** Add a delegate to call when text changes by user input
	  * @param delegate Delegate that takes the widget as argument.
	  * 		Ownership is transfered.
	  */
	virtual OP_STATUS AddChangeDelegate(OpDelegateBase<OpWidget&>* delegate) = 0;

	/** Remove all delegates of a subscriber object.
	  * @param subscriber subscriber object
	  */
	virtual void RemoveChangeDelegates(const void* subscriber) = 0;
};


/** @brief Default implementation of QuickTextWidget for OpWidget wrappers
  * @param OpWidgetType Which OpWidget to base this class on
  */
template<class OpWidgetType>
class QuickTextWidgetWrapper
  : public QuickOpWidgetWrapper<OpWidgetType>
  , public QuickTextWidget
{
	typedef QuickOpWidgetWrapper<OpWidgetType> Base;

public:
	// Implement QuickTextWidget
	virtual OP_STATUS SetText(const OpStringC& text)
		{ RETURN_IF_ERROR(Base::GetOpWidget()->SetText(text.CStr())); QuickWidget::BroadcastContentsChanged(); return OpStatus::OK; }
	OP_STATUS SetText(const OpStringC8& text8) { return QuickTextWidget::SetText(text8); }
	OP_STATUS SetText(Str::LocaleString id) { return QuickTextWidget::SetText(id); }
	virtual OP_STATUS GetText(OpString& text) const { return Base::GetOpWidget()->GetText(text); }
	virtual void SetMinimumCharacterWidth(unsigned count) { Base::SetMinimumWidth(QuickTextWidget::GetCharWidth(Base::GetOpWidget(), count)); }
	virtual void SetMinimumCharacterHeight(unsigned count) { Base::SetMinimumHeight(QuickTextWidget::GetCharHeight(Base::GetOpWidget(), count)); }
	virtual void SetNominalCharacterWidth(unsigned count) { Base::SetNominalWidth(QuickTextWidget::GetCharWidth(Base::GetOpWidget(), count)); }
	virtual void SetNominalCharacterHeight(unsigned count) { Base::SetNominalHeight(QuickTextWidget::GetCharHeight(Base::GetOpWidget(), count)); }
	virtual void SetPreferredCharacterWidth(unsigned count) { Base::SetPreferredWidth(QuickTextWidget::GetCharWidth(Base::GetOpWidget(), count)); }
	virtual void SetPreferredCharacterHeight(unsigned count) { Base::SetPreferredHeight(QuickTextWidget::GetCharHeight(Base::GetOpWidget(), count)); }
	virtual void SetFixedCharacterWidth(unsigned count) { Base::SetFixedWidth(QuickTextWidget::GetCharWidth(Base::GetOpWidget(), count)); }
	virtual void SetFixedCharacterHeight(unsigned count) { Base::SetFixedHeight(QuickTextWidget::GetCharHeight(Base::GetOpWidget(), count)); }
	virtual void SetCharacterMargins(const WidgetSizes::Margins& margins) { Base::SetMargins(QuickTextWidget::GetCharMargins(Base::GetOpWidget(), margins)); }
	virtual void SetRelativeSystemFontSize(unsigned int size) { Base::GetOpWidget()->SetRelativeSystemFontSize(size); }
	virtual void SetSystemFontWeight(QuickOpWidgetBase::FontWeight weight) { Base::GetOpWidget()->SetSystemFontWeight(weight); }
	virtual void SetEllipsis(ELLIPSIS_POSITION ellipsis_position) { Base::GetOpWidget()->SetEllipsis(ellipsis_position); }
};


/** @brief Default implementation of QuickEditableTextWidget for OpWidget wrappers
  * @param OpWidgetType Which OpWidget to base this class on
  */
template<class OpWidgetType> class QuickEditableTextWidgetWrapper
  : public QuickOpWidgetWrapper<OpWidgetType>
  , public QuickEditableTextWidget
  , public OpWidgetListener
  , public OpDelegateNotifier<OpWidget&>
{
	typedef QuickOpWidgetWrapper<OpWidgetType> Base;

public:
	virtual ~QuickEditableTextWidgetWrapper() { Base::RemoveOpWidgetListener(*this); }
	virtual OP_STATUS Init() { RETURN_IF_ERROR(Base::Init()); return Base::AddOpWidgetListener(*this); }

	// Implement QuickTextWidget
	virtual OP_STATUS SetText(const OpStringC& text)
		{ RETURN_IF_ERROR(Base::GetOpWidget()->SetText(text.CStr())); QuickWidget::BroadcastContentsChanged(); return OpStatus::OK; }
	OP_STATUS SetText(const OpStringC8& text8) { return QuickEditableTextWidget::SetText(text8); }
	OP_STATUS SetText(Str::LocaleString id) { return QuickEditableTextWidget::SetText(id); }
	virtual OP_STATUS GetText(OpString& text) const { return Base::GetOpWidget()->GetText(text); }
	virtual void SetMinimumCharacterWidth(unsigned count) { Base::SetMinimumWidth(QuickTextWidget::GetCharWidth(Base::GetOpWidget(), count)); }
	virtual void SetMinimumCharacterHeight(unsigned count) { Base::SetMinimumHeight(QuickTextWidget::GetCharHeight(Base::GetOpWidget(), count)); }
	virtual void SetNominalCharacterWidth(unsigned count) { Base::SetNominalWidth(QuickTextWidget::GetCharWidth(Base::GetOpWidget(), count)); }
	virtual void SetNominalCharacterHeight(unsigned count) { Base::SetNominalHeight(QuickTextWidget::GetCharHeight(Base::GetOpWidget(), count)); }
	virtual void SetPreferredCharacterWidth(unsigned count) { Base::SetPreferredWidth(QuickTextWidget::GetCharWidth(Base::GetOpWidget(), count)); }
	virtual void SetPreferredCharacterHeight(unsigned count) { Base::SetPreferredHeight(QuickTextWidget::GetCharHeight(Base::GetOpWidget(), count)); }
	virtual void SetFixedCharacterWidth(unsigned count) { Base::SetFixedWidth(QuickTextWidget::GetCharWidth(Base::GetOpWidget(), count)); }
	virtual void SetFixedCharacterHeight(unsigned count) { Base::SetFixedHeight(QuickTextWidget::GetCharHeight(Base::GetOpWidget(), count)); }
	virtual void SetCharacterMargins(const WidgetSizes::Margins& margins) { Base::SetMargins(QuickTextWidget::GetCharMargins(Base::GetOpWidget(), margins)); }
	virtual void SetRelativeSystemFontSize(unsigned int size) { Base::GetOpWidget()->SetRelativeSystemFontSize(size); }
	virtual void SetSystemFontWeight(QuickOpWidgetBase::FontWeight weight) { Base::GetOpWidget()->SetSystemFontWeight(weight); }
	virtual void SetEllipsis(ELLIPSIS_POSITION ellipsis_position) { Base::GetOpWidget()->SetEllipsis(ellipsis_position); }

	// Implement QuickEditableTextWidget
	virtual OP_STATUS AddChangeDelegate(OpDelegateBase<OpWidget&>* delegate) { return Subscribe(delegate); }
	virtual void RemoveChangeDelegates(const void* subscriber) { Unsubscribe(subscriber); }

	// Implement OpWidgetListener
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse)
		{ NotifyAll(*widget); }

private:
	OpProperty<OpString> m_widget_text;
};

#endif // QUICK_TEXT_WIDGET_H
