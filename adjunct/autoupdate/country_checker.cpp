/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/country_checker.h"
#include "adjunct/autoupdate/updatablesetting.h"

CountryChecker::CountryChecker()
: m_status(NotInitialized)
, m_autoupdate_xml(NULL)
, m_status_xml_downloader(NULL)
, m_listener(NULL)
, m_autoupdate_server_url(NULL)
{
	m_timer.SetTimerListener(this);
}

CountryChecker::~CountryChecker()
{
	OP_DELETE(m_status_xml_downloader);
	OP_DELETE(m_autoupdate_server_url);
	OP_DELETE(m_autoupdate_xml);
}

OP_STATUS CountryChecker::Init(CountryCheckerListener* listener)
{
	if (m_status != NotInitialized)
		return OpStatus::ERR;

	m_listener = listener;
	m_status = InitFailed;

	OpAutoPtr<AutoUpdateXML> autoupdate_xml_guard(OP_NEW(AutoUpdateXML, ()));
	OpAutoPtr<StatusXMLDownloader> status_xml_downloader_guard(OP_NEW(StatusXMLDownloader, ()));
	OpAutoPtr<AutoUpdateServerURL> autoupdate_server_url_guard(OP_NEW(AutoUpdateServerURL, ()));

	RETURN_OOM_IF_NULL(autoupdate_xml_guard.get());
	RETURN_OOM_IF_NULL(status_xml_downloader_guard.get());
	RETURN_OOM_IF_NULL(autoupdate_server_url_guard.get());

	RETURN_IF_ERROR(autoupdate_xml_guard->Init());
	RETURN_IF_ERROR(status_xml_downloader_guard->Init(StatusXMLDownloader::CheckTypeOther, this));
	RETURN_IF_ERROR(autoupdate_server_url_guard->Init());

	m_autoupdate_xml = autoupdate_xml_guard.release();
	m_status_xml_downloader = status_xml_downloader_guard.release();
	m_autoupdate_server_url = autoupdate_server_url_guard.release();

	m_status = CheckNotPerformed;

	return OpStatus::OK;
}

OP_STATUS CountryChecker::CheckCountryCode(UINT32 timeout)
{
	if (m_status != CheckNotPerformed)
		return OpStatus::ERR;

	OP_ASSERT(m_autoupdate_server_url);
	OP_ASSERT(m_autoupdate_xml);
	OP_ASSERT(m_status_xml_downloader);

	m_country_code.Empty();

	m_status = CheckFailed;

	OpString url_string;
	RETURN_IF_ERROR(GetServerAddress(url_string));

	OpString8 xml_string;
	m_autoupdate_xml->ClearRequestItems();

	// RT_Other ensures that this request is not identified as the "main" autoupdate, see DSK-344690
	RETURN_IF_ERROR(m_autoupdate_xml->GetRequestXML(xml_string, AutoUpdateXML::UpdateLevelCountryCheck, AutoUpdateXML::RT_Other));
	RETURN_IF_ERROR(m_status_xml_downloader->StartXMLRequest(url_string, xml_string));

	if (timeout)
	{
		m_timer.Start(timeout);
	}
	else
	{
		m_timer.Stop();
	}

	m_status = CheckInProgress;
	return OpStatus::OK;
}

OP_STATUS CountryChecker::GetServerAddress(OpString& address)
{
	OpString current_url;
	RETURN_IF_ERROR(m_autoupdate_server_url->GetCurrentURL(current_url));

	// Country check should be as fast as possible, so http is preferred over https. However
	// we switch to http only if current_url matches one of the default addresses. (DSK-353772)
	if (current_url.Compare("https:", 6) == 0)
	{
		OpString default_urls;
		RETURN_IF_ERROR(default_urls.Set(g_pcui->GetDefaultStringPref(PrefsCollectionUI::AutoUpdateServer)));

		int length = current_url.Length();
		bool trailing_slash = current_url[length-1] == '/';
		if (trailing_slash)
		{
			current_url.Delete(length-1);
			--length;
		}

		int pos = default_urls.Find(current_url);
		while (pos != KNotFound)
		{
			// check that current_url matched whole address at position pos in the list
			if (pos == 0 || default_urls[pos-1] == ' ')
			{
				int end_pos = pos + length;
				if (default_urls[end_pos] == '\0' || default_urls[end_pos] == ' ' ||
					(default_urls[end_pos] == '/' && (default_urls[end_pos+1] == '\0' || default_urls[end_pos+1] == ' ')))
				{
					current_url.Delete(4, 1); // remove 's' from 'https'
					break;
				}
			}
			pos = default_urls.Find(current_url, pos+length);
		}

		if (trailing_slash)
		{
			RETURN_IF_ERROR(current_url.Append("/"));
		}
	}

	return address.TakeOver(current_url);
}

void CountryChecker::StatusXMLDownloaded(StatusXMLDownloader* downloader)
{
	OP_ASSERT(downloader);
	OP_ASSERT(downloader == m_status_xml_downloader);
	OP_ASSERT(m_status == CheckInProgress);

	m_timer.Stop();

	UpdatableResource* resource = downloader->GetFirstResource();
	while (resource)
	{
		if (resource->GetType() == UpdatableResource::RTSetting)
		{
			UpdatableSetting* s = static_cast<UpdatableSetting*>(resource);

			OpString section, key;
			if (OpStatus::IsError(s->GetAttrValue(URA_SECTION, section)) ||
				OpStatus::IsError(s->GetAttrValue(URA_KEY, key)))
			{
				break;
			}

			if (section == "Auto Update" && key == "Country Code")
			{
				OpString data;
				if (OpStatus::IsSuccess(s->GetAttrValue(URA_DATA, data)) && data.Length() == 2 &&
					data != "A6") // A6 means "Unknown Country" - see discussion in BINT-76
				{
					m_country_code.TakeOver(data);
				}
				break;
			}
		}
		resource = downloader->GetNextResource();
	}

	m_status = m_country_code.HasContent() ? CheckSucceded : CheckFailed;

	if (m_listener)
	{
		m_listener->CountryCheckFinished();
	}
}

void CountryChecker::StatusXMLDownloadFailed(StatusXMLDownloader* downloader, StatusXMLDownloader::DownloadStatus)
{
	OP_ASSERT(downloader);
	OP_ASSERT(downloader == m_status_xml_downloader);
	OP_ASSERT(m_status == CheckInProgress);

	m_timer.Stop();

	m_status = CheckFailed;

	if (m_listener)
	{
		m_listener->CountryCheckFinished();
	}
}

void CountryChecker::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(timer == &m_timer);
	OP_ASSERT(m_status == CheckInProgress);

	m_status = CheckTimedOut;

	if (m_status_xml_downloader)
	{
		OpStatus::Ignore(m_status_xml_downloader->StopRequest());
	}

	if (m_listener)
	{
		m_listener->CountryCheckFinished();
	}
}

#endif // AUTO_UPDATE_SUPPORT
