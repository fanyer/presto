/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */

#ifndef QUICK_TREE_VIEW_H
#define QUICK_TREE_VIEW_H

#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"
#include "adjunct/desktop_util/widget_utils/WidgetUtils.h"


class QuickTreeView : public QuickOpWidgetWrapper<OpTreeView>, public QuickTextWidget
{
	IMPLEMENT_TYPEDOBJECT(QuickOpWidgetWrapper<OpTreeView>);
public:
	// Override QuickTextWidget
	virtual OP_STATUS SetText(const OpStringC& text) { return OpStatus::OK; }
	virtual OP_STATUS GetText(OpString& text) const { return OpStatus::OK; }
	virtual void SetMinimumCharacterWidth(unsigned count) { SetMinimumWidth(GetWidth(count)); }
	virtual void SetMinimumCharacterHeight(unsigned count) { SetMinimumHeight(GetHeight(count)); }
	virtual void SetNominalCharacterWidth(unsigned count) { SetNominalWidth(GetWidth(count)); }
	virtual void SetNominalCharacterHeight(unsigned count) { SetNominalHeight(GetHeight(count)); }
	virtual void SetPreferredCharacterWidth(unsigned count) { SetPreferredWidth(GetWidth(count)); }
	virtual void SetPreferredCharacterHeight(unsigned count) { SetPreferredHeight(GetHeight(count)); }
	virtual void SetFixedCharacterWidth(unsigned count) { SetFixedWidth(GetWidth(count)); }
	virtual void SetFixedCharacterHeight(unsigned count) { SetFixedHeight(GetHeight(count)); }
	virtual void SetCharacterMargins(const WidgetSizes::Margins& margins) {}
	virtual void SetRelativeSystemFontSize(unsigned int size) {}
	virtual void SetSystemFontWeight(QuickOpWidgetBase::FontWeight weight) {}
	virtual void SetEllipsis(ELLIPSIS_POSITION ellipsis_position) {}

protected:
	// Override QuickWidget
	virtual unsigned GetDefaultMinimumHeight(unsigned width)
		{ return GetHeight(MinimumLines); }
	virtual unsigned GetDefaultNominalHeight(unsigned width)
		{ return GetHeight(NominalLines); }
	virtual unsigned GetDefaultPreferredWidth()
		{ return WidgetSizes::Infinity; }
	virtual unsigned GetDefaultPreferredHeight(unsigned width)
		{ return WidgetSizes::Infinity; }

private:
	static const unsigned MinimumLines = 2; ///< Lines visible at default minimum size
	static const unsigned NominalLines = 10; ///< Lines visible at default nominal size

	unsigned GetWidth(unsigned count) const { return WidgetUtils::GetFontWidth(GetOpWidget()->font_info) * count; }
	unsigned GetHeight(unsigned count) const { return count * GetOpWidget()->GetLineHeight(); }
};

#endif // QUICK_TREE_VIEW_H
