// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//
#ifndef __MID_CLICK_DIALOG_H__
#define __MID_CLICK_DIALOG_H__

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class OpPage;
class OpView;
class DocumentManager;
class Window;

# ifdef WIC_MIDCLICK_SUPPORT
class MidClickManager
{
public:
	class PanningPageListener : public OpPageListener
	{
		public:
			void ListenPage(OpPage* page)
			{
				if (m_page != page)
				{
					if (m_page)
						m_page->RemoveListener(this);
					if (page)
						page->AddListener(this);
				}
			}
			virtual void OnPageClose(OpWindowCommander* commander)
			{
				MidClickManager::StopPanning();
			}
	};

	/*
	 * Called on center mouse click
	 *
	 * @param commander The window commander
	 * @param context The document context
	 * @param page The page that received the click. Can be 0
	 * @param mousedown TRUE if this happened on mouse down
	 * @param modifiers Current modifier state 
	 * @param is_gadget TRUE if function was called from a gadget. TODO: Param. should not be required
	 */
	static void OnMidClick(OpWindowCommander * commander, OpDocumentContext& context, OpPage* page,
						   BOOL mousedown, ShiftKeyState modifiers, BOOL is_gadget);
	static void OpenCurrentClickedURL(OpWindowCommander *commander, OpDocumentContext &context, BOOL is_gadget, BOOL test_only);

	static void StopPanning();
private:
	/** Checks if middle click should sent a mail.
	 *
	 * @param[in] context The document context.
	 * @param[in] test_only
	 *
	 * @return True if mail should be sent.
	 */
	static bool IsSentMail(OpDocumentContext& context, BOOL test_only);

	/** Opens mail form.
	 *
	 * @param[in] context The document context.
	 */
	static void SentMail(OpDocumentContext& context);
	static PanningPageListener panning_listener;
};

/**
 * Introduced with fix for DSK-358546. This class is used to open URL after
 * middle click without suspending handling event in HTML_Element by
 * unknown external protocol dialog. After handling a sent message object
 * deletes itself, so do not create object on a stack!
 */
class MidClickUrlOpener : public MessageObject
{
public:
	/**
	 * Opens Url by sending a message to itself, this causes that current
	 * procedure will be not blocked in case of external protocol dialog.
	 *
	 * @param[in] commander Used to set URL src_commander
	 * @param[in] context  Used to set URL address
	 * @param[in] is_gadget Informs if call is from a gadget
	 * @param[in] test_only
	 */
	OP_STATUS OpenUrl(OpWindowCommander * commander, OpDocumentContext& context, BOOL is_gadget, BOOL test_only);

	/**
	 * Opens Url after receiving MSG_OPEN_URL_AFTER_MIDDLE_CLICK.
	 * After hendling object deletes itself.
	 */
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:

	OpenURLSetting m_setting;
};
# endif // WIC_MIDCLICK_SUPPORT

class MidClickDialog : public Dialog
{
public:
	~MidClickDialog();

	static void Create(DesktopWindow* parent_window);
	static BOOL FirstTime(DesktopWindow* parent_window );

	virtual void				OnInit();
	virtual void 				OnInitVisibility();
	virtual UINT32				OnOk();
	void						OnChange(OpWidget *widget, BOOL changed_by_mouse);

	virtual BOOL				GetModality()			{return TRUE;}
	virtual BOOL				GetIsBlocking()			{return TRUE;}
	virtual Type				GetType()				{return DIALOG_TYPE_MIDCLICK_PREFERENCES;}
	virtual const char*			GetWindowName()			{return "Midclick Dialog";}

	INT32						GetButtonCount() { return 2; };
	void						GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name);
	virtual BOOL				OnInputAction(OpInputAction* action);

private:
	static MidClickDialog* m_dialog;
};

#endif
