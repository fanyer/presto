/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/
#include "core/pch.h"
#include "InsertLinkDialog.h"

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/m2_ui/widgets/RichTextEditor.h"
#include "modules/widgets/OpEdit.h"

#ifdef M2_SUPPORT

void InsertLinkDialog::Init(DesktopWindow * parent_window, RichTextEditor* rich_text_editor, OpString title)
{
	m_title.Set(title);
	m_rich_text_editor = rich_text_editor;
	Dialog::Init(parent_window);
}


void InsertLinkDialog::OnInit()
{
	if (m_title.HasContent())
	{
		SetWidgetText("Title_edit",m_title.CStr());
		SetWidgetEnabled("Title_edit",FALSE);
	}
	SetWidgetValue("Radio_use_web_address",TRUE);
	OpEdit* web_url_edit = GetWidgetByName<OpEdit>("web_url_edit", WIDGET_TYPE_EDIT);
	if (web_url_edit)
	{
		web_url_edit->SetText(UNI_L("http://"));
		web_url_edit->SetForceTextLTR(TRUE);
	}
	OpEdit* mailto_url_edit = GetWidgetByName<OpEdit>("mailto_url_edit", WIDGET_TYPE_EDIT);
	if (mailto_url_edit)
	{
		mailto_url_edit->SetEnabled(FALSE);
		mailto_url_edit->SetForceTextLTR(TRUE);
		if (SupportsExternalCompositing())
			mailto_url_edit->autocomp.SetType(AUTOCOMPLETION_CONTACTS);
	}
}

void InsertLinkDialog::OnClick(OpWidget *widget, UINT32 id)
{
	if (widget->IsNamed("Radio_use_web_address"))
	{
		SetWidgetEnabled("mailto_url_edit",FALSE);
		SetWidgetEnabled("web_url_edit",TRUE);
	}
	else if (widget->IsNamed("Radio_use_mailto_address"))
	{
		SetWidgetEnabled("mailto_url_edit",TRUE);
		SetWidgetEnabled("web_url_edit",FALSE);
	}

	Dialog::OnClick(widget, id);
}

UINT32 InsertLinkDialog::OnOk()
{
	OpString address, title, resolved_address;
	GetWidgetText("Title_edit",title);
	if (GetWidgetValue("Radio_use_web_address"))
	{
		GetWidgetText("web_url_edit",address);
	}
	else 
	{
		GetWidgetText("mailto_url_edit",address);
		if (address.Compare(UNI_L("mailto:")) != 0)
			address.Insert(0,UNI_L("mailto:"));
	}
	g_url_api->ResolveUrlNameL(address,resolved_address,TRUE);
	m_rich_text_editor->InsertLink(title,resolved_address);
	return 0;
}

#endif // M2_SUPPORT
