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
#include "InsertHTMLDialog.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpMultiEdit.h"


#ifdef M2_SUPPORT
void InsertHTMLDialog::Init(DesktopWindow* parent_window, RichTextEditor *rich_text_editor)
{
	m_rich_text_editor = rich_text_editor;
	Dialog::Init(parent_window);
};


void InsertHTMLDialog::OnInit()
{
	OpMultilineEdit *edit = static_cast<OpMultilineEdit*>(GetWidgetByName("HTML_multiline_edit"));
	if (edit) 
		edit->SetColoured(TRUE);
}

UINT32 InsertHTMLDialog::OnOk()
{
	OpString source_code;
	GetWidgetText("HTML_multiline_edit",source_code);
	m_rich_text_editor->InsertHTML(source_code);
	
	return 0;
}

#endif // M2_SUPPORT