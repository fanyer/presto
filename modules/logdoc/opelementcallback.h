/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef LOGDOC_OPELEMENTCALLBACK_H
#define LOGDOC_OPELEMENTCALLBACK_H

#ifdef OPELEMENTCALLBACK_SUPPORT

class XMLTokenHandler;
class LogicalDocument;

/** Utility interface for parsing an XML document and creating
    HTML_Element objects but without putting them together in a tree.

    To use this functionality, one must create an object that
    implements this interface, and from it create an XMLTokenHandler
    object using the MakeTokenHandler function.  This token handler
    object is then used as the argument to XMLParser::Make to create
    an XMLParser object.  This XMLParser object is then used to parse
    the XML document (see the documentation for XMLParser for more
    details on how to do that.)  Any extra information about the XML
    document or parse errors encountered must be fetched directly from
    the XMLParser object.  If parsing fails, none of the functions in
    this interface will be called again.

    If the URL passed to XMLParser::Make contains a fragment
    identifier, only the element with a matching ID attribute and its
    descendants will be created and signalled by calls to AddElement()
    and EndElement().  If no such element is found, no calls at all
    are made.  If more than one such element is found in the document,
    all but the first are ignored. */
class OpElementCallback
{
public:
	static OP_STATUS MakeTokenHandler(XMLTokenHandler *&tokenhandler, LogicalDocument *logdoc, OpElementCallback *callback);
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
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual ~OpElementCallback();
	/**< Destructor. */

	virtual OP_STATUS AddElement(HTML_Element *element) = 0;
	/**< Called when an element starts.  The element is owned by
	     the OpTreeCallback implementation, and must be freed by
	     doing something like

	       if (element->Clean(frames_doc))
	           element->Free(frames_doc);

	     where 'frames_doc' is the FramesDocument returned by

	       logdoc->GetFramesDocument()

	     on the LogicalDocument object passed to MakeTokenHandler.

	     If the element is of one of the types HE_TEXT, HE_TEXTGROUP,
		 HE_COMMENT, HE_PROCINST or HE_DOCTYPE no corresponding
		 EndElement call will be made.  Otherwise, EndElement() will
		 be called at some point, including for elements created for
		 empty element tags.

	     @param element The element that started.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS EndElement() = 0;
	/**< Called when an element ends.  Only called for regular
	     elements.  That is, not called to end HE_TEXT, HE_TEXTGROUP,
	     HE_COMMENT, HE_PROCINST or HE_DOCTYPE elements; they are
	     ended implicitly since they cannot have children.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */
};

#endif // OPELEMENTCALLBACK_SUPPORT
#endif // LOGDOC_OPELEMENTCALLBACK_H
