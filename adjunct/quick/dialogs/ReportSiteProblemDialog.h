/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef REPORT_SITE_PROBLEM_DIALOG_H
#define REPORT_SITE_PROBLEM_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

/***********************************************************************************
**
**	ReportSiteProblemDialog
**
***********************************************************************************/

class ReportSiteProblemDialog : public Dialog
{
	public:
			
								ReportSiteProblemDialog();
		Type					GetType()				{return DIALOG_TYPE_REPORT_SITE_PROBLEM;}
		const char*				GetWindowName()			{return "Report Site Problem Dialog";}
		BOOL					GetModality()			{return FALSE;}
		const char*				GetHelpAnchor()			{return "reportsite.html";}

		Str::LocaleString		GetOkTextID();

		void					OnInit();
		UINT32					OnOk();
		void					OnCancel();

		static OP_STATUS		XMLEscapeString(OpString& string);

	private:

//		BOOL*					m_retval;
//		const uni_char*			m_title;
//		uni_char*				m_buffer;
//		int						m_buflen;

		OpString				m_user_agent;
//		OpString				m_build_number;
		OpString				m_popups;
		OpString				m_encoding;
		OpString				m_document_mode;
		OpString				m_images;
		OpString				m_cookie_normal;
		OpString				m_cookie_third_party;

		BOOL					m_java;
		BOOL					m_plugins;
		BOOL					m_javascript;
		BOOL					m_cookies;
		BOOL					m_cookies3;
		BOOL					m_referrer_logging;
		BOOL					m_proxy;
		BOOL					m_strict_mode; // strict or quirks mode rendering
};

#endif //ASK_TEXT_DIALOG_H
