/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author shuais
 */

#include "core/pch.h"
#include "OpProtocolButton.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick/widgets/OpIndicatorButton.h"
#include "adjunct/quick/managers/DesktopSecurityManager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/models/HistoryAutocompleteModel.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/quick-widget-names.h"
#include "adjunct/quick/animation/QuickAnimation.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/widgets/OpEdit.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/url/url_man.h"

#ifdef PUBLIC_DOMAIN_LIST
#include "modules/security_manager/include/security_manager.h"
#endif


/***********************************************************************************
**
**	OpProtocolButton
**
***********************************************************************************/

OpProtocolButton::OpProtocolButton()
: m_status(NotSet)
, m_hide_protocol(TRUE)
, m_target_width(0)
, m_indicator_button(NULL)
, m_security_button(NULL)
, m_text_button(NULL)
, m_security_mode(OpDocumentListener::UNKNOWN_SECURITY)
{
}

OP_STATUS OpProtocolButton::Init()
{
	SetButtonTypeAndStyle(TYPE_TOOLBAR, STYLE_IMAGE);
	SetName(WIDGET_NAME_ADDRESSFIELD_BUTTON_PROTOCOL);
	
	m_indicator_button = OP_NEW(OpIndicatorButton, (NULL, OpIndicatorButton::RIGHT));
	RETURN_OOM_IF_NULL(m_indicator_button);
	AddChild(m_indicator_button);
	RETURN_IF_ERROR(m_indicator_button->Init("Badge Indicator Skin"));
	m_indicator_button->SetName("Badge Indicator Button");

	RETURN_IF_ERROR(OpButton::Construct(&m_security_button, TYPE_TOOLBAR, STYLE_IMAGE));
	m_security_button->GetBorderSkin()->SetImage("Address Bar Speed Dial Icon");
	m_security_button->SetIgnoresMouse(TRUE);
	m_security_button->SetName("Badge Security Icon");
	AddChild(m_security_button);

	RETURN_IF_ERROR(OpButton::Construct(&m_text_button, TYPE_TOOLBAR, STYLE_TEXT));
	m_text_button->GetBorderSkin()->SetImage("Address Bar Web Text");
	m_text_button->SetJustify(JUSTIFY_LEFT, FALSE);
	m_text_button->SetIgnoresMouse(TRUE);
	m_text_button->SetName("Badge Text Button");
	AddChild(m_text_button);

	UpdateTargetWidth();

	SetName(WIDGET_NAME_ADDRESSFIELD_BUTTON_PROTOCOL);

	return OpStatus::OK;
}

BOOL OpProtocolButton::SameRootDomain(OpStringC url1_s, OpStringC url2_s)
{
	URL url1 = urlManager->GetURL(url1_s);
	URL url2 = urlManager->GetURL(url2_s);

	if (url1.GetServerName() && url2.GetServerName())
	{
#ifdef PUBLIC_DOMAIN_LIST
		ServerName* common = url1.GetServerName()->GetCommonDomain(url2.GetServerName());
		if (common && common->GetUniName().HasContent())
		{
			BOOL is_public_domain;
			RETURN_VALUE_IF_ERROR(g_secman_instance->IsPublicDomain(common->GetUniName(), is_public_domain), FALSE);
			return !is_public_domain;
		}
#else
		return url1.GetServerName().GetUniName().CompareI(url2.GetServerName().GetUniName()) == 0;
#endif
	}
	return FALSE;
}

void OpProtocolButton::UpdateStatus(OpWindowCommander* win_comm, BOOL mismatch)
{
	if (!win_comm)
	{
		// Switched to an non document tab
		SetStatus(Empty, TRUE);
		return;
	}

	DocumentDesktopWindow* win = g_application->GetDesktopWindowCollection().GetDocumentWindowFromCommander(win_comm);
	
	OpDocumentListener::TrustMode trust_mode = win_comm->GetTrustMode();
	OpDocumentListener::SecurityMode sec_mode = win_comm->GetSecurityMode();
	BOOL loading_failed = FALSE;
	if (win)
	{
		trust_mode = win->GetTrustMode();
		loading_failed = win->LoadingFailed();
	}

	const uni_char* url = win_comm->GetCurrentURL(FALSE);

	if (url && win_comm->GetURLType() == DocumentURLType_HTTPS)
	{
		if (sec_mode == OpDocumentListener::UNKNOWN_SECURITY)
		{
			if (SameRootDomain(m_last_url, url))
				sec_mode = m_security_mode;
		}
		else
		{
			m_last_url.Set(url);
			m_security_mode = sec_mode;
		}
	}

	if (win && win->IsSpeedDialActive())
	{
		SetStatus(win_comm->GetTurboMode() ? Turbo : SpeedDial);
	}
	else if (mismatch || loading_failed || !url || !*url
		|| uni_stricmp(url,UNI_L("opera:blank")) == 0 || uni_stricmp(url,UNI_L("about:blank")) == 0)
	{
		// If url doesn't match the page(user is typing etc), show turbo
		// if it's on, otherwise Empty.
		// If we're switching to Empty badge because of loading failed
		// we want to set force to true otherwise the badge won't resize
		// since address field is probably focused in this case.
		SetStatus(win_comm->GetTurboMode() ? Turbo : Empty, loading_failed);
	}
	else if (url && (uni_strnicmp(url, UNI_L("file://"), 7) == 0))
	{
		SetStatus(Local);
	}
	else if (url && (uni_strnicmp(url, UNI_L("attachment:"), 11) == 0))
	{
		SetStatus(Attachment);
	}
	else if (trust_mode == OpDocumentListener::PHISHING_DOMAIN)
	{
		SetStatus(Fraud);
	}
	else if (trust_mode == OpDocumentListener::MALWARE_DOMAIN)
	{
		SetStatus(Malware);
	}
	else if (url && (uni_strnicmp(url, UNI_L("opera:speeddial"), 15) == 0 ))
	{
		SetStatus(SpeedDial);
	}
	else if (url && (uni_strnicmp(url, UNI_L("opera:"), 6) == 0 ))
	{
		SetStatus(Opera);
	}
#ifdef EXTENSION_SUPPORT
	else if (url && uni_strnicmp(url, UNI_L("widget://wuid-"), 14) == 0)
	{
		m_last_url.Set(url);
		SetStatus(Widget);
	}
#endif
	else if (g_desktop_security_manager->IsEV(sec_mode))
	{
		SetStatus(Trusted);
	}
	else if (sec_mode == OpDocumentListener::HIGH_SECURITY)
	{
		SetStatus(Secure);
	}
	else
	{
		DocumentURLType type = win_comm->GetURLType();
		switch(type)
		{
		case DocumentURLType_HTTP:
			SetStatus (!win_comm->IsIntranet() && win_comm->GetTurboMode() ? Turbo : Web);
			break;
		case DocumentURLType_FTP:
		case DocumentURLType_HTTPS:
			SetStatus(Web);
			break;
		default:
			SetStatus(Empty);
			break;
		}
	}
}

void OpProtocolButton::OnAdded()
{
	SetUpdateNeededWhen(UPDATE_NEEDED_ALWAYS);
}

void OpProtocolButton::OnLayout()
{
	OpRect rect = GetBounds();
	AddPadding(rect);

	if (m_indicator_button->IsVisible())
	{
		OpRect indicator_rect = rect;
		indicator_rect.width = m_indicator_button->GetPreferredWidth();
		indicator_rect.height = m_indicator_button->GetPreferredHeight();
		indicator_rect.y += (rect.height - indicator_rect.height) / 2;
		SetChildRect(m_indicator_button, indicator_rect);
		rect.x += indicator_rect.width;
	}

	OpRect icon_rect = rect;
	m_security_button->GetBorderSkin()->GetSize(&icon_rect.width, &icon_rect.height);
	rect.x += icon_rect.width;
	icon_rect.y += (rect.height - icon_rect.height) / 2;
	m_security_button->GetBorderSkin()->AddPadding(icon_rect);
	SetChildRect(m_security_button, icon_rect);

	if (m_text_button->IsVisible())
	{
		INT32 dummy;
		OpRect text_rect = rect;
		m_text_button->GetRequiredSize(dummy, text_rect.height);
		text_rect.y += (rect.height - text_rect.height) / 2;
		m_text_button->GetBorderSkin()->AddPadding(text_rect);
		SetChildRect(m_text_button, text_rect);
	}
}

void OpProtocolButton::SetStatus(ProtocolStatus status, BOOL force)
{
	if (m_status == status || !IsVisible())
		return;

	// When loading a url from speeddial never change the SpeedDial 
	// Badge to Empty, let it go directly to Web/Secure/Trusted whatever
	if (m_status == SpeedDial && status == Empty)
	{
		return;
	}
	SetStatusInternal(status);
}

void OpProtocolButton::SetStatusInternal(ProtocolStatus status)
{
	OpInputAction* action = NULL;
	m_status = status;
	m_hide_protocol = TRUE;
	OpString8 foreground_skin;
	OpString8 border_skin;
	Str::LocaleString text_id(Str::NOT_A_STRING);

	switch (m_status)
	{
	case Fraud:
		border_skin.Set("Address Bar Fraud");
		foreground_skin.Set("Address Bar Fraud Icon");
		text_id = Str::S_BLOCKED_SITE_TITLE_FRAUD;
		action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_ADDRESSBAR_OVERLAY));
		break;
	case Malware:
		border_skin.Set("Address Bar Malware");
		foreground_skin.Set("Address Bar Malware Icon");
		text_id = Str::S_BLOCKED_SITE_TITLE_MALWARE;
		action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_ADDRESSBAR_OVERLAY));
		break;
	case Trusted:
		border_skin.Set("Address Bar Trusted");
		foreground_skin.Set("Address Bar Trusted Icon");
		text_id = Str::S_ADDRESS_BUTTON_TRUSTED;
		action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_ADDRESSBAR_OVERLAY));
		break;
	case Secure:
		border_skin.Set("Address Bar Secure");
		foreground_skin.Set("Address Bar Secure Icon");
		text_id = Str::S_ADDRESS_BUTTON_SECURE;
		action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_ADDRESSBAR_OVERLAY));
		break;
	case Turbo:
		border_skin.Set("Address Bar Turbo");
		foreground_skin.Set("Address Bar Turbo Icon");
		text_id = Str::S_ADDRESS_BUTTON_TURBO;
		action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_ADDRESSBAR_OVERLAY));
		break;
	case Web:
		border_skin.Set("Address Bar Web");
		foreground_skin.Set("Address Bar Web Icon");
		text_id = Str::S_ADDRESS_BUTTON_WEB;
		action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_ADDRESSBAR_OVERLAY));
		break;
	case Opera:
		border_skin.Set("Address Bar Internal");
		foreground_skin.Set("Address Bar Internal Icon");
		text_id = Str::S_ADDRESS_BUTTON_OPERA;
		break;
#ifdef EXTENSION_SUPPORT
	case Widget:
		border_skin.Set("Address Bar Addon");
		foreground_skin.Set("Address Bar Addon Icon");
		text_id = Str::S_ADDRESS_BUTTON_ADDON;
		break;
#endif
	case Local:
		border_skin.Set("Address Bar Local");
		foreground_skin.Set("Address Bar Local Icon");
		text_id = Str::S_ADDRESS_BUTTON_LOCAL;
		action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_ADDRESSBAR_OVERLAY));
		break;
	case Attachment:
		border_skin.Set("Address Bar Attachment");
		foreground_skin.Set("Address Bar Attachment Icon");
		text_id = Str::D_MAILER_SAVE_ATTATCHMENTS_HEADER;
		break;
	case Empty:
		border_skin.Set("Address Bar Empty");
		foreground_skin.Set("Address Bar Empty Icon");
		m_hide_protocol = FALSE;
		break;
	case SpeedDial:
		border_skin.Set("Address Bar Speeddial");
		foreground_skin.Set("Address Bar Speed Dial Icon");
		m_hide_protocol = FALSE;
		break;
	default:
		border_skin.Empty();
		foreground_skin.Empty();
		m_status = NotSet;
		break;
	}

	if (action)
	{
		OpString tooltip;
		if (OpStatus::IsSuccess(g_languageManager->GetString(Str::S_INFOPANEL_TITLE, tooltip)))
		{
			action->GetActionInfo().SetTooltipText(tooltip.CStr());
			action->GetActionInfo().SetStatusText(tooltip.CStr());
		}
	}

	SetAction(action);
	GetBorderSkin()->SetImage(border_skin.CStr());
	m_security_button->GetBorderSkin()->SetImage(foreground_skin.CStr());

	OpString status_text;
	if (text_id != Str::NOT_A_STRING &&
			OpStatus::IsSuccess(g_languageManager->GetString(text_id, status_text)))
	{
		m_text_button->SetText(status_text);
		m_text_button->SetVisibility(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowFullURL) == 0);
	}
	else
	{
		m_text_button->SetVisibility(FALSE);
	}

	UpdateTargetWidth();	
}

void OpProtocolButton::OnUpdateActionState()
{
	OpWidget* parent = GetParent();
	if (parent && parent->GetType() == WIDGET_TYPE_ADDRESS_DROPDOWN)
	{
		OpWindowCommander* win = ((OpAddressDropDown*)parent)->GetWindowCommander();
		BOOL mismatch = ((OpAddressDropDown*)parent)->GetMisMatched();
		UpdateStatus(win, mismatch);
	}
	OpButton::OnUpdateActionState();
}

void OpProtocolButton::StartAnimation(bool is_expanding)
{
	QuickAnimationWidget* animation = m_text_button->GetAnimation();
	if (animation && animation->IsAnimating())
	{
		g_animation_manager->abortAnimation(animation);
		/* We should not delete animation object here as it will be
		   destroyed in QuickOpWidgetBase::SetAnimation which is
		   called in the contructor of QuickAnimationWidget.
		 */
	}

	/* This is a small hack to get valid target width.
	   First we set proper visibility flag for text
	   button (TRUE when we are showing it, and FALSE
	   when we are hiding it) and after we update target
	   width we show the text as it should fade in/out.
	   When the animation is finished animator object
	   will hide the text when animation fade type is set
	   to ANIM_FADE_OUT.
	*/
	m_text_button->SetVisibility(is_expanding);
	UpdateTargetWidth();
	m_text_button->SetVisibility(TRUE);

	animation = OP_NEW(QuickAnimationWidget,
			(m_text_button,
			 is_expanding ? ANIM_MOVE_GROW : ANIM_MOVE_SHRINK,
			 true,
			 is_expanding ? ANIM_FADE_IN : ANIM_FADE_OUT));
	if (animation)
	{
		g_animation_manager->startAnimation(animation, ANIM_CURVE_SLOW_DOWN, ANIMATION_INTERVAL);
	}
}

void OpProtocolButton::UpdateTargetWidth()
{
	INT32 dummy;
	GetPreferredSize(m_target_width, dummy, false);
}

void OpProtocolButton::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	GetPreferredSize(*w, *h, true);
}

void OpProtocolButton::GetPreferredSize(INT32& width, INT32& height, bool with_animation)
{
	m_security_button->GetBorderSkin()->GetSize(&width, &height);
	if (m_indicator_button->IsVisible())
	{
		width += m_indicator_button->GetPreferredWidth();
		INT32 left, top, right, bottom;
		m_indicator_button->GetBorderSkin()->GetMargin(&left, &top, &right, &bottom);
		width += right;
	}

	if (m_text_button->IsVisible())
	{
		INT32 text_button_width, dummy;
		m_text_button->GetPreferedSize(&text_button_width, &dummy, 0, 0);
		if (with_animation)
		{
			QuickAnimationWidget* animation = m_text_button->GetAnimation();
			if (animation && animation->IsAnimating())
			{
				animation->GetCurrentValue(text_button_width, dummy);
			}
		}
		width += text_button_width;
	}

	width += GetPaddingLeft() + GetPaddingRight();

}

BOOL OpProtocolButton::OnContextMenu(const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
{
	g_application->GetMenuHandler()->ShowPopupMenu("Protocol Button Context Menu", PopupPlacement::AnchorAtCursor(), 0, keyboard_invoked);
	return TRUE;
}

void OpProtocolButton::AddIndicator(OpIndicatorButton::IndicatorType type)
{
	m_indicator_button->AddIndicator(type);
	ResetRequiredSize();
	UpdateTargetWidth();
}

void OpProtocolButton::RemoveIndicator(OpIndicatorButton::IndicatorType type)
{
	m_indicator_button->RemoveIndicator(type);
	ResetRequiredSize();
	UpdateTargetWidth();
}

void OpProtocolButton::SetTextVisibility(bool vis)
{
	m_text_button->SetVisibility(vis);
	UpdateTargetWidth();
	SetUpdateNeeded(TRUE);
	Relayout();
}

bool OpProtocolButton::ShouldShowText() const
{
	return m_status != SpeedDial &&
		m_status != NotSet &&
		m_status != Empty &&
		g_pcui->GetIntegerPref(PrefsCollectionUI::ShowFullURL) == 0;
}

void OpProtocolButton::ShowText()
{
	if (ShouldShowText())
	{
		StartAnimation(true);
	}
}

void OpProtocolButton::HideText()
{
	if (m_text_button->IsVisible())
	{
		StartAnimation(false);
	}
}
