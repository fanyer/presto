/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/contexts/WidgetContext.h"
#include "adjunct/quick_toolkit/readers/UIReader.h"
#include "adjunct/quick_toolkit/widgets/QuickWidget.h"

namespace DefaultUIPaths
{
	const uni_char Widgets[] = UNI_L("widgets.yml");
};

OpAutoPtr<WidgetReader> WidgetContext::s_reader;

OP_STATUS WidgetContext::InitReader()
{
	OpAutoPtr<WidgetReader> reader (OP_NEW(WidgetReader, ()));
	if (!reader.get())
		return OpStatus::ERR_NO_MEMORY;

	CommandLineArgument* log_argument = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::UIWidgetParserLog);
	OpStringC log_filename = log_argument ? log_argument->m_string_value : OpStringC(0);

	RETURN_IF_ERROR(reader->Init(DefaultUIPaths::Widgets, OPFILE_UI_INI_FOLDER, log_filename));
	s_reader.reset(reader.release());

	return OpStatus::OK;
}

OP_STATUS WidgetContext::CreateQuickWidget(const OpStringC8 & widget_name, QuickWidget* &widget, TypedObjectCollection &collection)
{
	OpAutoPtr<QuickWidget> widget_holder;
	if (!s_reader.get())
		RETURN_IF_ERROR(InitReader());
	
	RETURN_IF_ERROR(s_reader->CreateQuickWidget(widget_name, widget_holder, collection));

	widget = widget_holder.release();
	return OpStatus::OK;
}


