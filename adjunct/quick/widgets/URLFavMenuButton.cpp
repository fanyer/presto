/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/widgets/URLFavMenuButton.h"

#include "adjunct/quick_toolkit/contexts/DialogContext.h"

URLFavMenuButton::URLFavMenuButton()
	: OpButton(TYPE_CUSTOM, STYLE_IMAGE)
	, m_dialog_context(NULL)
{
}

OP_STATUS URLFavMenuButton::Init(const char *brdr_skin, const char *fg_skin)
{
	SetSkinned(TRUE);
	SetFixedTypeAndStyle(TRUE);

	if (brdr_skin)
		GetBorderSkin()->SetImage(brdr_skin, "Push Button Skin");

	if (fg_skin)
		GetForegroundSkin()->SetImage(fg_skin);

	return OpStatus::OK;
}

void URLFavMenuButton::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	string.NeedUpdate();
	UpdateActionStateIfNeeded();
	GetInfo()->GetPreferedSize(this, OpTypedObject::WIDGET_TYPE_BUTTON, w, h, cols, rows);
}

OP_STATUS URLFavMenuButton::GetText(OpString& text)
{
	if (m_dialog_context && !m_dialog_context->IsDialogVisible())
		return OpButton::GetText(text);

	return OpStatus::OK;
}