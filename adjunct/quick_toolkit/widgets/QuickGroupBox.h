/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cihat Imamoglu (cihati)
 */
 
#ifndef QUICK_GROUP_BOX_H
#define QUICK_GROUP_BOX_H

#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContent.h"


/** @brief Group of content that belongs together
  * Used to group widgets in dialog. Can optionally provide
  * a text label to appear above the group
  */
class QuickGroupBox
  : public QuickWidget
  , public QuickWidgetContainer
  , public QuickTextWidget
{
	IMPLEMENT_TYPEDOBJECT(QuickWidget);
public:
	QuickGroupBox();

	OP_STATUS Init();

	/** Set contents of the group box
	  * @param new_child Contents to be used (takes ownership)
	  */
	void SetContent(QuickWidget* new_child);

	void SetName(const OpStringC8& name) { m_label.GetOpWidget()->SetName(name); }

	// Overriding QuickWidget
	virtual BOOL HeightDependsOnWidth()  { return m_content->HeightDependsOnWidth(); }
	virtual OP_STATUS Layout(const OpRect& rect);
	virtual void SetParentOpWidget(class OpWidget* parent);
	virtual void Show();
	virtual void Hide();
	virtual BOOL IsVisible();
	virtual void SetEnabled(BOOL enabled);	

	// Overriding QuickWidgetContainer
	virtual void OnContentsChanged() { BroadcastContentsChanged(); }

	// Overriding QuickTextWidget
	/** Set text to appear in label above group
	  * @param text Text that describes group contents
	  */
	virtual OP_STATUS SetText(const OpStringC& text) { return m_label.SetText(text); }
	virtual OP_STATUS GetText(OpString& text) const { return m_label.GetText(text); }
	virtual void SetMinimumCharacterWidth(unsigned count) {}
	virtual void SetMinimumCharacterHeight(unsigned count) {}
	virtual void SetNominalCharacterWidth(unsigned count) {}
	virtual void SetNominalCharacterHeight(unsigned count) {}
	virtual void SetPreferredCharacterWidth(unsigned count) {}
	virtual void SetPreferredCharacterHeight(unsigned count) {}
	virtual void SetFixedCharacterWidth(unsigned count) {}
	virtual void SetFixedCharacterHeight(unsigned count) {}
	virtual void SetCharacterMargins(const WidgetSizes::Margins& margins) {}
	virtual void SetRelativeSystemFontSize(unsigned int size) { m_label.GetOpWidget()->SetRelativeSystemFontSize(size); }
	virtual void SetSystemFontWeight(QuickOpWidgetBase::FontWeight weight) { m_label.GetOpWidget()->SetSystemFontWeight(weight); }
	virtual void SetEllipsis(ELLIPSIS_POSITION ellipsis_position) {}

protected:
	virtual unsigned GetDefaultMinimumWidth();
	virtual unsigned GetDefaultMinimumHeight(unsigned width);
	virtual unsigned GetDefaultNominalWidth();
	virtual unsigned GetDefaultNominalHeight(unsigned width);
	virtual unsigned GetDefaultPreferredWidth();
	virtual unsigned GetDefaultPreferredHeight(unsigned width);
	virtual void GetDefaultMargins(WidgetSizes::Margins& margins);

private:
	QuickLabel m_label;
	QuickWidgetContent m_content;

	BOOL m_visible;
	INT32 m_left_padding;
};

#endif // QUICK_GROUP_BOX_H
