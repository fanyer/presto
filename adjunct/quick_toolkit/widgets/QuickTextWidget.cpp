/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"

#include "adjunct/desktop_util/widget_utils/VisualDeviceHandler.h"
#include "adjunct/desktop_util/widget_utils/WidgetUtils.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpNumberEdit.h"
#include "modules/widgets/OpWidget.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"

OP_STATUS QuickTextWidget::SetText(const OpStringC8& text8)
{
	OpString text;
	RETURN_IF_ERROR(text.Set(text8));
	return SetText(text);
}

OP_STATUS QuickTextWidget::SetText(Str::LocaleString id)
{
	OpString string;
	RETURN_IF_ERROR(g_languageManager->GetString(id, string));
	return SetText(string);
}

unsigned QuickTextWidget::GetCharWidth(OpEdit* edit, unsigned count)
{
	return WidgetUtils::GetFontWidth(edit->font_info) * count;
}

unsigned QuickTextWidget::GetCharWidth(OpNumberEdit* edit, unsigned count)
{
	VisualDeviceHandler vis_dev_handler(edit);

	unsigned edit_width = WidgetUtils::GetFontWidth(edit->font_info) * count;
	unsigned edit_height = GetCharHeight(edit, count);
	return edit_width + edit->GetExtraWidth(edit_height);
}

unsigned QuickTextWidget::GetCharWidth(OpMultilineEdit* edit, unsigned count)
{
	VisualDeviceHandler vis_dev_handler(edit);
	return GetCharWidth(static_cast<OpWidget*>(edit), count);
}

unsigned QuickTextWidget::GetCharHeight(OpMultilineEdit* edit, unsigned count)
{
	return WidgetUtils::GetFontHeight(edit->font_info) * count;
}

unsigned QuickTextWidget::GetCharWidth(OpWidget* widget, unsigned count)
{
	INT32 width, height;
	widget->GetPreferedSize(&width, &height, count, 1);

	return width;
}

unsigned QuickTextWidget::GetCharHeight(OpWidget* widget, unsigned count)
{
	INT32 width, height;
	widget->GetPreferedSize(&width, &height, 1, count);

	return height;
}

unsigned QuickTextWidget::GetCharWidth(OpAddressDropDown* address_dropdown, unsigned count)
{
	return WidgetUtils::GetFontWidth(address_dropdown->edit->font_info) * count;
}

unsigned QuickTextWidget::GetCharHeight(OpAddressDropDown* address_dropdown, unsigned count)
{
	return WidgetUtils::GetFontHeight(address_dropdown->edit->font_info) * count;
}
