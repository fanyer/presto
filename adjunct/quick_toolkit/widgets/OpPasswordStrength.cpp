/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpPasswordStrength.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"
#include "adjunct/quick/managers/OperaAccountManager.h"

#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpEdit.h"
#include "modules/locale/oplanguagemanager.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_CONSTRUCT(OpPasswordStrength)

////////////////////////////////////////////////////////////////////////////////////////////////////////////

OpPasswordStrength::OpPasswordStrength()
	: m_pass_strength(PasswordStrength::NONE)
	, m_leds(NULL)
	, m_text(NULL)
	, m_password_listener(this)
{
	init_status = CreateControls();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/
OP_STATUS OpPasswordStrength::SetPasswordStrength(PasswordStrength::Level pass_strength)
{
	if (pass_strength != m_pass_strength)
	{
		m_pass_strength = pass_strength;
		return InternalSetPasswordStrength();
	}
	return OpStatus::OK;
}

OP_STATUS OpPasswordStrength::GetText(OpString& text)
{
	return m_text ? m_text->GetText(text) : OpStatus::ERR_NULL_POINTER;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpPasswordStrength::CreateControls()
{
	SetSkinned(TRUE);
	GetForegroundSkin()->SetImage("Password Strength");
	
	m_leds = NULL;
	m_text = NULL;
	OP_STATUS status = OpButton::Construct(&m_text, OpButton::TYPE_CUSTOM, OpButton::STYLE_TEXT);
	RETURN_IF_ERROR(status);

	status = OpIcon::Construct(&m_leds);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(m_text);
		m_text = NULL;
		return status;
	}

	AddChild(m_leds);
	AddChild(m_text);
	
	m_text->SetRelativeSystemFontSize(90);
	m_text->GetForegroundSkin()->SetImage("Password Strength Label");
	m_text->SetJustify(JUSTIFY_LEFT, FALSE);
	
	m_leds->SetJustify(JUSTIFY_LEFT, FALSE);
	
	return InternalSetPasswordStrength();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpPasswordStrength::InternalSetPasswordStrength()
{
	Str::LocaleString lang_id = Str::S_LITERAL_PASSWORD_STRENGTH;
	const char* skin = "Password Strength Empty";
	switch(m_pass_strength)
	{
		case PasswordStrength::TOO_SHORT:   lang_id = Str::S_LITERAL_TOO_SHORT;         break;
		case PasswordStrength::VERY_WEAK:   lang_id = Str::S_LITERAL_VERY_WEAK;         break;
		case PasswordStrength::WEAK:        lang_id = Str::S_LITERAL_WEAK;              break;
		case PasswordStrength::MEDIUM:      lang_id = Str::S_LITERAL_MEDIUM;            break;
		case PasswordStrength::STRONG:      lang_id = Str::S_LITERAL_STRONG;            break;
		case PasswordStrength::VERY_STRONG: lang_id = Str::S_LITERAL_VERY_STRONG;       break;
		case PasswordStrength::NONE:  		lang_id = Str::S_LITERAL_PASSWORD_STRENGTH; break;
	};

	switch(m_pass_strength)
	{
		case PasswordStrength::TOO_SHORT:   skin = "Password Strength Too Short";   break;
		case PasswordStrength::VERY_WEAK:   skin = "Password Strength Very Weak";   break;
		case PasswordStrength::WEAK:        skin = "Password Strength Weak";        break;
		case PasswordStrength::MEDIUM:      skin = "Password Strength Medium";      break;
		case PasswordStrength::STRONG:      skin = "Password Strength Strong";      break;
		case PasswordStrength::VERY_STRONG: skin = "Password Strength Very Strong"; break;
		case PasswordStrength::NONE:  		skin = "Password Strength Empty";       break;
	};

	OpString text;
	RETURN_IF_ERROR(g_languageManager->GetString(lang_id,text));
	m_leds->SetImage(skin);
	RETURN_IF_ERROR(m_text->SetText(text));
	Relayout();
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpPasswordStrength::InternalLayout(Padding here, Padding leds, Padding text)
{
	SetChildRect(m_leds, leds.GetControlRect(here.left, here.top));
	SetChildRect(m_text, text.GetControlRect(here.left, here.top + leds.GetTotalHeight()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpPasswordStrength::OnLayout()
{
	Padding here(0, 0, this);
	Padding leds(m_leds);
	Padding text(m_text);

	InternalLayout(here, leds, text);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpPasswordStrength::OnResize(INT32* new_w, INT32* new_h)
{
	Padding here(0, 0, this);
	Padding leds(m_leds);
	Padding text(m_text);

	InternalLayout(here, leds, text);

	here.width = leds.GetTotalWidth();
	INT32 width = text.GetTotalWidth();
	if (here.width < width) here.width = width;
	here.height = leds.GetTotalHeight() + text.GetTotalHeight();

	*new_w = here.GetTotalWidth();
	*new_h = here.GetTotalHeight();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpPasswordStrength::OnPasswordChanged(OpEdit* edit)
{
	OpString text;
	OP_ASSERT(edit && "Did someone broke AttachToEdit?");
	RETURN_VOID_IF_ERROR(edit->GetText(text));
	
	PasswordStrength::Level new_type = PasswordStrength::NONE;

	if (text.HasContent())
		new_type = PasswordStrength::Check(text, OperaAccountContext::GetPasswordMinLength());
	
	SetPasswordStrength(new_type);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpPasswordStrength::AttachToEdit(OpEdit* edit)
{
	if (!edit) return;
	edit->SetListener(&m_password_listener);
	OnPasswordChanged(edit);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpPasswordStrength::PassListener::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	OP_ASSERT(widget->IsOfType(WIDGET_TYPE_EDIT)  && "Did someone broke AttachToEdit?");
	parent->OnPasswordChanged(static_cast<OpEdit*> (widget));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpPasswordStrength::PassListener::OnChangeWhenLostFocus(OpWidget *widget)
{
	OP_ASSERT(widget->IsOfType(WIDGET_TYPE_EDIT)  && "Did someone broke AttachToEdit?");
	parent->OnPasswordChanged(static_cast<OpEdit*> (widget));
}
