/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2003-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_ENVIRONMENT_H
#define DOM_ENVIRONMENT_H

/** @file domenvironment.h
  *
  * Main DOM external API.
  *
  * @author Jens Lindstrom
  */


#include "modules/dom/domeventtypes.h"
#include "modules/url/url_enum.h"
#include "modules/dochand/documentreferrer.h"
#include "modules/pi/OpKeys.h"
#include "modules/hardcore/keys/opkeys.h"


/* Some classes defined in other modules that we need declared here. */
class FramesDocument;
class ES_Runtime;
class ES_Object;
class ES_Thread;
class ES_JavascriptURLThread;
class ES_ThreadScheduler;
class ES_AsyncInterface;
class ES_Value;
class HTML_Element;
class URL;
class JS_Plugin_Context;
class TempBuffer;
class OpFile;
class CSS_DOMRule;
class DocumentReferrer;
class MediaTrack;
class OpWindowCommander;

#ifdef WEBSERVER_SUPPORT
class WebResource_Custom;
#endif // WEBSERVER_SUPPORT

#ifdef _PLUGIN_SUPPORT_
class OpPlatformKeyEventData;
#endif // _PLUGIN_SUPPORT_

/* The only DOM class that modules using the API in this file needs
   to know anything about (and all they need to know about it). */
class DOM_Object;


/** Representation of the DOM/JavaScript environment in a document.
  *
  * Owns all related objects (ES_Runtime, ES_ThreadScheduler,
  * ES_AsyncInterface, JS_Window, DOM_Document), handles creation
  * of objects (Nodes and LiveConnect objects) and firing of events.
  */
class DOM_Environment
{
public:
	/*=== Creation and destruction ===*/

	static OP_STATUS Create(DOM_Environment *&env, FramesDocument *doc);
	/**< Create a DOM environment.

	     @param env Set to the created environment on success.
	     @param doc Document the environment is for.

	     @return OpStatus::OK on success, OpStatus::ERR if ECMAScript is
	             disabled or OpStatus::ERR_NO_MEMORY on OOM. */

	static void Destroy(DOM_Environment *env);
	/**< Destroy a DOM environment.

	     @param env Environment to destroy.  Can be NULL. */

	static OP_STATUS CreateStatic();
	/**< Allocate all global data used by the DOM module.  Call when Opera
	     starts before the ECMAScript engine is used.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static void DestroyStatic();
	/**< Deallocate all global data used by the DOM module.  Call when Opera
	     exits before the ECMAScript engine is shutdown. */

	virtual void BeforeUnload() = 0;
	/**< Should be called when the FramesDocument stops being the current
	     document. should not longer be the current document allowing the
	     environment to stop doing things that shouldn't be done in disabled
	     documents. */

	virtual void BeforeDestroy() = 0;
	/**< Should be called before the FramesDocument and LogicalDocument
	     objects are destroyed (typically just before Destroy() is called)
	     allowing the environment to do some pre-destruction cleanup that
	     depends on the FramesDocument and LogicalDocument objects being
	     valid. */


	/*=== Simple getters ===*/

	static BOOL IsEnabled(FramesDocument *doc);
	static BOOL IsEnabled(FramesDocument *doc, BOOL is_default_enabled);
	/**< Check if an environment for 'doc' would be enabled. If not explicitly disabled, return the default value.

	     @return TRUE or FALSE. */

	virtual BOOL IsEnabled() = 0;
	/**< Check if the environment is enabled.

	     @return TRUE or FALSE. */

	virtual ES_Runtime *GetRuntime() = 0;
	/**< Get the environment's runtime.

	     @return An ES_Runtime object (never NULL). */

	virtual ES_ThreadScheduler *GetScheduler() = 0;
	/**< Get the environment's thread scheduler.

	     @return An ES_ThreadScheduler object (never NULL). */

	virtual ES_AsyncInterface *GetAsyncInterface() = 0;
	/**< Get the environment's asynchronous interface.

	     @return An ES_AsyncInterface object (never NULL). */

	virtual DOM_Object *GetWindow() = 0;
	/**< Get the environment's window object (also the global object).

	     @return A DOM object (never NULL). */

	virtual DOM_Object *GetDocument() = 0;
	/**< Get the environment's document object.  May return NULL.

	     @return A DOM node object. */

	virtual FramesDocument *GetFramesDocument() = 0;
	/**< Get the document that owns this environment.

		 @return A FramesDocument object (can be NULL). */


	/*=== Security and current executing script ===*/

	virtual BOOL AllowAccessFrom(const URL &url) = 0;
	/**< Return TRUE if a script from the host designated by 'url' would
	     be allowed to access DOM objects from this environment.
	     Use the other AllowAccessFrom if possible.

	     @return TRUE or FALSE. */

	virtual BOOL AllowAccessFrom(ES_Thread* thread) = 0;
	/**< Return TRUE if a script in thread would
	     be allowed to access DOM objects from this environment.

	     @return TRUE or FALSE. */

	virtual ES_Thread *GetCurrentScriptThread() = 0;
	/**< Returns the currently executing script thread during calls out
	     of the DOM module to other modules or NULL if no such script
	     exists.

	     @return A thread or NULL. */

	virtual DocumentReferrer GetCurrentScriptURL() = 0;
	/**< Returns the currently executing script's URL during calls out
	     of the DOM module to other modules or the empty url if there
	     was no currently executing script.

	     @return The script URL or an empty URL if there was no current script. */

	virtual BOOL SkipScriptElements() = 0;
	/**< Returns TRUE if script elements inserted into the document should
	     be skipped (that is, not executed).

	     @return TRUE or FALSE. */

	virtual TempBuffer *GetTempBuffer() = 0;
	/**< Returns a pointer to a TempBuffer into which the result of an
	     operation can be written or NULL if no such result is expected.
	     Typically returns non-NULL when calling functions for retrieving
	     attribute values from HTML_Element.

	     @return A pointer to an empty TempBuffer or NULL. */


	/*=== Creating nodes ===*/

	virtual OP_STATUS ConstructNode(DOM_Object *&node, HTML_Element *element) = 0;
	/**< Create (if necessary) a Node object for 'element'.

	     @param node Set to the element's node object on success.
	     @param element The element to create a node for.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	/*=== Creating DOM_CSSRules ===*/

	virtual OP_STATUS ConstructCSSRule(DOM_Object *&rule, CSS_DOMRule *css_rule) = 0;
	/**< Create (if necessary) a CSSRule object for 'css_rule'.
		 Also creates the necessary structure of objects for CSSRuleList, CSSStyleSheet, etc.
		 which are necessary to hold the CSSRule.

		 @param rule Set to the css_rule's CSSRule object on success.
		 @param css_rule The CSS_DOMRule to create the DOM_CSSRule for.

		 @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */


#ifdef APPLICATION_CACHE_SUPPORT

	/*=== Send events to the ApplicationCache singleton ===*/

	virtual OP_STATUS SendApplicationCacheEvent(DOM_EventType event, BOOL lengthComputable = FALSE, OpFileLength loaded = 0, OpFileLength total = 0) = 0;
	/**< Sends a cache event to the ApplicationCache singleton.

		 Events should only be sent by ApplicationCacheGroup objects.

		 The progress events attributes are only used by ONPROGRESS event


	  	 @param  event The event.
	  	 @param  lengthComputable progress event attribute
	  	 @param  loaded  progress event attribute
	  	 @param  total progress event attribute
	  	 @return OpStatus::ERR if the ApplicactionCache object does not exist,
				 OpStatus::MEMORY ERROR on OOM. */

#endif // APPLICATION_CACHE_SUPPORT

#ifdef WEBSERVER_SUPPORT

	/*=== Creating and sending event to the server side ecmascript ===*/

	virtual BOOL WebserverEventHasListeners(const uni_char *type) = 0;
	/**< Checks if any listeneres for the event type has been added to
	 * 	 the webserver object.

	     @param type The type of event you want to check.

	     @return TRUE if any resource is listing to the event type,
	     		 FALSE if not.

	 */

	virtual OP_STATUS SendWebserverEvent(const uni_char *type, WebResource_Custom *web_resource_script) = 0;
	/**< Fires a DOM event with the webserver object in this
	     environment as its target.

	     The webserver object must have been installed in the
	     environment before this call can be done.  This is done
	     automatically for all environments in window's that have
	     webserver associations.

	     @param type The type of event to fire.
	     @param web_resource_script The script resouce that serves
	                                the content generated by the
	                                server side ecmascript.

	     @return OpStatus::OK, OpStatus::ERR if no webserver object
	             has been installed in this environment or
	             OpStatus::ERR_NO_MEMORY on OOM. */

#endif //WEBSERVER_SUPPORT

	/*=== Handling events ===*/

	static DOM_EventType GetEventType(const uni_char *type);
	/**< Return the DOM_EventType enumerator for the event type 'type'.  If no
	     appropiate enumerator is found, DOM_EVENT_NONE is returned.

		 @param type An event type.  Should not include an "on" prefix.

		 @return A DOM_EventType code. */

	static const char *GetEventTypeString(DOM_EventType type);
	/**< Return the string event type that corresponds to a DOM_EventType
	     enumerator.

	     @param type A DOM_EventType enumerator.

	     @return A string or NULL if known_type is invalid (DOM_EVENT_NONE
	             or DOM_EVENT_CUSTOM). */

	virtual BOOL HasEventHandlers(DOM_EventType type) = 0;
	/**< Returns whether any Node belonging in this environment might
	     have an event handler for the given event type.

	     @param type Event type.

	     @return TRUE or FALSE. */

	virtual BOOL HasEventHandler(HTML_Element *target, DOM_EventType event, HTML_Element **nearest_handler = NULL) = 0;
	/**< Check whether an event with the target 'target' would be
	     handled.

	     Note: if only the document node is found to handle the event,
	     'nearest_handler' will be set to NULL even though the function
	     returns TRUE.

	     @param target Element to check.
	     @param event Event type.
	     @param nearest_handler If not NULL, set to the nearest ancestor
	                            of 'target' that handles the event or to
	                            'target' if 'target' handles the event.
	     @param[in] real_target_is_window If the event (for instance ONLOAD)
	                                      is for window rather then target.
	                                      MAYBE means that it should be
	                                      guessed depending on the event
	                                      and target. For instance ONLOAD
	                                      sent to a HE_BODY is assumed to
	                                      be a window event in that case.

	     @return TRUE or FALSE. */

	virtual BOOL HasWindowEventHandler(DOM_EventType window_event) = 0;
	/**< Check whether an event aimed at the window object would be
	     handled.

	     @param window_event Event type.

	     @return TRUE or FALSE. */

	virtual BOOL HasEventHandler(DOM_Object *target, DOM_EventType event, DOM_Object **nearest_handler = NULL) = 0;
	/**< Check whether an event with the target 'target' would be
	     handled.

	     @param target Element to check.
	     @param event Event type.
	     @param nearest_handler If not NULL, set to the nearest ancestor
	                            of 'target' that handles the event or to
	                            'target' if 'target' handles the event.

	     @return TRUE or FALSE. */

	virtual BOOL HasLocalEventHandler(HTML_Element *target, DOM_EventType event) = 0;
	/**< Check whether an event with the target 'target' would be
	     handled by an event listener registered on 'target' itself.

	     @param target Element to check.
	     @param event Event type.
	     @return TRUE or FALSE. */

	virtual OP_STATUS SetEventHandler(DOM_Object *node, DOM_EventType event, const uni_char *handler, int handler_length) = 0;
	/**< Set an event handler on 'node'.  If 'node' already has an
	     event handler for 'event' that was set with this function,
	     it will be replaced. If event is an event that can be used
	     as a window event and the node corresponds to the right
	     BODY/FRAMESET, then the event handler will be set on
	     the window rather than the node.

	     @param node Node to set an event listener on.
	     @param event Event type.
	     @param handler Handler code.
	     @param handler_length Length of 'handler'.

	     @return OpStatus::OK, OpStatus::ERR if 'handler' could not be
	             compiled and OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_STATUS ClearEventHandler(DOM_Object *node, DOM_EventType event) = 0;
	/**< Clears node's event handler, or if the event is an event than can be
	     used as a window event and the node is equal to the right BODY/FRAMESET
	     element, then removes the corresponding window event handler.

	     @param node Node to clear an event listener on.
	     @param event Event type.
	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY on OOM. */

	virtual void AddEventHandler(DOM_EventType type) = 0;
	/**< Increase the handlers counter for the specified event type.

	     @param type Event type. */

	virtual void RemoveEventHandler(DOM_EventType type) = 0;
	/**< Decrease the handlers counter for the specified event type.

	     @param type Event type. */

	/** Event data structure.  Used to describe an event that is to be
	    dispatched by the HandleEvent function. */
	class EventData
	{
	public:
		EventData();
		/**< Constructor.  Sets all members to zero. */

		/*== Event ==*/
		DOM_EventType type;
		/**< Event: type.

		     This is the DOM_EventType enumerator representing the event if one
		     exists.  If the event type is not included in the DOM_EventType
		     enumeration, use the DOM_EVENT_CUSTOM enumerator and use
		     'namespaceURI' and 'type_custom' fields.  If the event type is
			 included the right DOM_EventType enumerator must be used.

			 To translate an type string into the right DOM_EventType
			 enumerator, use the DOM_Environment::GetEventType function. */

		const uni_char *type_custom;    /**< Type, if 'type' == DOM_EVENT_CUSTOM. */
		HTML_Element *target;           /**< Target. */
		int modifiers;                  /**< Shift key state. */

		/*== UIEvent ==*/
		int detail;                     /**< Detail (repeat count for ONCLICK). */

		/*== MouseEvent ==*/
		int screenX;                    /**< MouseEvent: screen X position. */
		int screenY;                    /**< MouseEvent: screen Y position. */
		int offsetX;                    /**< MouseEvent: offset X position. See calculate_offset_lazily. */
		int offsetY;                    /**< MouseEvent: offset Y position. See calculate_offset_lazily. */
		int clientX;                    /**< MouseEvent: client X position. */
		int clientY;                    /**< MouseEvent: client Y position. */
		int button;                     /**< MouseEvent: button. */
		BOOL might_be_click;            /**< MouseEvent: whether an ONMOUSEUP can trigger an ONCLICK */
		BOOL calculate_offset_lazily;   /**< MouseEvent: Whether offsetX and offsetY is precalculated (the
		                                     preferred case) or need to be calculated when needed. If TRUE, then
		                                     offset_x and offset_y must be set to the x and y coordinate of
		                                     the mouse event. */
		BOOL has_keyboard_origin;       /**< MouseEvent: whether event originated from the keyboard event. */
		HTML_Element *relatedTarget;    /**< MouseEvent: related target (can be NULL). */

		/*== KeyboardEvent ==*/
		OpKey::Code key_code;           /**< Virtual key code (OP_KEY_*.) */
		const uni_char *key_value;      /**< String generated by key. */
		BOOL key_repeat;                /**< TRUE iff this is a repeated key event. */
		OpKey::Location key_location;   /**< Keyboard location of key. */
		unsigned key_data;              /**< Extra key event data. */
		OpPlatformKeyEventData *key_event_data; /**< Platform key event data (for plugins.) */

#ifdef PROGRESS_EVENTS_SUPPORT
		/*== ProgressEvent ==*/
		unsigned progressEvent:1;       /**< Create a ProgressEvent? */
		unsigned lengthComputable:1;    /**< ProgressEvent: is total length known? */
		OpFileLength loaded;            /**< ProgressEvent: number of bytes loaded. */
		OpFileLength total;             /**< ProgressEvent: total number of bytes. */
#endif // PROGRESS_EVENTS_SUPPORT

#ifdef CSS_TRANSITIONS
		/*== TransitionEvent ==*/
		double elapsed_time;			/**< The elapsed time for the transition. */
		short css_property;				/**< The enum value for the css property that this transition event is for. */

#ifdef CSS_ANIMATIONS
		const uni_char *animation_name; /**< The value of the animation-name property of the animation that fired the event. */
#endif // CSS_ANIMATIONS
#endif // CSS_TRANSITIONS

		/*== hashchange ==*/
		const uni_char *old_fragment;
		const uni_char *new_fragment;

#ifdef TOUCH_EVENTS_SUPPORT
		int radius;                     /**< Radius of touch in document units. */
		void *user_data;                /**< Data supplied by platform, used for feedback. */
#endif // TOUCH_EVENTS_SUPPORT

#ifdef PAGED_MEDIA_SUPPORT
		unsigned int current_page;	/**< The page we're currently at (in a paged container). */
		unsigned int page_count;	/**< The total number of pages (in a paged container). */
#endif // PAGED_MEDIA_SUPPORT

		unsigned int id;                /**< Event's id. Used to identify an event in case more than one event of a given type may be handled at the same time.
		                                     May also be used to associate an event with some external data. */

		/*== Special stuff ==*/
		BOOL windowEvent;               /**< Event sent to a "window" (body/frameset + document). */
	};

	virtual OP_BOOLEAN HandleEvent(const EventData &data, ES_Thread *interrupt_thread = NULL, ES_Thread **event_thread = NULL) = 0;
	/**< Initiate the handling of an event.  Unless this function returns
	     OpBoolean::IS_TRUE, the reasonable thing to do next is to perform
	     the event's default action (even on OOM).

	     @param data Structure describing the event to be handled.
	     @param interrupt_thread Optional thread that will be
	                             interrupted by the event thread.  If
	                             non-NULL, the event is considered to
	                             be "synthetic" (that is, script
	                             generated.)
	     @param event_thread If event_thread is non-NULL and a new
	                         event thread is created for the event,
	                         then *event_thread is is set to this
	                         thread (otherwise it is unmodified).

	     @return OpBoolean::IS_TRUE if the event will be handled,
	             OpBoolean::IS_FALSE if no handling was necessary
	             and OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_BOOLEAN HandleError(ES_Thread* thread_with_error, const uni_char* message, const uni_char* resource_url, int resource_line) = 0;
	/**< Initiate the handling of a script error (like calling the window.onerror event handler).

	     @return OpBoolean::IS_TRUE if the event will be handled,
	             OpBoolean::IS_FALSE if no handling was necessary
	             and OpStatus::ERR_NO_MEMORY on OOM. */

	virtual void ForceNextEvent() = 0;
	/**< Normally an event would not be sent through the ecmascript engine if there
	     was no listener for it, but by calling this an event will be forced into
	     the slow route regardless. This is to be used when two events are sent
	     at the same time and where the first event might register a listener
	     for the second event that must fire. Example: ONMOUSEOVER -> ONMOUSEMOVE */

#ifdef MEDIA_HTML_SUPPORT
	virtual OP_BOOLEAN HandleTextTrackEvent(DOM_Object* target_object, DOM_EventType event_type, MediaTrack* affected_track = NULL) = 0;
	/**< Initiate the handling of an event related to a media element
	     TextTrack (TextTrack, TextTrackList or TextTrackCue.)

		 @param target_object The object the event should be sent to - non-NULL.
		 @param event_type The type of the event to be sent.
		                   Currently one of:
                               MEDIACUECHANGE,
                               MEDIACUEENTER,
                               MEDIACUEEXIT,
                               MEDIAADDTRACK or
                               MEDIAREMOVETRACK
		 @param affected_track The track object the event concerns (for MEDIA{ADD,REMOVE}TRACK.)

		 @return OpBoolean::IS_TRUE if the event will be handled,
		         OpBoolean::IS_FALSE if no handling was necessary and
		         OpStatus::ERR_NO_MEMORY on OOM. */
#endif // MEDIA_HTML_SUPPORT

#if defined(SELFTEST) && defined(APPLICATION_CACHE_SUPPORT)
	virtual ES_Object* GetApplicationCacheObject() = 0;
	/**< Used by the selftest in the applicationcache module.
	     @returns the application cache object or NULL. */
#endif // SELFTEST && APPLICATION_CACHE_SUPPORT


	/*=== Notifications ===*/

	virtual void NewDocRootElement(HTML_Element *element) = 0;
	/**< Should be called when the document's root (HE_DOC_ROOT) element is set.

	     @param element New root element.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_STATUS NewRootElement(HTML_Element *element) = 0;
	/**< Should be called when the document's root (Markup::HTE_HTML/Markup::SVGE_SVG/...) element is set.

	     @param element New root element.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_STATUS ElementInserted(HTML_Element *element) = 0;
	/**< Should be called when an element is added as a child of another
	     element.  It doesn't matter if the new parent is in a document.

	     OOM triggered by this function is typically non-critical in nature.
	     If several elements are inserted, it is important that each insertion
	     is signalled by a call to this function, even if previous calls have
	     failed.

	     @param element The element being inserted.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS DEPRECATED(ElementInsertedIntoDocument(HTML_Element *element, BOOL recurse = TRUE, BOOL hidden = FALSE));
	/**< Should be called when an element is inserted into the document.

	     @param element The element being inserted.
	     @param recurse If FALSE, element's children are not automatically
	                    updated as well.
	     @param hidden The element should be hidden (e.g. not be added to
	                   the table of named elements).
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_STATUS ElementRemoved(HTML_Element *element) = 0;
	/**< Should be called when an element is removed from its parent, that is,
	     becomes parentless.  It is important to call this function (and then
	     ElementInserted()) even when an element is just moved inside a tree.

	     OOM triggered by this function is typically non-critical in nature.
	     If several elements are removed, it is important that each removal is
	     signalled by a call to this function, even if previous calls have
	     failed.

	     @param element The element being removed.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS DEPRECATED(ElementRemovedFromDocument(HTML_Element *element, BOOL recurse = TRUE));
	/**< Should be called when an element is removed from the document.

	     @param element The element being removed.
	     @param recurse If FALSE, element's children are not automatically
	                    updated as well.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_STATUS DEPRECATED(ElementNameOrIdChanged(HTML_Element *element));
	/**< Shouldn't be called when an element's name or id attribute is
	     is changed.

	     @param element The affected element.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_STATUS ElementCharacterDataChanged(HTML_Element *element, int modification_type, unsigned offset  = 0, unsigned length1 = 0, unsigned length2 = 0) = 0;
	/**< Should be called when an element's character data is changed.

	     @param element The affected element.

	     @param[in] modification_type What kind of modification it
	     does. This is used to perform intelligent changes of for
	     instance selection. Can be MODIFICATION_REPLACING,
	     MODIFICATION_DELETING, MODIFICATION_INSERTING,
	     MODIFICATION_REPLACING_ALL or MODIFICATION_SPLITTING.

		 @param[in] offset If REPLACING, it's the offset of the replaced
	     text. length1 is the length of the replaced text and length2 is
	     the length of the text that was inserted.  <p>If DELETING, the
	     offset of the deleted text and length1 is the length of the
	     inserted text.  <p>If INSERTING, the offset of the inserted
	     text and length1 is the length of the inserted text.  <p>If
	     UNKNOWN, the whole block is assumed to be replaced by a new
	     block.

		 @param[in] length1 See the offset parameter.

		 @param[in] length2 See the offset parameter.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_STATUS ElementAttributeChanged(HTML_Element *element, const uni_char *name, int ns_idx) = 0;
	/**< Should be called when an element's attribute is changed.

	     @param element The affected element.
	     @param name The name of the changed attribute.
	     @param ns_idx The namespace index of the changed attribute.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	enum
	{
		COLLECTION_ALL = 0xffff,
		/**< Status in any collection affected.  Used when element is inserted
		     into or removed from the tree.  Note that removal and insertion is
		     already signalled via calls to ElementInsertedIntoDocument and
		     ElementRemovedFromDocument there is no need to call
		     ElementCollectionStatusChanged then. */

		COLLECTION_NAME_OR_ID = 0x01,
		/**< Status in collections that depend on element's names or ids. */

		COLLECTION_FORM_ELEMENTS = 0x02,
		/**< Status in HTMLFormElement.elements collection.  Affected when
		     changing the type of an input to or from INPUT_IMAGE, or when
		     changing what DOMTemplateHandler::IsInTemplate() returns for an
		     element. */

		COLLECTION_SELECTED_OPTIONS = 0x04,
		/**< Status in HTMLSelectElement.selectedOptions collection.
		     Affected when an option is selected or deselected. */

		COLLECTION_APPLETS = 0x08,
		/**< Status in HTMLDocument.applets collection.  Affected when an
		     OpApplet object is created or anything that can affect the result
		     of HTML_Element::GetResolvedObjectType is changed, for an element
		     other than HE_APPLET (which is always in the collection.)  */

		COLLECTION_PLUGINS = 0x10,
		/**< Status in HTMLDocument.plugins collection.  Affected when an
		     OpNS4Plugin object is created or anything that can affect the
		     result of HTML_Element::GetResolvedObjectType is changed, for an
		     element other than HE_EMBED (which is always in the
		     collection.)  */

		COLLECTION_LINKS = 0x20,
		/**< Status in HTMLDocument.links collection.  Affected when an href
		     attribute is added to or removed from an element of type HE_A or
		     HE_AREA. */

		COLLECTION_ANCHORS = 0x40,
		/**< Status in HTMLDocument.anchors collection.  Affected when a name
		     attribute is added to or removed from an element of type HE_A. */

		COLLECTION_DOCUMENT_NODES = 0x80,
		/**< Status in HTMLDocument automatic collection.  Affected when a
		     name attribute is added to or removed from an element of type
		     HE_FORM, HE_IMG, HE_IFRAME, HE_EMBED, HE_APPLET or HE_CANVAS. */

		COLLECTION_STYLESHEETS = 0x100,
		/**< Status in Document.styleSheets.  Affected when the rel attribute
		     of HE_LINK element is modified. */

		COLLECTION_LABELS = 0x200,
		/**< Status in HTML<form>Element.labels.  Affected when the for
		     attribute of a form element is modified. */

		COLLECTION_CLASSNAME = 0x400,
		/**< Status in collection returned by Document.getElementsByClassName.
		     Affected when the 'class' attribute is modified. */

		COLLECTION_MICRODATA_TOPLEVEL_ITEMS = 0x800,
		/**< Status in collection returned by Microdata's Document.getItems.
		     Affected when the 'itemscope', 'itemprop' or 'itemtype' attributes
		     are modified. */

		COLLECTION_MICRODATA_PROPERTIES = 0x1000,
		/**< Status in Microdata HTMLElement.properties collection.  Affected
		     when the 'itemscope', 'itemprop' or 'itemref' attributes are
		     modifed. */
	};

	virtual void ElementCollectionStatusChanged(HTML_Element *element, unsigned collections) = 0;
	/**< Should be called whenever anything changes that might affect the
	     contents of a DOM collection.  See the COLLECTION_* enumerators above
	     for the different situations in which this might be necessary.  Using
	     COLLECTION_ALL is safe but may hurt unnecessarily much. */

	virtual void OnSelectionUpdated() = 0;
	/**< Should be called whenever the window selection is changed by the user
	     or as a side-effect of operations on it. */

#ifdef USER_JAVASCRIPT

	/*=== User Javascript ===*/

	virtual OP_BOOLEAN HandleScriptElement(HTML_Element *element, ES_Thread *interrupt_thread) = 0;
	/**< Should be called before a script is executed.  If the script
	     element is handled by User Javascript, parsing should be
	     blocked as if the script was being executed.  The script element
	     will be handled or cancelled by User Javascript once User
	     Javascript processing is done.

	     @param element The script element.
	     @param interrupt_thread The thread the script element would
	                             interrupt, if any.

	     @return OpBoolean::IS_TRUE if the script element is handled by
	             User Javascript, OpBoolean::IS_FALSE if it is not and
	             OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_STATUS HandleScriptElementFinished(HTML_Element *element, ES_Thread *script_thread) = 0;
	/**< Should be called when a script has finished executing.

	     @param element The script element.
	     @param script_thread The script element's thread.

	     @return OpBoolean::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_BOOLEAN HandleExternalScriptElement(HTML_Element *element, ES_Thread *interrupt_thread) = 0;
	/**< Should be called before an external script is loaded.  If the
	     script element is handled by User Javascript, parsing should
	     be blocked as if the script was being executed.  The script
	     element will be loaded or cancelled once User Javascript is
	     done.

	     @param element The script element.
	     @param interrupt_thread The thread the script element would
	                             interrupt, if any.

	     @return OpBoolean::IS_TRUE if the script element is handled by
	             User Javascript, OpBoolean::IS_FALSE if it is not and
	             OpStatus::ERR_NO_MEMORY on OOM. */

	virtual BOOL IsHandlingScriptElement(HTML_Element *element) = 0;
	/**< Returns TRUE if the currently executing script is a User
	     Javascript script handling a 'BeforeExternalScript' or
	     'BeforeScript' event for 'element'.

	     @param element The script element.

	     @return TRUE if the script element is currently being handled
	             by User Javascript, FALSE otherwise. */

	virtual OP_BOOLEAN HandleCSS(HTML_Element *element, ES_Thread *interrupt_thread) = 0;
	/**< Should be called before a css fragment is parsed, either a linked
	     stylesheet or a style element.  If the css fragment is handled by
	     User Javascript, the css will only be parsed after.

	     @param element The style element or placeholder for the css fragment.
	     @param interrupt_thread The thread the script element would
	                             interrupt, if any.

	     @return OpBoolean::IS_TRUE if the script element is handled by
	             User Javascript, OpBoolean::IS_FALSE if it is not and
	             OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_BOOLEAN HandleCSSFinished(HTML_Element *element, ES_Thread *interrupt_thread) = 0;
	/**< Should be called after a css fragment or linked stylesheet have been parsed

	     @param element The style element or placeholder for the css fragment.
	     @param interrupt_thread The thread the script element would
	                             interrupt, if any.

	     @return OpBoolean::IS_TRUE if the script element is handled by
	             User Javascript, OpBoolean::IS_FALSE if it is not and
	             OpStatus::ERR_NO_MEMORY on OOM. */

#ifdef _PLUGIN_SUPPORT_
	virtual OP_BOOLEAN PluginInitializedElement(HTML_Element *element) = 0;
	/**< Should be called when a plugin has initialized.
	     @param element The OBJECT or EMBED element.

	     @return OpBoolean::IS_TRUE if the element is handled by
	             User Javascript, OpBoolean::IS_FALSE if it is not and
	             OpStatus::ERR_NO_MEMORY on OOM. */

#endif // _PLUGIN_SUPPORT_

	virtual OP_STATUS HandleJavascriptURL(ES_JavascriptURLThread *thread) = 0;
	/**< Should be called before a javascript: URL is executed.

	     @param thread The thread handling the javascript: URL.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_STATUS HandleJavascriptURLFinished(ES_JavascriptURLThread *thread) = 0;
	/**< Should be called after a javascript: URL has finished executing
	     but before its return value (if any) is handled.

	     @param thread The thread handling the javascript: URL.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	static BOOL GetUserJSEnabled();
	/**< Returns TRUE if User Javascript is enabled. */

	static unsigned GetUserJSFilesCount();
	/**< Returns the number of User Javascript files used.

	     @return The number of files used. */

	static const uni_char *GetUserJSFilename(unsigned index);
	/**< Returns the filename of the index-th User Javascript file used.

	     @param index File index.  Should be less than the value returned
	                  by GetUserJavascriptFilesCount.

	     @return A string that is owned by the DOM module.  It needs t
	             copied if it is not used right away. */

	static OP_STATUS UpdateUserJSFiles();
	/**< Update the list of User Javascript files used and reload all the
	     files.

	     @return OpStatus::OK or OpStatus:ERR_NO_MEMORY. */

#ifdef DOM_BROWSERJS_SUPPORT

	static OP_BOOLEAN CheckBrowserJSSignature(const OpFile &file);
	/**< Loads the content of 'file' and checks that it is a properly
	     formatted and signed browser.js file.  The file needs to be
	     constructed with the appropriate file name, but need not be
	     opened.

	     @param file OpFile object representing the file to check.

	     @return OpStatus::ERR if the file didn't exist or couldn't be
	             loaded, OpBoolean::IS_TRUE if it existed, could be
	             loaded and is a properly formatted and signed
	             browser.js file and OpBoolean::IS_FALSE if it
	             existed, could be loaded but was not a properly
	             formatted and signed browser.js file. */

#endif // DOM_BROWSERJS_SUPPORT
#endif // USER_JAVASCRIPT

#ifdef ECMASCRIPT_DEBUGGER
	virtual OP_STATUS GetESRuntimes(OpVector<ES_Runtime> &es_runtimes) = 0;
	/**< Gets the list of ES_Runtimes associated with this DOM environment. A DOM
	     environment may have more than one runtime associated to it if there for
	     example are active extensions running.

	     @param [out] es_runtimes A OpVector to store the ES_Runtimes in.
	     @return OpStatus::OK on success, or OOM. */
#endif // ECMASCRIPT_DEBUGGER

#ifdef WEBSOCKETS_SUPPORT
	/**< Close all active WebSockets in the environment.

		@return TRUE if there was a WebSocket that was closed in the environment. */
	virtual BOOL CloseAllWebSockets() = 0;
#endif //WEBSOCKETS_SUPPORT

#ifdef JS_PLUGIN_SUPPORT

	/*=== JS plugins ===*/

	virtual JS_Plugin_Context *GetJSPluginContext() = 0;
	/**< Retrieve the environment's JS plugin context.

	     @return A JS plugin context or NULL. */

	virtual void SetJSPluginContext(JS_Plugin_Context *context) = 0;
	/**< Set the environment's JS plugin context.

	     @param context A JS plugin context or NULL. */

#endif // JS_PLUGIN_SUPPORT


#ifdef GADGET_SUPPORT

	/*=== Gadget support ===*/

	enum GadgetEvent
	{
		GADGET_EVENT_ONDRAGSTART,
		GADGET_EVENT_ONDRAGSTOP,
		GADGET_EVENT_ONSHOW,
		GADGET_EVENT_ONHIDE,
		GADGET_EVENT_ONMODECHANGE,
		GADGET_EVENT_ONRESOLUTION,
		GADGET_EVENT_ONSHOWNOTIFICATIONFINISHED,
		GADGET_EVENT_ONICONCHANGED,
		GADGET_EVENT_ONICONDATACHANGED
#ifdef DOM_JSPLUGIN_CUSTOM_EVENT_HOOK
		,GADGET_EVENT_CUSTOM
#endif // DOM_JSPLUGIN_CUSTOM_EVENT_HOOK
#ifdef DOM_JIL_API_SUPPORT
		,GADGET_EVENT_ONFOCUS
#endif // DOM_JIL_API_SUPPORT
	};

	/** Event data structure for gadget events.  Used to describe an event that is to be
	    dispatched by the HandleGadgetEvent function. */

	class GadgetEventData
	{
	public:
		GadgetEventData();					/**< Constructor.  Sets all members to zero. */

		/*== GADGET_EVENT_ONMODECHANGE ==*/
		const uni_char *mode;				/**< Copied by HandleGadgetEvent(); owned by the caller. */

		/*== GADGET_EVENT_ONRESOLUTION ==*/
		int screen_width;
		int screen_height;

#ifdef DOM_JSPLUGIN_CUSTOM_EVENT_HOOK
		/*== GADGET_EVENT_CUSTOM ==*/
		const uni_char *type_custom;	/**< event name */
		int param;						/**< event param */
#endif // DOM_JSPLUGIN_CUSTOM_EVENT_HOOK
	};

	virtual OP_STATUS HandleGadgetEvent(GadgetEvent event, GadgetEventData *data = NULL) = 0;
	/**< Called when a gadget event needs to be sent to hooks defined
	     by the gadget developer (in javascript).

	     @param event The event that has occurred.
		 @param data Data related to some events. For GADGET_EVENT_ONMODECHANGE, GadgetEventData::mode
		 contains the new mode. For GADGET_EVENT_ONRESOLUTION, GadgetEventData::screen_width and
		 GadgetEventData::screen_height are the new screen dimensions.

	     @return OpStatus::OK, OpStatus::ERR, or OpStatus::ERR_NO_MEMORY. */

#endif // GADGET_SUPPORT


	/*=== External callbacks ===*/

	/** Different locations where callbacks can be registered (object on which
        the callbacks can be put as properties). */
	enum CallbackLocation
	{
		GLOBAL_CALLBACK,
		/**< Callback available on the global object. */

		OPERA_CALLBACK
		/**< Callback available on the 'opera' object. */
	};

	/** Security domain information for the script that calls a callback. */
	class CallbackSecurityInfo
	{
	public:
		const uni_char *domain;
		URLType type;
		int port;
	};

	typedef int (*CallbackImpl)(ES_Value *argv, int argc, ES_Value *return_value, CallbackSecurityInfo *security_info);
	/**< Callback implementation.

	     @param argv Array of arguments, containing least 'argc' elements.
	     @param argc Number of arguments.
	     @param return_value Value that should be set to the function's returned value.
	     @param security_info Security domain information.

	     @return ES_FAILED, ES_VALUE, ES_EXCEPTION or ES_NO_MEMORY. */

	typedef int (*ExtendedCallbackImpl)(ES_Value *argv, int argc, ES_Value *return_value, OpWindowCommander* origining_window, CallbackSecurityInfo *security_info);
	/**< Callback implementation. For every call to a callback of this type the "active frame" pointer in
	     the doc/dochand modules will be set to the frame running the script. That way any operations
	     targetted at the OpWindowCommander that depends on the "active frame" pointer will arrive at the
	     right document environment.

	     @param argv Array of arguments, containing least 'argc' elements.
	     @param argc Number of arguments.
	     @param return_value Value that should be set to the function's returned value.
	     @param origining_window The window with the document that is calling the function.
	     @param security_info Security domain information.

	     @return ES_FAILED, ES_VALUE, ES_EXCEPTION or ES_NO_MEMORY. */

	static OP_STATUS AddCallback(CallbackImpl impl, CallbackLocation location, const char *name, const char *arguments);
	/**< Add a callback function that will be available in every DOM
	     environment.  For more information about the format of the
		 'arguments' string, see the documentation of the function
		 'ES_Runtime::SetFunctionRuntime' (in the ecmascript module.)

	     @param impl The callback implementation.
	     @param location Where the callback will be available.
	     @param name The function name.
	     @param arguments The argument types.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static OP_STATUS AddCallback(ExtendedCallbackImpl impl, CallbackLocation location, const char *name, const char *arguments);
	/**< Add a callback function that will be available in every DOM
	     environment.  For more information about the format of the
		 'arguments' string, see the documentation of the function
		 'ES_Runtime::SetFunctionRuntime' (in the ecmascript module.)

	     @param impl The callback implementation.
	     @param location Where the callback will be available.
	     @param name The function name.
	     @param arguments The argument types.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	/*=== Popup blocking ===*/

	static OP_STATUS OpenURLWithTarget(URL url, DocumentReferrer& refurl, const uni_char *target, FramesDocument *document, ES_Thread *thread, BOOL user_initiated = FALSE, BOOL plugin_unrequested_popup = FALSE);
	/**< Called when a script clicks a link (submits a form, et.c.)
	     with a target that would cause a new window to be opened.
	     Calls OpDocumentListener::OnJSPopup and support delayed
	     opening and all.  Since the operation might be asynchronous,
	     there is no feedback (other than OOM signalling) from this.

	     @param url The URL to open.
	     @param refurl The referrer url to use (empty URL is okay.)
	     @param document The document from which the URL is opened.
	     @param thread The script thread that triggered the popup.
	     @param user_initiated The user initiated this request.
	     @param plugin_unrequested_popup A plugin initiated this
	                                     request.  Only relevant if
	                                     plugin support is enabled.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static OP_STATUS OpenWindowWithData(const uni_char *data, FramesDocument *document, ES_JavascriptURLThread *thread, BOOL user_initiated = FALSE);
	/**< Called when opening a javascript: URL that returns a string
	     value in a new window, for instance by shift-clicking the
	     link.

	     @param data Document data.
	     @param document The document from which the URL is opened.
	     @param thread The javascript: URL thread.
	     @param user_initiated The user initiated this request.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static BOOL IsCalledFromUnrequestedScript(ES_Thread *thread = NULL);
	/**< Returns TRUE if the code that calls it is called from a
	     script that is unrequested.  Use with care if you don't have
	     a reference to the calling ES_Thread.  If you are unsure
	     whether it is appropriate for you to call this function
	     without a thread from somewhere, please consult the DOM
	     module (co-)owner(s).  Don't assume it will provide you with
	     a good answer.

	     @param thread The currently executing thread.  If NULL, a
	                   global flag is consulted.  This flag is only
	                   accurate if a script is executing right now.

	     @return TRUE if a script is executing right now and that
	             script is unrequested. */

	virtual void OnWindowActivated() = 0;
	/**< Called when the window that an environment appears within is
	     activated. */

	virtual void OnWindowDeactivated() = 0;
	/**< Called when the window that an environment appears within is
	     deactivated. */

protected:
	virtual ~DOM_Environment() {}
	/**< Virtual destructor so that compilers won't give bogus
	     warnings claiming this class needs a virtual destructor,
	     which it doesn't. */
};


/** Representation of the DOM proxy environment owned by each frame
  * (DocumentManager).
  *
  * Provides a proxy window object that redirects all operations
  * performed on it to the window belonging to the document
  * currently displayed in the frame and a mechanism for updating
  * which document that is.
  */
class DOM_ProxyEnvironment
{
public:
	/*=== Creation and destruction ===*/

	static OP_STATUS Create(DOM_ProxyEnvironment *&env);
	/**< Create a DOM proxy environment.

	     @param env Set to the created environment on success.

	     @return OpStatus::OK on success, OpStatus::ERR if ECMAScript is
	             disabled or OpStatus::ERR_NO_MEMORY on OOM. */

	static void Destroy(DOM_ProxyEnvironment *env);
	/**< Destroy a DOM proxy environment.

	     @param env Environment to destroy.  Can be NULL. */


	/*=== Proxy/real object management ===*/

	class RealWindowProvider
	{
	public:
		virtual OP_STATUS GetRealWindow(DOM_Object *&window) = 0;
		/**< Called to retrieve a real window object when one is needed. */

	protected:
		virtual ~RealWindowProvider() {}
		/**< Virtual destructor so that compilers won't give bogus
		     warnings claiming this class needs a virtual destructor,
		     which it doesn't. */
	};

	virtual void SetRealWindowProvider(RealWindowProvider *provider) = 0;
	/**< Set a callback provider that can be used to retrieve the real
	     window object when it is needed.  Clears any object set by
	     SetRealWindow.

	     @param provider Real window object provider.  Can be NULL. */

	virtual void Update() = 0;
	/**< Signal that previously returned "real windows" might be inaccurate, and
	     thus request a new call to GetRealWindow() the next time the real
	     object is needed. */

	virtual OP_STATUS GetProxyWindow(DOM_Object *&proxy_window, ES_Runtime *origining_runtime) = 0;
	/**< Get the proxy window object to use in the specified runtime.

	     @param proxy_window Set to the proxy window on success.
	     @param origining_runtime Origining runtime.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

protected:
	virtual ~DOM_ProxyEnvironment() {}
	/**< Virtual destructor so that compilers won't give bogus
	     warnings claiming this class needs a virtual destructor,
	     which it doesn't. */
};

inline OP_STATUS DOM_Environment::ElementNameOrIdChanged(HTML_Element *element)
{
	return OpStatus::OK;
}

#endif // DOM_ENVIRONMENT_H
