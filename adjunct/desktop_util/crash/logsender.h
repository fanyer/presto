/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#ifndef LOG_SENDER_H
#define LOG_SENDER_H

class Upload_OpString8;

/** @brief Class for setting up and sending crash logs to Opera
 */
class LogSender : private MessageObject
{
public:
	class Listener
	{
	public:
		virtual ~Listener() {}

		/** Called when the log sender starts sending a log over HTTP */
		virtual void OnSendingStarted(LogSender* sender) = 0;

		/** Called when the log sender has succeeded sending a log over HTTP */
		virtual void OnSendSucceeded(LogSender* sender) = 0;

		/** Called when the log sender has failed sending a log over HTTP.
		 * @note the log can be resent by calling LogSender::Send again.
		 */
		virtual void OnSendFailed(LogSender* sender) = 0;
	};

	/** Constructor
	 * @param wants_crashpage Whether Opera should display a page with crash information
	 * after restarting if sending the log succeeds
	 */
	LogSender(bool wants_crashpage) : m_wants_crashpage(wants_crashpage), m_crash_time(0), m_listener(NULL) {}
	~LogSender();

	/** @param listener Listener that gets events from this log sender
	 * @ref LogSender::Listener
	 */
	void SetListener(Listener* listener) { m_listener = listener; }

	/** @param url URL the user was visiting when the crash occured
	 */
	OP_STATUS SetURL(OpStringC16 url) { return m_url.SetUTF8FromUTF16(url); }

	/** @param email Email address at which the user can be contacted for further questions
	 */
	OP_STATUS SetEmail(OpStringC16 email) { return m_email.SetUTF8FromUTF16(email); }

	/** @param comments Comments on this crash from the user
	 */
	OP_STATUS SetComments(OpStringC16 comments) { return m_comments.SetUTF8FromUTF16(comments); }

	/** @param path (absolute) path to the log file to send, or path to a whole folder to zip up and send.
	 */
	OP_STATUS SetLogFile(OpStringC16 path) { return m_dmp_path.Set(path); }

	/** @return the log file or folder used in this log sender
	 */
	OpStringC GetLogFile() const { return m_dmp_path; }

	/** @param crash_time Time at which the crash happened (UTC, unix time format)
	 */
	void SetCrashTime(time_t crash_time) { m_crash_time = crash_time; }

	/** Send the log to Opera
	 * @return see @ref OP_STATUS.
	 * @note On error, sending can be retried by calling this function again. It's not necessary to do
	 * setup again when this happens.
	 */
	OP_STATUS Send();

private:
	URL SetupUploadL();
	OP_STATUS ZipLogFile();
	void SetFormKeyValueL(Upload_OpString8* element, OpStringC8 key, OpStringC8 value);
	void Reset();
	void ResetUpload();

	// From MessageObject
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	bool m_wants_crashpage;
	OpString8 m_email;
	OpString8 m_url;
	OpString8 m_comments;
	OpString m_dmp_path;
	OpString m_zip_path;
	time_t m_crash_time;
	URL_InUse m_uploading_url;
	Listener* m_listener;
};

#endif // LOG_SENDER_H
