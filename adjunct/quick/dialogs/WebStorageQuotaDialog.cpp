/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Petter Nilsen (pettern)
 */

#include "core/pch.h"

#if defined SUPPORT_DATABASE_INTERNAL

#include "adjunct/quick/dialogs/WebStorageQuotaDialog.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpDropDown.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/windowcommander/OpWindowCommander.h"

static const int ratios[] =
{
	2, 3, 4, 8, 16
};

// max accepted web site storage hint to show as default selected is 2GB
static const OpFileLength MAX_WEBSTORAGE_QUOTA_HINT = 2u * 1024 * 1024 * 1024;

WebStorageQuotaDialog::WebStorageQuotaDialog(const uni_char *server_name, OpFileLength current_limit, OpFileLength requested_limit, UINTPTR id) : Dialog(),
	m_current_limit(current_limit),
	m_requested_limit(requested_limit),
	m_quota_type(WebStorageQuota),
	m_id(id),
	m_callback(NULL)
{
	m_server_name.Set(server_name);
}

/***********************************************************************************
**  WebStorageQuotaDialog::OnInit
************************************************************************************/
void
WebStorageQuotaDialog::OnInit()
{
	OpWidget* label = GetWidgetByName("webstorage_description");
	if(label)
	{
		OpString text;
		OpString final_string;
		int expected_len;

		// format the description and replace %1 with the site domain
		if(OpStatus::IsSuccess(label->GetText(text)))
		{
			expected_len = text.Length() + 256;
			uni_char* mess_p = final_string.Reserve(expected_len);
			if (mess_p)
			{
				uni_snprintf_ss(mess_p, expected_len, text.CStr(), m_server_name.CStr(), NULL);
				label->SetText(final_string.CStr());
			}
		}
	}
	label = GetWidgetByName("webstorage_current_limit");
	if(label)
	{
		// show the current limit in a nice localized format
		OpString sizestring;

		if(sizestring.Reserve(64))
		{
			StrFormatByteSize(sizestring, m_current_limit, SFBS_ABBRIVATEBYTES);
			label->SetText(sizestring.CStr());
		}
	}	
	OpDropDown* dropdown = (OpDropDown*)GetWidgetByName("webstorage_requested_limit");
	if(dropdown)
	{
		BOOL found_default = FALSE;

		// add all the different predefined sizes as ratios of the current limit
		if ( m_current_limit != 0)
		{
			for (size_t i = 0; i < ARRAY_SIZE(ratios); i++)
			{
				OpString sizestring;

				UINT32 size = m_current_limit * ratios[i];

				if(sizestring.Reserve(64))
				{
					StrFormatByteSize(sizestring, size, SFBS_ABBRIVATEBYTES);

					dropdown->AddItem(sizestring.CStr(), -1, NULL, (size / 1024));	// divide by 1024 to avoid overflow

					// make the requested limit from the web page the default, unless it's above 2GB
					if(size == m_requested_limit && m_requested_limit <= MAX_WEBSTORAGE_QUOTA_HINT)
					{
						found_default = TRUE;
						dropdown->SelectItem(i, TRUE);
					}
				}
			}
		}
		if(!found_default && m_requested_limit)
		{
			// add and select the requested value
			OpString sizestring;

			if(sizestring.Reserve(64))
			{
				INT32 index;
				StrFormatByteSize(sizestring, m_requested_limit, SFBS_ABBRIVATEBYTES);

				if(OpStatus::IsSuccess(dropdown->AddItem(sizestring.CStr(), -1, &index, (m_requested_limit / 1024))))
				{
					if(m_requested_limit <= MAX_WEBSTORAGE_QUOTA_HINT)
					{
						dropdown->SelectItem(index, TRUE);
					}
				}
			}
		}
		// add an unlimited entry at the end
		OpString str;

		g_languageManager->GetString(Str::S_UNLIMITED, str);

		dropdown->AddItem(str.CStr(), -1, NULL);
	}
	CompressGroups();
}

/***********************************************************************************
**  GeolocationPrivacyDialog::OnOk
************************************************************************************/
UINT32
WebStorageQuotaDialog::OnOk()
{
	OpFileLength limit = 0;
	OpDropDown* dropdown = (OpDropDown*)GetWidgetByName("webstorage_requested_limit");
	if(dropdown)
	{
		limit = (INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());
	}

	if (m_quota_type == ApplicationCacheQuota)
	{
		if (m_callback)
			m_callback->OnQuotaReply(TRUE, limit * 1024);
	}
	else
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_ACCEPT_WEBSTORAGE_QUOTA_REQUEST, limit, NULL, GetParentInputContext());
	}
	return 1;
}

/***********************************************************************************
**  GeolocationPrivacyDialog::OnCancel
************************************************************************************/
void
WebStorageQuotaDialog::OnCancel()
{
	if (m_quota_type == ApplicationCacheQuota)
	{
		if (m_callback)
			m_callback->OnQuotaReply(FALSE, m_current_limit);
	}
	else
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_REJECT_WEBSTORAGE_QUOTA_REQUEST, 0, NULL, GetParentInputContext());
	}
}

void WebStorageQuotaDialog::OnClose(BOOL user_initiated)
{
	if(!user_initiated)
	{
		// tab got closed or similar, so we need to cancel the callback instead of replying
		if (m_quota_type != ApplicationCacheQuota)
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_CANCEL_WEBSTORAGE_QUOTA_REQUEST, 0, NULL, GetParentInputContext());
		}
	}
	Dialog::OnClose(user_initiated);
}

void WebStorageQuotaDialog::GetOverlayPlacement(OpRect& initial_placement, OpWidget* overlay_layout_widget)
{
	OpRect parent_rect = overlay_layout_widget->GetParent()->GetRect();
	initial_placement = OpRect((parent_rect.width - initial_placement.width) / 2, parent_rect.y, initial_placement.width, initial_placement.height);
	overlay_layout_widget->SetRect(initial_placement);
	overlay_layout_widget->SetXResizeEffect(RESIZE_CENTER);
}

void WebStorageQuotaDialog::SetApplicationCacheCallback(OpApplicationCacheListener::QuotaCallback* callback)
{
	m_callback = callback;
	m_quota_type = ApplicationCacheQuota;
}

#endif // SUPPORT_DATABASE_INTERNAL
