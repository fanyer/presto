/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickDialog.h"

#include "adjunct/quick_toolkit/widgets/QuickButtonStrip.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickIcon.h"
#include "adjunct/quick_toolkit/widgets/QuickScrollContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickSkinElement.h"


QuickDialog::QuickDialog()
  : QuickWindow(OpWindow::STYLE_MODAL_DIALOG, OpTypedObject::DIALOG_TYPE)
  , m_content_can_be_scrolled(TRUE)
  , m_header(NULL)
  , m_content(NULL)
  , m_buttonstrip(NULL)
{
}

void QuickDialog::SetDialogHeader(QuickWidget* header)
{
	m_header->SetContent(header);
	if (header)
		m_header->Show();
	else
		m_header->Hide();
}

OP_STATUS QuickDialog::SetDialogContent(QuickButtonStrip* button_strip, QuickWidget* main_content, const OpStringC8& alert_image)
{
	m_buttonstrip->SetContent(button_strip);
	if (button_strip)
		m_buttonstrip->Show();
	else
		m_buttonstrip->Hide();

	if (alert_image.HasContent())
		return ConstructAlertContent(main_content, alert_image);

	m_content->SetContent(main_content);
	return OpStatus::OK;
}

QuickWidget* QuickDialog::GetDialogContent() const
{
	return m_content->GetContent();
}

OP_STATUS QuickDialog::ConstructAlertContent(QuickWidget* content, const OpStringC8& alert_image)
{
	OpAutoPtr<QuickWidget> main_content(content);
	OpAutoPtr<QuickStackLayout> stack (OP_NEW(QuickStackLayout, (QuickStackLayout::HORIZONTAL)));
	OpAutoPtr<QuickIcon> icon (OP_NEW(QuickIcon, ()));
	if (!stack.get() || !icon.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(icon->Init());
	icon->SetImage(alert_image.CStr());

	RETURN_IF_ERROR(stack->InsertWidget(icon.release()));
	RETURN_IF_ERROR(stack->InsertWidget(main_content.release()));

	m_content->SetContent(stack.release());

	return OpStatus::OK;
}

OP_STATUS QuickDialog::Init(BOOL content_can_be_scrolled)
{
	m_content_can_be_scrolled = content_can_be_scrolled;
	return Init();
}

OP_STATUS QuickDialog::Init()
{
	RETURN_IF_ERROR(QuickWindow::Init());

	OpAutoPtr<QuickSkinElement> header(OP_NEW(QuickSkinElement, ()));
	OpAutoPtr<QuickSkinElement> content(OP_NEW(QuickSkinElement, ()));
	OpAutoPtr<QuickSkinElement> buttonstrip(OP_NEW(QuickSkinElement, ()));
	OpAutoPtr<QuickStackLayout> stack(OP_NEW(QuickStackLayout, (QuickStackLayout::VERTICAL)));
	if (!header.get() || !content.get() || !buttonstrip.get() || !stack.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(header->Init());
	RETURN_IF_ERROR(content->Init());
	RETURN_IF_ERROR(buttonstrip->Init());

	m_header = header.release();
	RETURN_IF_ERROR(stack->InsertWidget(m_header));
	m_header->Hide();

	if (m_content_can_be_scrolled)
	{
		OpAutoPtr<QuickScrollContainer> scroll (OP_NEW(QuickScrollContainer,
				(QuickScrollContainer::VERTICAL, QuickScrollContainer::SCROLLBAR_AUTO)));
		RETURN_OOM_IF_NULL(scroll.get());
		RETURN_IF_ERROR(scroll->Init());
		m_content = content.release();
		scroll->SetContent(m_content);
		RETURN_IF_ERROR(stack->InsertWidget(scroll.release()));
	}
	else
	{
		m_content = content.release();
		RETURN_IF_ERROR(stack->InsertWidget(m_content));
	}

	m_buttonstrip = buttonstrip.release();
	RETURN_IF_ERROR(stack->InsertWidget(m_buttonstrip));
	m_buttonstrip->Hide();

	UpdateSkin();
	SetContent(stack.release());

	return OpStatus::OK;
}

OP_STATUS QuickDialog::SetName(const OpStringC8& window_name)
{
	RETURN_IF_ERROR(QuickWindow::SetName(window_name));
	UpdateSkin();
	return OpStatus::OK;
}

void QuickDialog::UpdateSkin()
{
	OpString8 header_skin;
	OpStatus::Ignore(header_skin.SetConcat(GetName(), " Header Skin"));
	m_header->SetSkin(header_skin, GetDefaultHeaderSkin());

	OpString8 content_skin;
	OpStatus::Ignore(content_skin.SetConcat(GetName(), " Page Skin"));
	m_content->SetSkin(content_skin, GetDefaultContentSkin());

	OpString8 buttonstrip_skin;
	OpStatus::Ignore(buttonstrip_skin.SetConcat(GetName(), " Button Border Skin"));
	m_buttonstrip->SetSkin(buttonstrip_skin, GetDefaultButtonSkin());
}
