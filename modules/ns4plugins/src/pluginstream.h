/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _PLUGINSTREAM_H_INC_
#define _PLUGINSTREAM_H_INC_

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/src/plugincommon.h"

#include "modules/util/simset.h"
#include "modules/url/url2.h"
# include "modules/url/url_docload.h"

class URL_DataDescriptor;
class PluginStream;
class Plugin;

class PluginStream
  : public Link
  , public URL_DocumentHandler
{
private:

	Plugin*				m_plugin; ///< The plugin containing this stream
	URL_DataDescriptor*	url_data_desc;
	URL_InUse			m_url;
	int					id;
	NPStream*			stream;
	uint16				stype;
	BOOL				finished;
	BOOL				notify;
	BOOL				loadInTarget;

	PluginMsgType		m_lastposted;
	PluginMsgType		m_lastcalled;
	NPMIMEType			m_mimetype;
	int32				m_writeready;
	unsigned long		m_offset;
	unsigned long		m_buflength;
	int32				m_byteswritten;
	unsigned long		m_bytesleft;
	const char*			m_buf;
	BOOL				m_more;
	NPReason			m_reason;
	NPUTF8*				m_js_eval;
	unsigned			m_js_eval_length;
	const char*			m_notify_url_name;
	int					m_msg_flush_counter;

#ifdef NS4P_STREAM_FILE_WITH_EXTENSION_NAME
	OpString			m_url_cachefile_name;
#endif //NS4P_STREAM_FILE_WITH_EXTENSION_NAME


	PluginMsgType		GetLastPosted() const { return m_lastposted; }
	void				SetLastPosted( const PluginMsgType ty ) { m_lastposted = ty; }

	OP_STATUS			SetMimeType(const uni_char* mime_type);
	int32				GetWriteReady() const { return m_writeready; }
	void				SetOffset( unsigned long offs ) { m_offset = offs; }
	void				SetBufLength( unsigned long buflen ) { m_buflength = buflen; }
	void				SetBytesLeft( unsigned long bytes ) { m_bytesleft = bytes; }
	void				SetBuf( const char* b ) { m_buf = b; }
	void				SetMore( BOOL m ) { m_more = m; }
	void				SetURLDataDescr( URL_DataDescriptor* urldd ) { url_data_desc = urldd; }

	OP_STATUS           CreateNPStreamHeaders();

public:

						PluginStream(Plugin* plugin, int nid, URL& curl, BOOL notify, BOOL loadInTarget = FALSE);
						~PluginStream();
	OP_STATUS			CreateStream(void* notify_data);

	PluginStream*		Suc() const { return (PluginStream*)Link::Suc(); }
	PluginStream*		Pred() const { return (PluginStream*)Link::Pred(); }

	NPStream*			Stream() const { return stream; }
	int					Id() const { return id; }
	uint16				Type() const { return stype; }
	BOOL				IsFinished() { return finished; }
	int					UrlId(BOOL redirected = TRUE) { return m_url->Id(redirected); }
	BOOL				IsJSUrl() { return m_url.GetURL().Type() == URL_JAVASCRIPT; }
	URL					GetURL() { return m_url.GetURL(); }
	BOOL				IsLoadingInTarget() { return loadInTarget; }

	OP_STATUS			New(Plugin* plugin, const uni_char* mime_type, const NPUTF8* result, unsigned length);
	OP_STATUS			WriteReady(Plugin* plugin);
	OP_STATUS			Write(Plugin* plugin);
	OP_STATUS			StreamAsFile(Plugin* plugin);
	OP_STATUS			Destroy(Plugin* plugin);
	OP_STATUS			Notify(Plugin* plugin);
	OP_STATUS			Interrupt(Plugin* plugin, NPReason reason);

#ifdef NS4P_STREAM_FILE_WITH_EXTENSION_NAME
	/** GetUrlCacheFileName() fetches the name of the url cache file name.
	 *
	 * @return saved url cach file name
	 */
	const OpString&		GetUrlCacheFileName() const { return m_url_cachefile_name; }

	/** SaveUrlAsCacheFile() saves the stream's url content as a renamed cache file.
	 *
	 * @return status value
	 */
	OP_STATUS			SaveUrlAsCacheFile();
#endif //NS4P_STREAM_FILE_WITH_EXTENSION_NAME

	void				SetLastCalled( const PluginMsgType ty ) { m_lastcalled = ty; }
	PluginMsgType		GetLastCalled() const { return m_lastcalled; }

	NPMIMEType			GetMimeType() const { return m_mimetype; }
	void				SetType( const uint16 st ) { stype = st; }
	void				SetWriteReady( const int32 wr ) { m_writeready = wr; }
	void				SetBytesWritten( const int32 bw ) { m_byteswritten = bw; }
	int32				GetBytesWritten() const { return m_byteswritten; }
	unsigned long		GetOffset() const { return m_offset; }
	unsigned long		GetBytesLeft() const { return m_bytesleft; }
	unsigned long		GetBufLength() const { return m_buflength; }
	const char*			GetBuf() const { return m_buf; }
	void				SetReason( const NPReason reas ) { m_reason = reas; }
	NPReason			GetReason() const { return m_reason; }
	URL_DataDescriptor*	GetURLDataDescr() const { return url_data_desc; }
	BOOL				GetNotify() const { return notify; }
	BOOL				GetMore() const { return m_more; }
	void				SetFinished() { finished = TRUE; }
	const char*			GetStreamUrl() { return stream->url; }
	void*				GetStreamNotifyData() { return stream->notifyData; }

	const NPUTF8*		GetJSEval() { return m_js_eval; }
	unsigned			GetJSEvalLength() { return m_js_eval_length; }

	/** Mechanism for halting the streaming url, reposition and restart streaming.
	 *  Every time the plugin is calling NPN_RequestRead() with an offset value, 
	 *  the m_msg_flush_counter is incremented and 
	 *  the ongoing streaming with WRITEREADY and WRITE messages is ignored (flushed).
	 *  The streaming is restarted as soon as the new WRITEREADY and WRITE messages
	 *  with the correct m_msg_flush_counter value arrives in PluginHandler::HandleMessage().
	 *
	 * @return value m_msg_flush_counter is incremented every time the stream is repositioned.
	 */
	int					GetMsgFlushCounter() { return m_msg_flush_counter; }

	/** Updates the stream content length in PluginStream::NPStream->end if needed (stream->end < offset + m_bytesleft)
	 *  NPStream* struct is submitted to the plugin in NPP_NewStream(), NPP_WriteReady(), NPP_Write() and NPP_DestroyStream().
	 *  m_offset counts the number of bytes streamed to the plugin so far
	 *  m_bytesleft counts the number of bytes ready to be streamed to the plugin
	 *
	 */
	void				UpdateStreamLength() { if (stream->end < (m_offset + m_bytesleft)) stream->end = m_offset + m_bytesleft; }

	/** NPP_ plugin function directly called
	 *
	 * @param msgty denotes the plugin function
	 * @param plugin denotes the plugin instance
	 * @return value from plugin
	 */
	NPError			CallProc(PluginMsgType msgty, Plugin* plugin);

	/** Interrupts streaming by calling plugin functions directly
	 *
	 * @param plugin denotes the plugin instance
	 */
	void			NonPostingInterrupt(Plugin* plugin);

	/** Update the status bar with streaming progress information.
	 *
	 * @param plugin denotes the plugin instance
	 * @param terminate decides whether to display progress information (FALSE) or emtpy the progress information (TRUE)
	 * @return status value
	 */
	OP_STATUS		UpdateStatusRequest(Plugin* plugin, BOOL terminate = FALSE);

	/** Implements a simplified version of NPN_RequestRead.
	 *
	 * @param rangeList denotes the wanted offset of the streaming data
	 * @return value: NPERR_INVALID_INSTANCE_ERROR (stream is destroyed)
	 *				  NPERR_INVALID_PARAM (rangeList contains more sophisticated NPByteRange.length or NPByteRange.next)
	 *				  NPERR_GENERIC_ERROR (the url's data descriptor could not be positioned to the wanted offset or
	 *									   the url module does not support positioning of the url data descriptor)
	 *				  NPERR_NO_ERROR (success)
	 */
	NPError			RequestRead(NPByteRange* rangeList);

	// Overrides from the URL_DocumentHandler class, see the documentation of URL_DocumentHandler
	virtual BOOL OnURLHeaderLoaded(URL &url);
	virtual BOOL OnURLDataLoaded(URL &url, BOOL finished, OpAutoPtr<URL_DataDescriptor> &stored_desc);
	virtual void OnURLLoadingFailed(URL &url, Str::LocaleString error, OpAutoPtr<URL_DataDescriptor> &stored_desc);
	virtual void OnURLLoadingStopped(URL &url, OpAutoPtr<URL_DataDescriptor> &stored_desc);
};

#endif // _PLUGIN_SUPPORT_
#endif // !_PLUGINSTREAM_H_INC_
