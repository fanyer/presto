/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */


#ifndef FAKE_SERVICE_DOWNLOAD_LISTENER
#define FAKE_SERVICE_DOWNLOAD_LISTENER

#ifdef SELFTEST

/*class FakeServiceDownloadListener : public ServiceDownloadListener
{
public:
	FakeServiceDownloadListener(BOOL async_test) :
		m_async_test(async_test),
		m_download_has_started(FALSE),
		m_downloading(FALSE),
		m_failed(FALSE),
		m_completed(FALSE)
	{}

	BOOL IsDownloading() { return m_downloading; }
	BOOL HasDownloadEverStarted() { return m_download_has_started; }
	BOOL HasDownloadFailed() { return m_failed; }
	BOOL HasDownloadCompleted() { return m_completed; }

	virtual void OnDownloading(UINT current_step, UINT total_steps)
	{
		m_download_has_started = TRUE;
		m_downloading = TRUE;

		if (m_async_test)
			ST_passed();
	}

	virtual void OnDownloadCompleted()
	{
		m_downloading = FALSE;
		m_completed = TRUE;

		if (m_async_test)
			ST_passed();
	}

	virtual void OnDownloadFailed()
	{
		m_downloading = FALSE;
		m_failed = TRUE;

		if (m_async_test)
			ST_passed();
	}

private:
	BOOL	m_async_test;
	BOOL	m_download_has_started;
	BOOL	m_downloading;
	BOOL	m_failed;
	BOOL	m_completed;
};
*/
#endif // SELFTEST

#endif // FAKE_SERVICE_DOWNLOAD_LISTENER
