/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/**
 * @file OpTrustAndSecurityButton.h
 *
 * Button that is used to display encryption and trust information in the address bar.
 *
 * @author rfz@opera.com (created by mariusab@opera.com)
 */

#ifndef OP_TRUST_AND_SECURITY_BUTTON_H
#define OP_TRUST_AND_SECURITY_BUTTON_H

#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "modules/widgets/OpButton.h"

/**
 * Class OpTrustAndSecurityButton is the button  used to
 * display encryption and trust information in the address bar.
 *
 * The rationale for making this button its own class is to
 * collect all logic related to appearance and visibility of
 * this button in one, easily maintainable, place.
 *
 * Note that the Type of this derived class when asked will be
 * that of the superclass, OpButton and OpWidgetInfo will use
 * that since this class does not overload the method.
 */
class OpTrustAndSecurityButton : public OpButton
{
public:
	/**
	 * Constructor.
	 */
	OpTrustAndSecurityButton();

	static OP_STATUS Construct(OpTrustAndSecurityButton** obj);
	/**
	 * Set trust and security level based on windowcommander.
	 *
	 * @param button_text The cn (lo) text on the button if it exists.
	 * @param wic Commander of window in question, provides information about page.
	 * @param show_server_name don't use elablorate info from certificates etc, but use server name
	 * @param always_visible don't let visibility depend on whether the connection to the server is secure
	 *
	 * @return TRUE if the state of the button changed, false otherwise.
	 */
	BOOL SetTrustAndSecurityLevel(	const OpString &button_text, OpWindowCommander* wic,
									OpAuthenticationCallback* callback = NULL,
									BOOL show_server_name = FALSE, BOOL always_visible = FALSE);
	/**
	 * Function to get easy access to whether the button needs to be visible,
	 * based on the state of the document passed it with
	 * SetTrustAndSecurityLevel. The rules for the button visibility
	 * are collected in this function.
	 */
	BOOL ButtonNeedsToBeVisible();
	void SuppressText(BOOL suppress);

	virtual const char*	GetInputContextName() {return "TrustAndSecurity Button";}

	// We never want this button to have focus so override the function
	virtual void SetFocus(FOCUS_REASON reason) {}

	/**
	 *
	 * @param rect
	 * @param height
	 */
	void GetRequiredSize(INT32& width, INT32& height);

	/**
	 *	If the type of URL is https return TRUE
	 */
#ifdef SECURITY_INFORMATION_PARSER
	BOOL URLTypeIsHTTPS() { return m_url_type == OpWindowCommander::WIC_URLType_HTTPS; }
#else // SECURITY_INFORMATION_PARSER
	BOOL URLTypeIsHTTPS() { return FALSE; }
#endif // SECURITY_INFORMATION_PARSER

	virtual BOOL HasToolTipText(OpToolTip* tooltip) { return string.GetWidth() > GetWidth(); }
	virtual INT32 GetToolTipDelay(OpToolTip* tooltip) { return 0; } // Show this right away
	virtual BOOL AlwaysShowHiddenContent() { return string.GetWidth() > GetWidth(); }

private:
	// Functions to move the actual button modification out of the
	// structure that selects based on the mode and level, in order
	// to make that set of if/elsifs more readable.
	void SetFraudAppearance(); ///< Sets the button to indicate fraud
	void SetSecureAppearance(const OpString &button_text); ///< Sets the button to indicate security.
	void SetInsecureAppearance();
	void SetUnknownAppearance(); ///< Sets the button to indicate unknown site
	void SetKnownAppearance(); ///< Sets the button to indicate known, trusted site
	void SetHighAssuranceAppearance(const OpString &button_text); ///< Sets the button to indicate that the site has extended validation.
	OP_STATUS SetText(const uni_char* text);
	// The following may or may not be needed...
	/**
	 * The security mode of the document in question
	 */
	OpDocumentListener::SecurityMode m_sec_mode;
	BOOL m_suppress_text;
#ifdef TRUST_RATING
	/**
	 * The trust level of the document as fetched from the server.
	 */
	OpDocumentListener::TrustMode m_trust_mode;
#endif // TRUST_RATING
	/**
	 * The server name of the document.
	 */
	OpString m_server_name;
	OpString m_button_text;
#ifdef SECURITY_INFORMATION_PARSER
	OpWindowCommander::WIC_URLType m_url_type;
#endif // SECURITY_INFORMATION_PARSER
};

#endif //OP_TRUST_AND_SECURITY_BUTTON_H
