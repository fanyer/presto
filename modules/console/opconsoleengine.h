/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if !defined OPCONSOLEENGINE_H && defined OPERA_CONSOLE
#define OPCONSOLEENGINE_H

#include "modules/hardcore/mh/messobj.h"
#include "modules/util/simset.h"

class OpConsoleListener;
class OpConsoleFilter;
class Window;
class OpWindowCommander;

/** Global console engine object. */
#define g_console (g_opera->console_module.OpConsoleEngine())

/**
 * The Opera Console. This class is the engine of the Opera message console.
 * It will store a list of messages up to a configured maximum size. When
 * that number is exceeded, the oldest message will be deleted.
 */
class OpConsoleEngine : private MessageObject
{
public:
	/**
	 * First-phase constructor.
	 */
	OpConsoleEngine();

	/**
	 * Second-phase constructor.
	 *
	 * @param size Number of messages to keep in the console.
	 */
	void ConstructL(size_t size);

	/**
	 * Destructor.
	 */
	virtual ~OpConsoleEngine();

	/**
	 * Source description for messages to the console. This list should
	 * be kept in synch with m_keywords in opconsoleengine.cpp to
	 * ensure that serialization from/to preferences works properly.
	 */
	enum Source
	{
		EcmaScript,		///< EcmaScript message
#ifdef _APPLET_2_EMBED_
		Java,			///< Java message
#endif
#ifdef M2_SUPPORT
		M2,				///< M2 message
#endif
		Network,		///< Networking code
		XML,			///< XML parser and related code
		HTML,			///< HTML parser and related code
		CSS,			///< CSS parser and related code
#ifdef XSLT_SUPPORT
		XSLT,			///< XSLT parser and related code
#endif
#ifdef SVG_SUPPORT
		SVG,			///< SVG parser and related code
#endif
#ifdef _BITTORRENT_SUPPORT_
		BitTorrent,		///< BitTorrent message
#endif
#ifdef GADGET_SUPPORT
		Gadget,			///< Gadget message
#endif
#if defined(SUPPORT_DATA_SYNC) || defined(WEBSERVER_SUPPORT)
		OperaAccount,	///< Opera Account message for Opera Link and Opera Unite
#endif
#ifdef SUPPORT_DATA_SYNC
		OperaLink,		///< Opera Link messages
#endif // SUPPORT_DATA_SYNC
#ifdef WEBSERVER_SUPPORT
		OperaUnite,		///< Opera Unite messages
#endif // WEBSERVER_SUPPORT
#ifdef AUTO_UPDATE_SUPPORT
		AutoUpdate,		///< Autoupdate messages
#endif
		Internal,		///< Internal debugging messages
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
		PersistentStorage,		///< HTML5 Persistant Storage / Web Storage
#endif //DATABASE_MODULE_MANAGER_SUPPORT
#ifdef WEBSOCKETS_SUPPORT
		WebSockets,		///< WebSocket message
#endif //WEBSOCKETS_SUPPORT
#ifdef GEOLOCATION_SUPPORT
		Geolocation,		///< Geolocation message
#endif // GEOLOCATION_SUPPORT
#ifdef MEDIA_SUPPORT
		Media,			///< Media (<video>, <track> et al.)
#endif // MEDIA_SUPPORT
#ifdef _PLUGIN_SUPPORT_
		Plugins,        ///< Plug-in messages
#endif // _PLUGIN_SUPPORT_

		// Insert new components above

#ifdef SELFTEST
		SelfTest,		///< Used by internal selftest code
#endif
		SourceTypeLast	///< Sentinel value, not allowed in messages
	};

	/**
	 * Severity indication for messages to the console.
	 */
	enum Severity
	{
#ifdef _DEBUG
		Debugging = 0,	///< Internal debugging information (debug builds only)
#endif
		Verbose = 1,	///< Verbose log information, not normally shown in the UI console
		Information,	///< Standard log information, normally shown in the UI console
		Error,			///< Error information, which would pop up the UI console if enabled
		Critical,		///< Critical error information, which would always pop up the UI console

		DoNotShow		///< Special severity used for filters to not show messages.
	};

	/**
	 * Structure used for posting messages to the console.
	 */
	struct Message
	{
		unsigned int	id;					///< ID of the message (Can be used in GetMessage call)
		enum Source		source;				///< Sender of message
		enum Severity	severity;			///< Severity of message
		unsigned long	window;				///< Window associated with message (0 if none)
		time_t			time;				///< Time (set automatically if 0)
		OpString		url;				///< URL associated with message (empty if none)
		OpString		context;			///< Context associated with message
		OpString		message;			///< Message text
		INT32			error_code;			///< Error code associated with the message

		Message(Source src, Severity sev, unsigned long window_id = 0, time_t posted_time = 0)
			: source(src), severity(sev), window(window_id), time(posted_time) {};
		Message(Source src, Severity sev, OpWindowCommander *wic, time_t posted_time = 0);
		Message(Source src, Severity sev, Window *win, time_t posted_time = 0);
		Message()
			: window(0), time(0) {};

		/** Deep copy data from another OpConsoleEngine::Message struct. */
		void CopyL(const Message *);
		/** Deallocate all data used by this message. */
		void Clear();

	private:
		Message operator = (const Message &);	///< Prohibit use of unsafe copying.
		Message(const Message &);				///< Prohibit use of unsafe copying.
	};

	/**
	 * Post a message to the Opera console. The data in the posted object
	 * is copied and must be deleted by the caller. If the console has been
	 * configured to display whenever an error message appears, calling
	 * this with a severity of Error or higher will cause it to be displayed.
	 *
	 * Messages posted for a window running in privacy mode will be dropped.
	 * If the console stores no backlog (CON_NO_BACKLOG), and there are no
	 * listeners, the message will also be blocked. In both these cases,
	 * the special value MESSAGE_BLOCKED will be returned.
	 *
	 * @param msg Message to post.
	 * @return Number assigned to the posted message, or MESSAGE_BLOCKED.
	 */
	unsigned int PostMessageL(const OpConsoleEngine::Message *msg);

	/** Non-leaving version of PostMessageL. */
	OP_STATUS PostMessage(const OpConsoleEngine::Message *msg, unsigned int& message_number)
	{
		TRAPD(rc, message_number = PostMessageL(msg));
		return rc;
	}

	/** Non-leaving version of PostMessageL, which ignores message_number.*/
	inline OP_STATUS PostMessage(const OpConsoleEngine::Message *msg)
	{
		unsigned int unused;
		return PostMessage(msg, unused);
	}

	enum { MESSAGE_BLOCKED = static_cast<unsigned int>(-1) };

	/**
	 * Retrieve a message from the Opera console. The returned pointer is
	 * owned by the console and is not guaranteed to live beyond the next
	 * call to GetMessage() or PostMessageL().
	 *
	 * @param id Number of the message to retrieve.
	 * @return Pointer to the message structure. NULL if message does not
	 *         exist.
	 */
	const OpConsoleEngine::Message *GetMessage(unsigned int id);

	/**
	 * Retrieve the number of the oldest stored message. If this number
	 * is higher than the return value from GetHighestId(), there are
	 * no messages in the console.
	 *
	 * @return id Number of the oldest meessage.
	 */
	inline unsigned int GetLowestId()
	{ return m_lowest_id; }

	/**
	 * Retrieve the number of the most recently stored message. If this
	 * number if lower than the return value from GetHighestId(), there
	 * are no messages in the console.
	 *
	 * @return id Number of the most recently meessage.
	 */
	inline unsigned int GetHighestId()
	{ return m_highest_id; }

	/**
	 * Remove all messages stored in the console.
	 */
	void Clear();

	/**
	 * Register a console listener. The listener will be notified each time
	 * a new message is posted to the console.
	 */
	inline void RegisterConsoleListener(OpConsoleListener *listener);

	/**
	 * Unregister a console listener. The listener will no longer be notified
	 * of changes.
	 */
	inline void UnregisterConsoleListener(OpConsoleListener *listener);

	/**
	 * Is the console engine actually logging messages? This can be called to
	 * see if there is any point in generating the message at all.
	 */
	inline BOOL IsLogging();

#ifdef CON_GET_SOURCE_KEYWORD
	/**
	 * Retrieve the internal string representation of a console source.
	 */
	inline const char *GetSourceKeyword(Source source);
#endif

private:
	Head m_listeners;			///< List of console listeners.
	Message *m_messages;		///< Circular buffer of messages in the console.
	unsigned int m_lowest_id;	///< Lowest message id.
	unsigned int m_highest_id;	///< Highest message id.
	size_t m_size;				///< Size of message buffer.
	BOOL m_pending_broadcast;	///< Broadcast of messages is pending.

#ifdef CON_GET_SOURCE_KEYWORD
	/** String representations of message sources */
# ifdef HAS_COMPLEX_GLOBALS
	static const char * const m_keywords[static_cast<int>(OpConsoleEngine::SourceTypeLast)];
# else
	const char *m_keywords[static_cast<int>(OpConsoleEngine::SourceTypeLast)];
	void InitSourceKeywordsL();
# endif
#endif

	// Inherited interfaces from MessageObject:
	virtual void HandleCallback(OpMessage, MH_PARAM_1, MH_PARAM_2);
};

/**
 * Interface for listening to messages from the OpConsoleEngine.
 * Implementations of this class that are registered with the g_engine object
 * will receive a signal for each new message that is posted to the Opera
 * console. It can then chose to either process that message or ignore it as
 * it sees fit.
 */
class OpConsoleListener : private Link
{
public:
	/**
	 * A new message has been posted to the console.
	 *
	 * @param id Number of the posted message.
	 * @param message The posted message.
	 */
	virtual OP_STATUS NewConsoleMessage(unsigned int id, const OpConsoleEngine::Message *message) = 0;

private:
	friend class OpConsoleEngine;
};

inline void OpConsoleEngine::RegisterConsoleListener(OpConsoleListener *listener)
{ listener->Into(&m_listeners); }

inline void OpConsoleEngine::UnregisterConsoleListener(OpConsoleListener *listener)
{ listener->Out(); }

inline BOOL OpConsoleEngine::IsLogging()
{
#ifdef CON_NO_BACKLOG
	return m_listeners.Cardinal() > 0;
#else
	return TRUE;
#endif
}

#ifdef CON_GET_SOURCE_KEYWORD
inline const char *OpConsoleEngine::GetSourceKeyword(Source source)
{
# ifdef HAS_COMPLEX_GLOBALS
	return OpConsoleEngine::m_keywords[source];
# else
	return m_keywords[source];
# endif
}
#endif

#endif // OPCONSOLEENGINE_H
