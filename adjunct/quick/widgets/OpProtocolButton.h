/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author shuais
 */

#ifndef OP_PROTOCOL_BUTTON_H
#define OP_PROTOCOL_BUTTON_H

#include "modules/widgets/OpButton.h"
#include "adjunct/quick/widgets/OpIndicatorButton.h"
#include "adjunct/quick/managers/AnimationManager.h"

class OpAddressDropDown;

// The button which shows the protocol/security status/turbo status of the url
class OpProtocolButton : public OpButton
{
public:
	OpProtocolButton();

	OP_STATUS Init();

	enum ProtocolStatus
	{
		Fraud,
		Malware,
		Trusted,
		Secure,
		Turbo,
		Web,
		Opera,
		Local,
		Attachment,
#ifdef EXTENSION_SUPPORT
		Widget,
#endif
		SpeedDial,
		Empty,	// Blank, when user is typing an address
		NotSet	// Not initialized
	};

	// @param mismatch The text of the address bar doesn't match the page, this happens when user edited the url
	void UpdateStatus(OpWindowCommander*, BOOL mismatch);
	ProtocolStatus GetStatus() {return m_status;}

	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	virtual void OnAdded();
	virtual void OnLayout();
	virtual BOOL OnContextMenu(const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked);

	// In case turbo state change we need to update the button
	virtual void OnUpdateActionState();

	// When animating this is the width we're growing/shrinking to
	INT32 GetTargetWidth() const { return m_target_width; }
	BOOL HideProtocol() {return m_hide_protocol;}
	void SetStatus(ProtocolStatus status, BOOL force = FALSE);

	void AddIndicator(OpIndicatorButton::IndicatorType type);
	void RemoveIndicator(OpIndicatorButton::IndicatorType type);

	OpIndicatorButton* GetIndicatorButton() const { return m_indicator_button; }

	/** Show security text (Web/Secure/Trusted). This method starts badge animation. */
	void ShowText();

	/** Hide security text (Web/Secure/Trusted). This method starts badge animation. */
	void HideText();

	void SetTextVisibility(bool vis);

private:
	static const unsigned ANIMATION_INTERVAL = 150;

	void GetPreferredSize(INT32& width, INT32& height, bool with_animation);

	void UpdateTargetWidth();

	void SetCurrentFactor(double factor);

	void StartAnimation(bool is_expanding);

	void SetStatusInternal(ProtocolStatus status);

	// Get preferred width for a badge
	INT32 GetPreferredWidthFor(Str::LocaleString str);
	
	ProtocolStatus m_status;
	// Some badges don't want to hide protocol, for instance Empty badge
	BOOL m_hide_protocol; 

	INT32 m_target_width;

	OpIndicatorButton* m_indicator_button;
	OpButton* m_security_button;
	OpButton* m_text_button;

	// Hack time(CORE-34360), remember the trust mode of the server so that when 
	// reloading we assume that mode since core will reset the mode
	// to Unknown which cause flickers!
	OpString m_last_url;
	OpDocumentListener::SecurityMode m_security_mode;
	BOOL SameRootDomain(OpStringC url1, OpStringC url2);
	bool ShouldShowText() const;
};

#endif
