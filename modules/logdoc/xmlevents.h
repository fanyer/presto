/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef XMLEVENTS_H
#define XMLEVENTS_H
#ifdef XML_EVENTS_SUPPORT

#include "modules/util/simset.h"

class HTML_Element;
class FramesDocument;

class XML_Events_Activator;
class XML_Events_EventHandler;
class XML_Events_ExternalElementCallback;

class XML_Events_Registration
	: public Link
{
private:
	HTML_Element *owner;
	HTML_Element *observer;
	uni_char *event_type;
	uni_char *observer_id;
	uni_char *target_id;
	uni_char *handler_uri;
	BOOL prevent_default;
	BOOL stop_propagation;
	BOOL capture;
	BOOL handler_prepared;
	BOOL disabled;
	XML_Events_EventHandler *event_handler;
	XML_Events_Activator *activator;

	XML_Events_ExternalElementCallback *external_element_callback;

public:
	XML_Events_Registration(HTML_Element *owner);
	~XML_Events_Registration();

	OP_STATUS SetEventType(FramesDocument *frames_doc, const uni_char *type, int length);
	OP_STATUS SetObserverId(FramesDocument *frames_doc, const uni_char *id, int length);
	OP_STATUS SetTargetId(FramesDocument *frames_doc, const uni_char *id, int length);
	OP_STATUS SetHandlerURI(FramesDocument *frames_doc, const uni_char *uri, int length);
	void SetPreventDefault(BOOL value) { prevent_default = value; }
	void SetStopPropagation(BOOL value) { stop_propagation = value; }
	void SetCapture(BOOL value) { capture = value; }
	void SetActivator(XML_Events_Activator *activator);

	HTML_Element *GetOwner() { return owner; }
	HTML_Element *GetObserver() { return observer; }
	const uni_char *GetEventType() { return event_type; }
	const uni_char *GetObserverId() { return observer_id; }
	const uni_char *GetTargetId() { return target_id; }
	const uni_char *GetHandlerURI() { return handler_uri; }
	BOOL GetPreventDefault() { return prevent_default; }
	BOOL GetStopPropagation() { return stop_propagation; }
	BOOL GetCapture() { return capture; }
	XML_Events_Activator *GetActivator() { return activator; }

	OP_STATUS HandleElementInsertedIntoDocument(FramesDocument *frames_doc, HTML_Element *inserted);
	OP_STATUS HandleElementRemovedFromDocument(FramesDocument *frames_doc, HTML_Element *removed);
	OP_STATUS HandleIdChanged(FramesDocument *frames_doc, HTML_Element *changed);

	OP_STATUS Update(FramesDocument *frames_doc);
	void ResetEventHandler();

	BOOL GetHandlerIsExternal(URL &document_url);
	XML_Events_ExternalElementCallback *GetExternalElementCallback() { return external_element_callback; }
	OP_STATUS PrepareHandler(FramesDocument *frm_doc);

	/**
	 * Use this when the event is moved to another document or when the document
	 * is getting removed. This is needed since the xml event objects have a
	 * cached frames_doc pointer and is included in document lists.
	 *
	 * @param new_document The new doucment or NULL if the document is being deleted.
	 */
	void MoveToOtherDocument(FramesDocument *old_document, FramesDocument *new_document);
};

#endif ///* XML_EVENTS_SUPPORT */
#endif ///* XMLEVENTS_H */
