/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef LOGDOC_OPTREECALLBACK_H
#define LOGDOC_OPTREECALLBACK_H

#ifdef OPTREECALLBACK_SUPPORT

class XMLTokenHandler;
class LogicalDocument;

/** Utility interface for parsing an XML document and building a tree
    of HTML_Element objects from it or from a part of it.

    To use this functionality, one must create an object that
    implements this interface, and from it create an XMLTokenHandler
    object using the MakeTokenHandler function.  This token handler
    object is then used as the argument to XMLParser::Make to create
    an XMLParser object.  This XMLParser object is then used to parse
    the XML document (see the documentation for XMLParser for more
    details on how to do that.)  Any extra information about the XML
    document or parse errors encountered must be fetched directly from
    the XMLParser object.  If parsing fails, none of the functions in
    this interface will be called.

    The ElementNotFound function in this interface will only be called
    if the URL passed to XMLParser::Make contains a fragment
    identifier (and no element with a matching id is found.)  If the
    URL does not contain a fragment identifier, a HE_DOC_ROOT element
    containing the root element and any top-level comments,
    white-space text nodes and processing-instructions will be
    returned via the ElementFound function. */
class OpTreeCallback
{
public:
	static OP_STATUS MakeTokenHandler(XMLTokenHandler *&tokenhandler, LogicalDocument *logdoc, OpTreeCallback *callback, const uni_char *fragment = NULL);
	/**< Create an XMLTokenHandler that will create a tree of HTML
	     elements and notify the supplied OpTreeCallback when
	     finished.  If the URL parsed by the XMLParser that the token
	     handler is used with contains a fragment identifier, only the
	     sub-tree rooted by an element with an id that matches the
	     fragment identifier is created.

	     @param tokenhandler Set to the created token handler on
	                         success.  The token handler must be freed
	                         by the caller.
	     @param logdoc The logical document to which the created
	                   elements will belong.  Can not be NULL.
	     @param callback OpTreeCallback implementation.  Can not be
	                     NULL.
	     @param fragment If non-NULL, use this fragment identifier
	                     instead of the one from the parsed URL.  If
	                     an empty string is specified, no fragment
	                     identifier will be used, even if the parsed
	                     URL contains one.  The string is copied.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual ~OpTreeCallback();
	/**< Destructor. */

	virtual OP_STATUS ElementFound(HTML_Element *element) = 0;
	/**< Called when parsing has finished successfully.  The element
	     is owned by the OpTreeCallback implementation, and must be
	     freed by doing something like

	       if (element->Clean(frames_doc))
	           element->Free(frames_doc);

	     where 'frames_doc' is the FramesDocument returned by

	       logdoc->GetFramesDocument()

	     on the LogicalDocument object passed to MakeTokenHandler.

	     @param element Root element of the parsed element tree.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS ElementNotFound() = 0;
	/**< Called if the parsed URL contained a fragment identifier and
	     no element with a matching id was found in the document.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */
};

#endif // OPTREECALLBACK_SUPPORT
#endif // LOGDOC_OPTREECALLBACK_H
