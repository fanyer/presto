/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef PASSWORDIDALOG_H
#define PASSWORDIDALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class OpAddressDropDown;

/***********************************************************************************
**
**  AuthenticationDialog
**
***********************************************************************************/
class PasswordDialog : public Dialog
{
private:
	OpAuthenticationCallback*			m_callback;
	URL_ID								m_authid;
	URLInformation*						m_url_info;
	OpWindowCommander*					m_commander;
	OpString							m_idstring;
	BOOL								m_clean_windowcommander;
	BOOL								m_attached_to_address_field;
	BOOL								m_addressbar_group_visible;
	OpWidget*							m_address_proxy_widget;
	BOOL								m_title_bar;
	static const BOOL EnableAttachingToAddressField =
#ifdef _MACINTOSH_
			FALSE;
#else
			TRUE;
#endif
public:

						PasswordDialog(OpAuthenticationCallback* callback, OpWindowCommander *commander, BOOL titlebar = TRUE);
    virtual				~PasswordDialog();

	Type				GetType()				{return DIALOG_TYPE_PASSWORD;}
	const char*			GetWindowName()			{return "Basic or Digest Authentication Dialog";}
	OpAuthenticationCallback* GetCallback()		{return m_callback;}
#ifdef VEGA_OPPAINTER_SUPPORT
	BOOL				GetModality()			{return FALSE;}
	BOOL				GetOverlayed()			{return TRUE;}
	BOOL				GetDimPage()			{return TRUE;}
	BOOL				GetIsWindowTool()		{return !m_title_bar;}
#ifdef _MACINTOSH_
	DialogAnimType		GetPosAnimationType()	{return POSITIONAL_ANIMATION_SHEET;}
#else
	DialogAnimType		GetPosAnimationType()	{return POSITIONAL_ANIMATION_NONE;}
#endif
#endif

	virtual BOOL        IsMovable() {return FALSE;}
	
	void				GetOverlayPlacement(OpRect& initial_placement, OpWidget* overlay_layout_widget);

	URL_ID				GetAuthID()				{ return m_authid; }

	virtual BOOL		GetDoNotShowAgain() {return TRUE;}
	virtual Str::LocaleString GetDoNotShowAgainTextID();

	virtual void		OnInitVisibility();
	void				OnInit();
	
	virtual	const char*	GetFallbackSkin();
	virtual const char*	GetPagesSkin() {return NULL;}
	virtual Str::LocaleString GetOkTextID();

    UINT32				OnOk();
    void				OnCancel();
	void				OnClose(BOOL user_initiated);
	void				OnAddressDropdownResize();

	BOOL				OnInputAction(OpInputAction* action);
	BOOL				HideWhenDesktopWindowInActive()	{ return TRUE; }

	OpAddressDropDown*	GetAddressDropdown();
	BOOL				UseOverlayedSkin()			{return GetOverlayed() && GetParentDesktopWindow() && GetParentDesktopWindow()->GetBrowserView();}

	// == OpWidgetListener ======================

	virtual void		OnChange(OpWidget *widget, BOOL changed_by_mouse);
	virtual void		OnPaint(OpWidget *widget, const OpRect &paint_rect);
	
};

#endif //PASSWORDIDALOG_H
