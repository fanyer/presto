/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef XMLTOKENHANDLER_H
#define XMLTOKENHANDLER_H

class XMLToken;
class XMLParser;

/**

  Interface to be implemented by users of an XMLParser.  For simple
  applications, it is very simple.  Implement HandleToken, handle the
  relevant tokens and ignore the rest.  As long as everything is
  peachy, RESULT_OK should be returned.  If a "parse error" occurs,
  RESULT_ERROR should be returned, and if an out of memory condition
  occurs, RESULT_OOM should be returned.

  If you know what you are doing, you can return RESULT_BLOCK.  This
  means "stop calling me with tokens until I tell you to continue."
  After you return RESULT_BLOCK, someone will call SetSourceCallback
  with a SourceCallback that should be used to unblock the parser.
  Note that doing this makes the parser asynchronous from the client
  code's perspective.  It might not be designed to handle that, so be
  sure to know that all code that might be using this token handler
  supports asynchronous parsing before you return RESULT_BLOCK.

  Note that you cannot store references to or copies of the tokens
  passed to HandleToken; the token may become invalid right after the
  call.

*/
class XMLTokenHandler
{
public:
	/** Return codes for the HandleToken function. */
	enum Result
	{
		RESULT_OK,
		/**< No error, continue parsing. */

		RESULT_BLOCK,
		/**< No error, but stop parsing temporarily. */

		RESULT_ERROR,
		/**< Generic error, stop parsing. */

		RESULT_OOM
		/**< Out of memory error.  Parsing is stopped but can be
		     continued later.  If it is, the next token produced by
		     the parser will be identical to the one that produced
		     this error. */
	};

	virtual ~XMLTokenHandler();
	/**< Destructor. */

	virtual Result HandleToken(XMLToken &token) = 0;
	/**< Called to handle token.  The token is only valid during the
	     call.  The same token object is typically reused for all
	     calls, and between calls the data referenced by the token may
	     be invalid.

	     @return Token handler result.  RESULT_BLOCK is special, see
	             the class description for details. */

	virtual void ParsingStopped(XMLParser *parser);
	/**< Called when the parsing has stopped.  The parser object can
	     be used to determine whether the parsing stopped successfully
	     or not.  When the parsing finishes normally, this function is
	     always called after the XMLToken::TYPE_Finished token has
	     been processed.  This function might be called while the
	     parser is being destroyed (that is, from its destructor.) */

	/** Callback for a blocking XMLTokenHandler.  See the description
	    of the XMLTokenHandler class for details. */
	class SourceCallback
	{
	public:
		enum Status
		{
			STATUS_CONTINUE,
			/**< The source should continue. */

			STATUS_ABORT_FAILED,
			/**< The source should abort due to a failure. */

			STATUS_ABORT_OOM
			/**< The source should abort due to an OOM condition. */
		};

		virtual void Continue(Status status) = 0;
		/**< Signal the source to continue or abort. */

	protected:
		virtual ~SourceCallback() {}
		/**< XMLTokenHandler implementations shouldn't delete the
		     source callback. */
	};

	virtual void SetSourceCallback(SourceCallback *callback);
	/**< When HandleToken() return RESULT_BLOCK, the calling source
	     sets a callback using this function that should be called
	     when the parsing needs to continue (or be aborted.)  The
	     default implementation does nothing and is only appropriate
	     for token handlers that never return RESULT_BLOCK. */
};

#endif // XMLTOKENHANDLER_H
