/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_BINDER_TEXT_H
#define QUICK_BINDER_TEXT_H

#include "adjunct/quick_toolkit/bindings/QuickBinder.h"

namespace PrefUtils { class StringAccessor; }
class QuickTextWidget;
class QuickEditableTextWidget;


/**
 * A base class of (potentially) bidirectional bindings for the text of
 * a widget.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::TextBinding : public QuickBinder::Binding
{
public:
	TextBinding();
	virtual ~TextBinding() {}

	template<class T> OP_STATUS Init(T& widget) { return Init(widget, widget); }
	OP_STATUS Init(QuickWidget& widget, QuickTextWidget& text_widget);

	// From Binding
	virtual const QuickWidget* GetBoundWidget() const { return m_widget; }

protected:
	QuickWidget* m_widget;
	QuickTextWidget* m_text_widget;
};


/**
 * A unidirectional binding for the text of a widget.
 *
 * The text displayed in the widget is bound to a string OpProperty.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::TextPropertyBinding : public TextBinding
{
public:
	TextPropertyBinding();
	virtual ~TextPropertyBinding();

	template<class T> OP_STATUS Init(T& widget, OpProperty<OpString>& property) { return Init(widget, widget, property); }
	OP_STATUS Init(QuickWidget& widget, QuickTextWidget& text_widget, OpProperty<OpString>& property);

	void OnPropertyChanged(const OpStringC& new_value);

protected:
	OpProperty<OpString>* m_property;
};


/**
 * A bidirectional binding for the text of a widget.
 *
 * The widget text is bound to a string OpProperty.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::EditableTextPropertyBinding : public TextPropertyBinding
{
public:
	virtual ~EditableTextPropertyBinding();

	template<class T> OP_STATUS Init(T& widget, OpProperty<OpString>& property) { return Init(widget, widget, property); }
	OP_STATUS Init(QuickWidget& widget, QuickEditableTextWidget& edit, OpProperty<OpString>& property);

	void OnTextChanged(OpWidget& widget);
};


/**
 * A unidirectional preference binding for the text of a widget.
 *
 * The widget text is bound to a string preference.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::EditableTextPreferenceBinding : public TextBinding
{
public:
	EditableTextPreferenceBinding();
	virtual ~EditableTextPreferenceBinding();

	template<class T> OP_STATUS Init(T& widget, PrefUtils::StringAccessor* accessor) { return Init(widget, widget, accessor); }
	OP_STATUS Init(QuickWidget& widget, QuickEditableTextWidget& edit, PrefUtils::StringAccessor* accessor);

	void OnTextChanged(OpWidget& widget);

private:
	PrefUtils::StringAccessor* m_accessor;
};

#endif // QUICK_BINDER_TEXT_H
