/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/creators/QuickDialogCreator.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick_toolkit/creators/QuickWidgetCreator.h"
#include "adjunct/quick_toolkit/readers/UIReader.h"
#include "adjunct/quick_toolkit/widgets/QuickButtonStrip.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/ui_parser/ParserIterators.h"


////////// Init
OP_STATUS
QuickDialogCreator::Init(ParserNodeMapping * ui_node)
{
	RETURN_IF_ERROR(QuickUICreator::Init(ui_node));
	RETURN_IF_ERROR(GetScalarStringFromMap("name", m_dialog_name));

	if (m_dialog_name.IsEmpty())
	{
		GetLog().OutputEntry("ERROR: Dialog name ('%s') not specified", m_dialog_name);
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}


////////// CreateQuickDialog
OP_STATUS
QuickDialogCreator::CreateQuickDialog(QuickDialog& dialog)
{
	ParserLogger::AutoIndenter indenter(GetLog(), "Reading dialog definition for dialog '%s'", m_dialog_name);

	OpString8 dialog_type;
	RETURN_IF_ERROR(GetScalarStringFromMap("type", dialog_type));
	if (dialog_type.IsEmpty())
	{
		GetLog().OutputEntry("ERROR: Missing dialog's 'type' attribute");
		return OpStatus::ERR;
	}

	OpString8 alert_image;
	bool scrolled_content = true;

	if (dialog_type == "AlertDialog")
	{
		RETURN_IF_ERROR(GetScalarStringFromMap("skin-image", alert_image));
		if (alert_image.IsEmpty())
		{
			GetLog().OutputEntry("ERROR: AlertDialog needs to specify the 'skin-image' attribute");
			return OpStatus::ERR;
		}
	}
	else if (dialog_type == "Dialog")
	{
		RETURN_IF_ERROR(GetScalarBoolFromMap("scrolled-content", scrolled_content));
	}
	else
	{
		GetLog().OutputEntry("ERROR: Unsupported dialog type");
		OP_ASSERT(!"not supported dialog type");
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(GetLog().Evaluate(dialog.Init(scrolled_content), "NON-PARSER ERROR: Dialog initialization failed"));

	RETURN_IF_ERROR(GetLog().Evaluate(dialog.SetName(m_dialog_name), "NON-PARSER ERROR: Setting dialog name failed"));

	OpString title;
	RETURN_IF_ERROR(GetTranslatedScalarStringFromMap("title", title));
	if (title.HasContent())
	{
		RETURN_IF_ERROR(dialog.SetTitle(title));
	}

	OpAutoPtr<TypedObjectCollection> widget_collection(
			OP_NEW(TypedObjectCollection, ()));
	RETURN_OOM_IF_NULL(widget_collection.get());

	OpAutoPtr<QuickWidget> button_strip;
	{
		ParserLogger::AutoIndenter indenter(GetLog(), "Reading dialog button strip");
		RETURN_IF_ERROR(CreateWidgetFromMapNode("button-strip", button_strip, *widget_collection, true));
		indenter.Done();
	}
	OpAutoPtr<QuickWidget> header;
	{
		ParserLogger::AutoIndenter indenter(GetLog(), "Reading dialog header");
		RETURN_IF_ERROR(CreateWidgetFromMapNode("header", header, *widget_collection, true));
		indenter.Done();
	}
	OpAutoPtr<QuickWidget> content_widget;
	{
		ParserLogger::AutoIndenter indenter(GetLog(), "Reading dialog content");
		RETURN_IF_ERROR(CreateWidgetFromMapNode("content", content_widget, *widget_collection, true));
		indenter.Done();
	}
	if (!content_widget.get())
		return OpStatus::ERR;

	RETURN_IF_ERROR(dialog.SetDialogContent(TypedObject::GetTypedObject<QuickButtonStrip>(button_strip.release()), content_widget.release(), alert_image));
	if (header.get())
		dialog.SetDialogHeader(header.release());

	dialog.SetWidgetCollection(widget_collection.release());

	indenter.Done();
	return OpStatus::OK;
}
