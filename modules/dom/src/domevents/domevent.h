/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_EVENT_H
#define DOM_EVENT_H

#include "modules/dom/src/domobj.h"
#include "modules/dom/domeventtypes.h"
#include "modules/dom/domevents.h"

class OpPlatformKeyEventData;

class DOM_Node;
class DOM_EventThread;

#ifdef DOM3_EVENTS
class DOM_EventStoppedForGroup;
#endif // DOM3_EVENTS
#ifdef DOM2_EVENTS_API
class DOM_ExternalEvent;
#endif // DOM2_EVENTS_API

class DOM_Event
	: public DOM_Object
{
protected:
	enum
	{
		STOP_PROPAGATION_NONE      = 0,
		STOP_PROPAGATION_DEFERRED  = 1,
		STOP_PROPAGATION_IMMEDIATE = 2
	};

	DOM_EventType known_type;

#ifdef DOM3_EVENTS
	uni_char *namespaceURI;
#endif // DOM3_EVENTS
	uni_char *type;

	unsigned bubbles:1;
	unsigned stop_propagation:2;
	unsigned cancelable:1;
	unsigned prevent_default:1;
	unsigned event_return_value_set:1;
	unsigned synthetic:1;
	unsigned signal_document_finished:1;
	unsigned window_event:1;

	ES_Value event_return_value;
	ES_EventPhase event_phase;
	double timestamp;

#ifdef DOM2_EVENTS_API
	DOM_ExternalEvent *external_event;
#endif // DOM2_EVENTS_API


	/**
	 * The target that the event will be dispatched
	 * to. Most often the same object as GetEvent()->GetTarget but in case
	 * of some window events GetEvent()->GetTarget will be the document
	 * but the event will be sent to the window.
	 */
	DOM_Object *dispatch_target;

	DOM_Object *target, *real_target, *current_target;

#ifdef DOM3_EVENTS
	ES_Object *current_group;
	DOM_EventStoppedForGroup *stopped_for_group;
	unsigned stop_propagation_later:1;
#endif // DOM3_EVENTS

	DOM_EventThread *thread;

	HTML_Element *GetTargetElement();
	ES_GetState DOMSetElementParent(ES_Value *value, DOM_Object* obj);
	HTML_Element **bubbling_path;
	int bubbling_path_len;

public:
	DOM_Event();
	virtual ~DOM_Event();

	/**
	 * @param[in] target The element that the event will be sent to, but look at the dispatch_target parameter.
	 *
	 * @param[in] real_target A more exact description of the event target. Typically a text node below the
	 * element node that is target.
	 *
	 * @param[in] dispatch_target Sometimes the official target of an event will differ from where it's
	 * actually sent. In that case you can set a non_NULL dispatch_target. This is typically used for
	 * some window events that are sent to the window object but will offically have the documen as target.
	 * They should not be sent to the document though. Sending NULL here will make the event be
	 * dispatched to the ordinary target. That should be the common case.
	 */
	void InitEvent(DOM_EventType type, DOM_Object *target, DOM_Object *real_target = NULL, DOM_Object *dispatch_target = NULL);

	OP_STATUS SetType(const uni_char *type);

	void SetSynthetic() { synthetic = TRUE; }
	void SetThread(DOM_EventThread *new_thread) { thread = new_thread; }
	void SetEventPhase(ES_EventPhase new_event_phase) { event_phase = new_event_phase; }
	void SetTarget(DOM_Object *new_target) { target = new_target; }
#ifdef DOM3_EVENTS
	void SetCurrentTarget(DOM_Object *new_current_target);
	void SetCurrentGroup(ES_Object *new_current_group) { current_group = new_current_group; }
#else // DOM3_EVENTS
	void SetCurrentTarget(DOM_Object *new_current_target) { current_target = new_current_target; }
#endif // DOM3_EVENTS
	void SetSignalDocumentFinished() { signal_document_finished = 1; }
	void SetWindowEvent() { window_event = 1; }
	void SetBubblingPath(HTML_Element **path, int path_len);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EVENT || DOM_Object::IsA(type); }
	virtual void GCTrace();
	virtual BOOL AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op);

	virtual OP_STATUS DefaultAction(BOOL cancelled);
	virtual ShiftKeyState GetModifiers();

	DOM_EventType GetKnownType();
#ifdef DOM3_EVENTS
	const uni_char *GetNamespaceURI() { return namespaceURI; }
#endif // DOM3_EVENTS
	const uni_char *GetType() { return type; }
	BOOL GetBubbles() { return bubbles; }
	void SetBubbles(BOOL new_bubbles) { bubbles = new_bubbles; }
	BOOL GetCancelable() { return cancelable; }
	void SetCancelable(BOOL new_cancelable) { cancelable = new_cancelable; }
#ifdef DOM3_EVENTS
	BOOL GetStopPropagation(ES_Object *group);
#else // DOM3_EVENTS
	BOOL GetStopPropagation() { return (stop_propagation & STOP_PROPAGATION_DEFERRED) != 0; }
#endif // DOM3_EVENTS
	BOOL GetStopImmediatePropagation() { return (stop_propagation & STOP_PROPAGATION_IMMEDIATE) != 0; }
	BOOL GetPreventDefault() { return prevent_default; }
	BOOL GetSynthetic() { return synthetic; }
	BOOL GetWindowEvent() { return window_event; }
	/**
	 * The target of the event as by the DOM specification. In HTML events this
	 * is en element, the document or the window.
	 */
	DOM_Object *GetTarget() { return target; }

	/**
	 * Slightly more exact information about the target or NULL.
	 * Typically used to specify which text node under an element
	 * was clicked so that we can bubble the event correctly
	 * considering last_descendant.
	 */
	DOM_Object *GetRealTarget() { return real_target; }

	/**
	 * The target that the event will be dispatched
	 * to. Most often NULL which means "the same object
	 * as GetEvent()->GetTarget()" but in case
	 * of some window events GetEvent()->GetTarget() will be
	 * the document
	 * while the event will be sent to the window.
	 */
	DOM_Object *GetDispatchTarget() { return dispatch_target ? dispatch_target : target; }

	ES_EventPhase GetEventPhase() { return event_phase; }
	double GetTimeStamp() { return timestamp; }
	DOM_Object *GetCurrentTarget() { return current_target; }
	DOM_EventThread *GetThread() { return thread; }
#ifdef DOM3_EVENTS
	ES_Object *GetCurrentGroup() { return current_group; }
#endif // DOM3_EVENTS

#ifdef DOM3_EVENTS
	OP_STATUS SetStopPropagation(BOOL value, BOOL immediate);
#else // DOM3_EVENTS
	void SetStopPropagation(BOOL value);
	void SetStopImmediatePropagation(BOOL value);
#endif // DOM3_EVENTS
	void SetPreventDefault(BOOL value) { prevent_default = value; }

#ifdef DOM2_EVENTS_API
	OP_STATUS GetExternalEvent(DOM_EventsAPI::Event *&event);
#endif // DOM2_EVENTS_API

#ifdef DOM3_EVENTS
	static DOM_EventType GetEventType(const uni_char *namespaceURI, const uni_char *type_string, BOOL as_property);
#else // DOM3_EVENTS
	static DOM_EventType GetEventType(const uni_char *type_string, BOOL as_property);
#endif // DOM3_EVENTS

	/** Events that can be sent to the window object */
	static BOOL IsWindowEvent(DOM_EventType type);
	/**
	 * Events that can be sent to the window object, or is a document event
	 * that bubbles to the window (but only the ones that can be declared as
	 * body attributes, e.g. <body onscroll="">)
	 *
	 * @see IsDocumentEvent()
	 */
	static BOOL IsWindowEventAsBodyAttr(DOM_EventType type);
	/** Events that are dispatched on the document but also bubble to the window. */
	static BOOL IsDocumentEvent(DOM_EventType type);

	static BOOL IsAlwaysPresentAsProperty(DOM_Object *target, DOM_EventType type);
	static void FetchNamedHTMLElmEventPropertiesL(ES_PropertyEnumerator *enumerator, HTML_Element* elm);
	static void FetchNamedHTMLDocEventPropertiesL(ES_PropertyEnumerator *enumerator);

	/** Initialize the Event constructor and prototype objects. */
	static void ConstructEventObjectL(ES_Object *object, DOM_Runtime *runtime);

	DOM_DECLARE_FUNCTION(preventDefault);
	DOM_DECLARE_FUNCTION(initEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };

	DOM_DECLARE_FUNCTION_WITH_DATA(stopPropagation);
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 3 };
};

class DOM_Event_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_Event_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::EVENT_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);

	static void AddConstructorL(DOM_Object *object);
};

class DOM_UIEvent
	: public DOM_Event
{
protected:
	DOM_Object *view;
	int detail;

public:
	DOM_UIEvent();

	void InitUIEvent(DOM_Object *view, int detail);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_UIEVENT || DOM_Event::IsA(type); }
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(initUIEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

class DOM_MouseEvent
	: public DOM_UIEvent
{
protected:
	friend class DOM_EventThread;

	int screen_x, screen_y;
	int client_x, client_y;
	BOOL calculate_offset_lazily;
	int offset_x, offset_y;
	BOOL ctrl_key;
	BOOL alt_key;
	BOOL shift_key;
	BOOL meta_key;
	int button;
	BOOL might_be_click; // whether an ONMOUSEUP event can trigger ONCLICK or ONCONTEXTMENU
	BOOL has_keyboard_origin; // for specific case when ONCONTEXTMENU is triggered form the keyboard
	DOM_Node *related_target;

	void TranslatePropertyName(OpAtom &property_name, BOOL& hide_text_elements);
	OP_STATUS CalculateOffset();

public:
	DOM_MouseEvent();

	void InitMouseEvent(long screen_x, long screen_y, long client_x, long client_y,
	                    BOOL calculate_offset_lazily, long offset_x, long offset_y,
	                    ShiftKeyState modifiers, int button, BOOL might_be_click,
	                    BOOL has_keyboard_origin, DOM_Node *related_target);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_MOUSEEVENT || DOM_UIEvent::IsA(type); }
	virtual void GCTrace();

	virtual OP_STATUS DefaultAction(BOOL cancelled);
	virtual ShiftKeyState GetModifiers();

	int GetButton() { return button; }

	DOM_DECLARE_FUNCTION(initMouseEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

class DOM_KeyboardEvent_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_KeyboardEvent_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::KEYBOARDEVENT_PROTOTYPE)
#ifdef DOM_EVENT_VIRTUAL_KEY_CONSTANTS
		, added_constants(FALSE)
#endif // DOM_EVENT_VIRTUAL_KEY_CONSTANTS
	{
	}

	virtual int Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);

	static void AddConstructorL(DOM_Object *object);

#ifdef DOM_EVENT_VIRTUAL_KEY_CONSTANTS
	static void AddKeyConstantsL(ES_Object *object, DOM_Runtime *runtime);

	/* Methods provided to allow delayed initialization of constructor
	   constants. */
	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);
	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);

private:
	BOOL added_constants;
	/**< TRUE if native object has been extended with large block
	     of virtual key constants. */
#endif // DOM_EVENT_VIRTUAL_KEY_CONSTANTS
};

class DOM_KeyboardEvent
	: public DOM_UIEvent
{
protected:
	friend class DOM_EventThread;
	friend class DOM_KeyboardEvent_Constructor;

	OpKey::Code key_code;
	const uni_char *key_value;
	BOOL ctrl_key;
	BOOL alt_key;
	BOOL shift_key;
	BOOL meta_key;
	BOOL repeat;
	OpKey::Location location;
	unsigned data;
	OpPlatformKeyEventData *key_event_data;

	unsigned TranslateKey(OpAtom property_name);
	/**< Convert the event into some numeric coded value, dependent on property_name.
	     (which, keyCode, and charCode.) */

	BOOL FiresKeypress();
	/**< Returns TRUE if this keyboard event must fire a keypress event. */

public:
	DOM_KeyboardEvent();
	virtual ~DOM_KeyboardEvent();

	static void ConstructKeyboardEventObjectL(ES_Object *object, DOM_Runtime *runtime);

	OP_STATUS InitKeyboardEvent(OpKey::Code new_key_code, const uni_char *new_value, unsigned new_modifiers, BOOL new_repeat, OpKey::Location new_location, unsigned data, OpPlatformKeyEventData *platform_event_data);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_KEYBOARDEVENT || DOM_UIEvent::IsA(type); }

	virtual OP_STATUS DefaultAction(BOOL cancelled);
	virtual ShiftKeyState GetModifiers();

	OpKey::Code GetKeyCode() { return key_code; }

	OP_STATUS GetCharValue(const uni_char *&result, TempBuffer *buffer);
	/**< Derive the character value string produced by this event's key,
	     as reported by the 'char' property.

	     If the key doesn't have a character value (a "function key"
	     to use the DOM Level3 key event spec term), the empty string
	     is returned. If a non-function key, but required to have a
	     fixed Unicode codepoint by the spec (specified locally using
	     the module.keys configuration for that key), its string representation
	     is generated.

	     @param [out]result The result string; only modified on success.
	     @param buffer Temporary buffer, assumed empty, to use in constructing
	            the character value.
	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	const uni_char *GetKeyString();
	/**< Return the string representation of the event's key, as reported by
	     the 'key' property. */

	DOM_DECLARE_FUNCTION(getModifierState);
	DOM_DECLARE_FUNCTION(initKeyboardEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };
};

class DOM_TextEvent
	: public DOM_UIEvent
{
public:
	enum InputMethod {
		DOM_INPUT_METHOD_UNKNOWN = 0,
		DOM_INPUT_METHOD_KEYBOARD,
		DOM_INPUT_METHOD_PASTE,
		DOM_INPUT_METHOD_DROP,
		DOM_INPUT_METHOD_IME,
		DOM_INPUT_METHOD_OPTION,
		DOM_INPUT_METHOD_HANDWRITING,
		DOM_INPUT_METHOD_VOICE,
		DOM_INPUT_METHOD_MULTIMODAL,
		DOM_INPUT_METHOD_SCRIPT,

		DOM_INPUT_METHOD_LAST = DOM_INPUT_METHOD_SCRIPT
	};

	DOM_TextEvent();
	virtual ~DOM_TextEvent();

	static void ConstructTextEventObjectL(ES_Object *object, DOM_Runtime *runtime);

	OP_STATUS InitTextEvent(const uni_char *new_text, InputMethod new_input_method = DOM_INPUT_METHOD_UNKNOWN, const uni_char *new_locale = NULL);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_TEXTEVENT || DOM_UIEvent::IsA(type); }

	DOM_DECLARE_FUNCTION(initTextEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };

protected:
	friend class DOM_TextEvent_Constructor;

	uni_char *text_data;
	InputMethod input_method;
	uni_char *locale;
};

class DOM_TextEvent_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_TextEvent_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::TEXTEVENT_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);

	static void AddConstructorL(DOM_Object *object);
};

class DOM_ErrorEvent
	: public DOM_Event
{
protected:
	friend class DOM_EventThread;

	OpString message;
	OpString resource_url;
	OpString resource_line;
	unsigned int resource_line_number;

public:
	OP_STATUS InitErrorEvent(const uni_char* msg, const uni_char* res_url, unsigned int res_line);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_ERROREVENT || DOM_Event::IsA(type); }

	const uni_char* GetMessage() { return message.CStr(); }
	const uni_char* GetResourceUrl() { return resource_url.CStr(); }
	const uni_char* GetResourceLine() { return resource_line.CStr(); }
	unsigned int    GetResourceLineNumber() { return resource_line_number; }

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	DOM_DECLARE_FUNCTION(initErrorEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

class DOM_HashChangeEvent
	: public DOM_Event
{
	OpString old_fragment;
	OpString new_fragment;

	BOOL fragments_are_urls;
	/**< initHashChangeEvent() in javascript passes full urls.
	 *   HandleHashChangeEvent() in Framesdocument only passes the fragments.
	 *   This bool tells what old_fragment and new_fragment hold.
	 */

public:
	DOM_HashChangeEvent() : fragments_are_urls(TRUE){}
	/**< Default constructor. After the object is created, the
	 *   fragments will be empty but can later be set either
	 *   using initHashChangeEvent, or calling ::Make() to create
	 *   and event object with fragments set
	 */
	virtual ~DOM_HashChangeEvent(){}

	static OP_STATUS Make(DOM_Event *&event,
			const uni_char *old_fragment,
			const uni_char *new_fragment,
			DOM_Runtime *runtime);
	/**< Create an event object and fills in the fragments.
	 *   Note that old_fragment and new_fragment are just the
	 *   fragment identifiers from the url */

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HASHCHANGEEVENT || DOM_Event::IsA(type); }

	DOM_DECLARE_FUNCTION(initHashChangeEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

class DOM_CustomEvent
	: public DOM_Event
{
protected:
	ES_Value detail;

public:
	DOM_CustomEvent(){}
	virtual ~DOM_CustomEvent();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_CUSTOMEVENT || DOM_Event::IsA(type); }
	virtual void GCTrace();

	ES_Value &GetDetail() { return detail; }

	DOM_DECLARE_FUNCTION(initCustomEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

class DOM_CustomEvent_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_CustomEvent_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::CUSTOMEVENT_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);

	static void AddConstructorL(DOM_Object *object);
};

#ifdef WEBSOCKETS_SUPPORT

/*interface CloseEvent : Event {
  readonly attribute boolean wasClean;
  readonly attribute unsigned short code;
  readonly attribute DOMString reason;
};*/
class DOM_CloseEvent
	: public DOM_Event
{
public:
	static OP_STATUS Make(DOM_CloseEvent *&event, DOM_Runtime *runtime, const uni_char *type);
	static OP_STATUS Make(DOM_CloseEvent *&event, DOM_Object *target, BOOL clean, unsigned short code, const uni_char *reason);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type);

	DOM_DECLARE_FUNCTION(initCloseEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };

	DOM_CloseEvent()
		: close_clean(FALSE)
		, close_code(0)
	{
	}

private:
	friend class DOM_CloseEvent_Constructor;

	BOOL close_clean;
	unsigned short close_code;
	OpString close_reason;
};

class DOM_CloseEvent_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_CloseEvent_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::CLOSEEVENT_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);

	static void AddConstructorL(DOM_Object *object);
};

#endif //WEBSOCKETS_SUPPORT

class DOM_PopStateEvent : public DOM_Event
{
public:
	virtual ~DOM_PopStateEvent()
	{
		DOMFreeValue(state);
	}
	static OP_STATUS Make(DOM_Event *&event, DOM_Runtime *runtime);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_POPSTATEEVENT || DOM_Event::IsA(type); }
	virtual void GCTrace()
	{
		DOM_Event::GCTrace();
		GCMark(state);
	}

	DOM_DECLARE_FUNCTION(initPopStateEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };

private:
	ES_Value state;
};

#ifdef PAGED_MEDIA_SUPPORT

class DOM_PageEvent
	: public DOM_Event
{
protected:

	int current_page;
	int page_count;

public:

	static OP_STATUS Make(DOM_Event *&event, DOM_Runtime *runtime, unsigned int current_page, unsigned int page_count);

	DOM_PageEvent() :
		current_page(0),
		page_count(1)
	{
	}

	DOM_PageEvent(int current_page, int page_count) :
		current_page(current_page),
		page_count(page_count)
	{
	}

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_PAGEEVENT || DOM_Event::IsA(type); }

	DOM_DECLARE_FUNCTION(initPageEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

class DOM_PageEvent_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_PageEvent_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::PAGEEVENT_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);

	static void AddConstructorL(DOM_Object *object);
};

#endif // PAGED_MEDIA_SUPPORT

#ifdef MEDIA_HTML_SUPPORT

class DOM_TrackEvent
	: public DOM_Event
{
public:
	DOM_TrackEvent() : track(NULL) {}

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual void GCTrace();

	void SetTrack(ES_Object* affected_track) { track = affected_track; }

private:
	ES_Object* track;
};

class DOM_TrackEvent_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_TrackEvent_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::TRACKEVENT_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);

	static void AddConstructorL(DOM_Object* object);
};

#endif // MEDIA_HTML_SUPPORT

#ifdef DRAG_SUPPORT
class OpDragObject;
class DOM_DataTransfer;

class DOM_DragEvent : public DOM_MouseEvent
{
private:
	DOM_DataTransfer *m_data_transfer;
public:
	DOM_DragEvent()
	: m_data_transfer(NULL)
	{}

	static OP_STATUS Make(DOM_Event *&event, DOM_Runtime *runtime, DOM_EventType event_type);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_DRAGEVENT || DOM_MouseEvent::IsA(type); }
	virtual void GCTrace();
	OpDragObject *GetDataStore();
	void SetDataTransfer(DOM_DataTransfer* data_transfer) { m_data_transfer = data_transfer; }
	DOM_DataTransfer* GetDataTransfer() { return m_data_transfer; }

	DOM_DECLARE_FUNCTION(initDragEvent);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

class DOM_DragEvent_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_DragEvent_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::DRAGEVENT_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);
	static void AddConstructorL(DOM_Object* object);
};

#endif // DRAG_SUPPORT

#ifdef USE_OP_CLIPBOARD
class OpDragObject;
class DOM_DataTransfer;

class DOM_ClipboardEvent
	: public DOM_Event
{
public:
	DOM_ClipboardEvent()
		: m_clipboard_data(NULL)
		, m_id(0)
	{
	}

	static OP_STATUS Make(DOM_Event *&event, DOM_Runtime *runtime, unsigned int id);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CLIPBOARDEVENT || DOM_Event::IsA(type); }
	virtual void GCTrace();
	virtual OP_STATUS DefaultAction(BOOL cancelled);

	void SetData(DOM_DataTransfer *data) { m_clipboard_data = data; }
	DOM_DataTransfer *GetData() { return m_clipboard_data; }

	DOM_DECLARE_FUNCTION(initClipboardEvent);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };

private:
	DOM_DataTransfer *m_clipboard_data;
	unsigned int m_id;
};

class DOM_ClipboardEvent_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_ClipboardEvent_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::CLIPBOARDEVENT_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);
	static void AddConstructorL(DOM_Object* object);
};
#endif // USE_OP_CLIPBOARD

#endif // DOM_EVENT_H
