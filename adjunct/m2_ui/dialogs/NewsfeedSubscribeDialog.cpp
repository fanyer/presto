/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2_ui/dialogs/NewsfeedSubscribeDialog.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/engine/engine.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"


/***********************************************************************************
 ** Initialization
 **
 ** NewsfeedSubscribeDialog::Init
 ** @param parent_window
 ** @param newsfeed_title Title of the newsfeed, will be used to refer to feed in dialog
 ** @param newsfeed_url URL to subscribe if user clicks Yes
 ***********************************************************************************/
OP_STATUS NewsfeedSubscribeDialog::Init(DesktopWindow* parent_window,
										const OpStringC& newsfeed_title,
										const OpStringC& newsfeed_url)
{
	// Save URL for use in OnOk()
	RETURN_IF_ERROR(m_newsfeed_url.Set(newsfeed_url));

	// Construct dialog
	OpString title;

	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_NEW_SUBSCRIPTION_WARNING, title));

	OpString message;

	RETURN_IF_ERROR(StringUtils::GetFormattedLanguageString(message,
					Str::D_NEW_SUBSCRIPTION_WARNING_TEXT_1,
					newsfeed_title.CStr() ? newsfeed_title.CStr() : UNI_L("")));

	OpString message_part;

	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_NEW_SUBSCRIPTION_DOUBLE_LINEBREAK, message_part));
	RETURN_IF_ERROR(message.Append(message_part));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_NEW_SUBSCRIPTION_WARNING_TEXT_2, message_part));
	RETURN_IF_ERROR(message.Append(message_part));

	return SimpleDialog::Init(WINDOW_NAME_NEWSFEED_SUBSCRIBE, title, message, parent_window, Dialog::TYPE_YES_NO, Dialog::IMAGE_QUESTION);
}


/***********************************************************************************
 ** Initialization
 **
 ** NewsfeedSubscribeDialog::Init
 ** @param parent_window
 ** @param downloaded_newsfeed_url URL object that contains downloaded newsfeed
 ***********************************************************************************/
OP_STATUS NewsfeedSubscribeDialog::Init(DesktopWindow* parent_window, URL& downloaded_newsfeed_url)
{
	m_downloaded_newsfeed_url = downloaded_newsfeed_url;

	OpString newsfeed_url;

	RETURN_IF_ERROR(m_downloaded_newsfeed_url.GetAttribute(URL::KUniName, newsfeed_url));

	return Init(parent_window, newsfeed_url, newsfeed_url);
}


/***********************************************************************************
 **
 **
 ** NewsfeedSubscribeDialog::OnOk
 ***********************************************************************************/
UINT32 NewsfeedSubscribeDialog::OnOk()
{
#ifdef M2_CAP_LOAD_RSS_FROM_URL
	OpStatus::Ignore(MessageEngine::GetInstance()->LoadRSSFeed(m_newsfeed_url, m_downloaded_newsfeed_url, TRUE));
#else
	OpStatus::Ignore(MessageEngine::GetInstance()->LoadRSSFeed(m_newsfeed_url, TRUE));
#endif // M2_CAP_LOAD_RSS_FROM_URL

	return SimpleDialog::OnOk();
}

#endif // M2_SUPPORT
