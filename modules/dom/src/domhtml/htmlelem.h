/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_HTMLELEM_H
#define DOM_HTMLELEM_H

#include "modules/dom/src/domcore/element.h"

#include "modules/logdoc/html.h"
#include "modules/dom/src/domcore/domtokenlist.h"

class DOM_Collection;
class DOM_HTMLOptionsCollection;
class JS_Window;
class DOM_LocalMediaStream;

#ifdef JS_PLUGIN_SUPPORT
class JS_Plugin_Object;
#endif // JS_PLUGIN_SUPPORT

/* This needs to be declared here since it is used in a selftest. */
class ConstructHTMLElementData
{
public:
	unsigned element_type:16;
	unsigned prototype:8;
	unsigned element_class:8;
};
extern const ConstructHTMLElementData g_DOM_construct_HTMLElement_data[];

/** The DOM element of an HTML element.
 * All HTML elements are represented as DOM elements by an
 * object of this kind.  It is not subclassed, all different
 * HTML elements are handled by switch statements inside this
 * specific class.  At least, that's the way it looks to me (eirik).
 */
class DOM_HTMLElement
  : public DOM_Element
{
protected:
	DOM_HTMLElement() : properties(NULL) {}

public:
    enum
    {
        NOWHERE,
        BEFORE_BEGIN,
        AFTER_BEGIN,
        BEFORE_END,
        AFTER_END
    };

	/** The kinds of numeric properties supported. Used to direct the conversion
	    between attribute values and properties. */
	enum
	{
		TYPE_LONG = 0,
		TYPE_LONG_NON_NEGATIVE,
		TYPE_ULONG,
		TYPE_ULONG_NON_ZERO,
		TYPE_DOUBLE
	};

	enum
	{
		NUMERIC_ATTRIBUTE_MAX = 0x7fffffff
		/**< The maximum allowed value of a long (signed or unsigned)
		     attribute value. */
	};

	class SimpleProperties
	{
	public:
		const unsigned *string;
		const OpAtom *boolean;
		const unsigned *numeric;
		/**< Numeric properties are encoded as follows:

		     (OpAtom << 4 | numeric_type << 1 | has_default), default_value?

		     the property atom along with its type. An optional default value
		     may follow. */
	};

	static OP_STATUS Make(DOM_HTMLElement *&element, HTML_Element *html_element, DOM_EnvironmentImpl *environment);
	static void PutConstructorsL(JS_Window *target);

	static void InitializeConstructorsTableL(OpString8HashTable<DOM_ConstructorInformation> *table);

	static ES_Object *CreateConstructorL(DOM_Runtime *runtime, DOM_Object *target, unsigned id);

	void SetProperties(const SimpleProperties *new_properties) { OP_ASSERT(!properties); properties = new_properties; }

	ES_GetState GetStringAttribute(OpAtom property_name, ES_Value *value);
	ES_GetState GetNumericAttribute(OpAtom property_name, const unsigned *type, ES_Value *value);
	ES_GetState GetBooleanAttribute(OpAtom property_name, ES_Value *value);

	ES_PutState SetStringAttribute(OpAtom property_name, BOOL treat_null_as_empty_string, ES_Value *value, ES_Runtime *origining_runtime);
	ES_PutState SetNumericAttribute(OpAtom property_name, const unsigned *type, ES_Value *value, ES_Runtime *origining_runtime);
	ES_PutState SetBooleanAttribute(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState GetName(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutNameRestart(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_PutState PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_ELEMENT || DOM_Element::IsA(type); }

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);
	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);
	static void ConstructHTMLElementPrototypeL(ES_Object *object, DOM_Runtime *runtime);

    ES_PutState ParseAndReplace(OpAtom prop, ES_Value* value, DOM_Runtime *origining_runtime, int where = NOWHERE, DOM_Object *restarted = NULL);
    int InsertAdjacent(OpAtom prop, DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime);

#ifdef DOM2_MUTATION_EVENTS
	OP_STATUS SendAttrModified(ES_Thread *interrupt_thread, OpAtom property_name, const uni_char *prevValue, const uni_char *newValue);
#endif /* DOM2_MUTATION_EVENTS */

	OP_STATUS GetChildElement(DOM_HTMLElement *&element, HTML_ElementType type, unsigned index = 0);
	ES_PutState PutChildElement(OpAtom property_name, ES_Value* value, DOM_Runtime* origining_runtime, ES_Object *restart_object);

	static OP_STATUS CreateElement(DOM_HTMLElement *&element, DOM_Node *reference, const uni_char *name);

	enum FrameEnvironmentType { FRAME_DOCUMENT, FRAME_WINDOW };
	ES_GetState GetFrameEnvironment(ES_Value *value, FrameEnvironmentType type, DOM_Runtime *origining_runtime);

	/** @return property which itemValue mimics for this element. */
	static OpAtom GetItemValueProperty(HTML_Element* this_element);

	/** Called before a click is dispatched on a html element (type checked by function) */
	static OP_STATUS BeforeClick(DOM_Object* object);

	static const unsigned *GetTabIndexTypeDescriptor();

	DOM_DECLARE_FUNCTION(focus);
	DOM_DECLARE_FUNCTION(blur);
	DOM_DECLARE_FUNCTION(click);
	DOM_DECLARE_FUNCTION(insertAdjacentElement);
	DOM_DECLARE_FUNCTION(insertAdjacentHTML);
	DOM_DECLARE_FUNCTION(insertAdjacentText);
	enum {
		FUNCTIONS_START,
		FUNCTIONS_focus,
		FUNCTIONS_blur,
		FUNCTIONS_click,
		FUNCTIONS_insertAdjacentElement,
		FUNCTIONS_insertAdjacentHTML,
		FUNCTIONS_insertAdjacentText,
		FUNCTIONS_ARRAY_SIZE
	};

protected:
	const SimpleProperties *properties;
};

/* Convenience macros for working with numeric property descriptors */
#define HTML_NUMERIC_PROPERTY_ATOM(t) (static_cast<OpAtom>(t >> 4))
#define HTML_NUMERIC_PROPERTY_TYPE(t) ((t[0] >> 1) & 0x7)
#define HTML_NUMERIC_PROPERTY_HAS_DEFAULT(t) (((t)[0] & 0x1) != 0)
#define HTML_NUMERIC_PROPERTY_DEFAULT(t) (static_cast<int>(t[1]))
#define HTML_NUMERIC_PROPERTY_SKIP_DEFAULT(idx, t) if (HTML_NUMERIC_PROPERTY_HAS_DEFAULT(&t)) idx++

#define HTML_STRING_PROPERTY_ATOM(t) static_cast<OpAtom>((t) & 0x7fffffff)
#define HTML_STRING_PROPERTY_ATOM_NULL_AS_EMPTY(t) (((t) & 0x80000000) != 0)

class DOM_ValidityState;

/**
 * Convenience base class for all forms elements (INPUT, SELECT, OPTION, TEXTAREA and ISINDEX).
 */
class DOM_HTMLFormsElement
  : public DOM_HTMLElement
{
protected:
	DOM_Collection *labels;
	DOM_ValidityState *validity;

public:
	DOM_HTMLFormsElement() : labels(NULL), validity(NULL) {}

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

	DOM_DECLARE_FUNCTION(setCustomValidity);
	DOM_DECLARE_FUNCTION(checkValidity);

#ifdef DOM_SELECTION_SUPPORT
	DOM_DECLARE_FUNCTION(select);
	DOM_DECLARE_FUNCTION(setSelectionRange);

	OP_STATUS SetSelectionRange(ES_Runtime* origining_runtime, int start, int end, SELECTION_DIRECTION direction);
	/** Sets a new selection on the element and fires the ONSELECT event.

		@param origining_runtime The script's origining runtime.
		@param start The offset of the first selected character.
		@param end The offset of the character after the last selected.
		@param direction The direction of the new selection.
		@return OpStatus::OK if setting the new selection succeeded. */
#endif // DOM_SELECTION_SUPPORT

	static OP_STATUS InitLabelsCollection(DOM_HTMLElement *element, DOM_Collection *&labels);
	static OP_STATUS InitValidityState(DOM_HTMLElement *element, DOM_ValidityState *&validity);

protected:
	ES_GetState GetFormValue(ES_Value *value, BOOL with_crlf = TRUE);
	ES_PutState SetFormValue(const uni_char *value);
	HTML_Element *GetFormElement();

	DOM_ValidityState *GetValidityState() { return validity; }

#ifdef DOM_SELECTION_SUPPORT
	static BOOL InputElementHasSelectionAttributes(HTML_Element *element);
#endif // DOM_SELECTION_SUPPORT
};

class DOM_HTMLElement_Prototype
{
private:
	static void ConstructL(ES_Object *prototype, DOM_Runtime::HTMLElementPrototype type, DOM_Runtime *runtime);

public:
	static OP_STATUS Construct(ES_Object *prototype, DOM_Runtime::HTMLElementPrototype type, DOM_Runtime *runtime);
};

class DOM_HTMLOptionElement_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_HTMLOptionElement_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::OPTION_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

class DOM_HTMLImageElement_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_HTMLImageElement_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::IMAGE_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

/* The classes below are in the order listed in idl definition in the appendix of
   the DOM2 HTML spec.   Please keep them in that order if possible.
*/

class DOM_HTMLHtmlElement : public DOM_HTMLElement
{
public:
};

class DOM_HTMLTitleElement : public DOM_HTMLElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
};

class DOM_HTMLBodyElement : public DOM_HTMLElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
};

class DOM_HTMLStylesheetElement : public DOM_HTMLElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

#ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
class DOM_HTMLXmlElement : public DOM_HTMLElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};
#endif // #ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692

class DOM_HTMLFormElement : public DOM_HTMLElement
{
protected:
	DOM_Collection *elements;
    DOM_Collection *images;

public:
	DOM_HTMLFormElement();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_FORMELEMENT || DOM_HTMLElement::IsA(type); }

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

	OP_STATUS InitElementsCollection();
	OP_STATUS InitImagesCollection();
	DOM_Collection *GetElementsCollection() const { return elements; }

	static ES_GetState GetEnumeratedValue(OpAtom property_name, ES_GetState state, ES_Value* value);
	/**< Translate an attribute's string value into the enumerated values
	     that the property can only hold. And if empty, map it to the default
	     value. Used to handle the enumerated attributes of forms and its elements.

	     @param property_name The property with an enumerated value.
	     @param state The result from fetching the underlying string attribute.
	     @param value If state is GET_SUCCESS, the string value of the attribute.
	                  Can be NULL if called during property enumeration.
	     @return The return value is identical to 'state', but if GET_SUCCESS, then
	             'value' is constrained to the allowed enumerated values for
	             the property. */

	DOM_DECLARE_FUNCTION(reset);
	DOM_DECLARE_FUNCTION(submit);
	DOM_DECLARE_FUNCTION(item);
	DOM_DECLARE_FUNCTION(namedItem);
	DOM_DECLARE_FUNCTION(checkValidity);
	DOM_DECLARE_FUNCTION(resetFromData);
	DOM_DECLARE_FUNCTION_WITH_DATA(dispatchFormInputOrFormChange);

	enum DispatchType
	{
		DISPATCH_FORM_INPUT,
		DISPATCH_FORM_CHANGE
	};
};

class DOM_HTMLSelectElement : public DOM_HTMLFormsElement
{
protected:
	DOM_HTMLOptionsCollection *options;
	DOM_Collection *selected_options;

public:
	DOM_HTMLSelectElement();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_SELECTELEMENT || DOM_HTMLElement::IsA(type); }

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);
	virtual void GCTraceSpecial(BOOL via_tree);

	OP_STATUS InitOptionsCollection();
	DOM_HTMLOptionsCollection *GetOptionsCollection() { return options; }

	OP_STATUS InitSelectedOptionsCollection();
	DOM_Collection *GetSelectedOptionsCollection() { return selected_options; };

	virtual ES_PutState PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_PutState PutIndexRestart(int property_index, ES_Value* value, ES_Runtime *origining_runtime, ES_Object* restart_object);

	static int ChangeOptions(DOM_HTMLSelectElement *select, BOOL add, BOOL remove, BOOL modify_length, DOM_Object *option, DOM_Object *reference, int index, ES_Value *exception_value, DOM_Runtime *origining_runtime, ES_Object *&restart_object);
	static int ChangeOptionsRestart(ES_Value *exception_value, ES_Object *&restart_object);

	DOM_DECLARE_FUNCTION_WITH_DATA(addOrRemove);
	DOM_DECLARE_FUNCTION(item);
	DOM_DECLARE_FUNCTION(namedItem);
};

class SelectionObject;

class DOM_HTMLOptionElement : public DOM_HTMLFormsElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_OPTIONELEMENT || DOM_HTMLFormsElement::IsA(type); }

	HTML_Element *GetSelectElement();
	int GetOptionIndex();
};

class DOM_HTMLOptGroupElement : public DOM_HTMLFormsElement
{
public:
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_OPTGROUPELEMENT || DOM_HTMLFormsElement::IsA(type); }
};

class DOM_HTMLInputElement : public DOM_HTMLFormsElement
{
private:
	ES_GetState GetDataListForInput(ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetNativeValueForInput(OpAtom property_name, ES_Value* value, DOM_Runtime* origining_runtime);
	ES_PutState PutNativeValueForInput(OpAtom property_name, ES_Value* value, DOM_Runtime* origining_runtime);

public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_INPUTELEMENT || DOM_HTMLFormsElement::IsA(type); }

	enum StepUpDownType
	{
		STEP_UP,
		STEP_DOWN
	};

	DOM_DECLARE_FUNCTION_WITH_DATA(stepUpDown);
};

class DOM_HTMLTextAreaElement : public DOM_HTMLFormsElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
};

class DOM_HTMLButtonElement : public DOM_HTMLFormsElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

class DOM_HTMLLabelElement : public DOM_HTMLFormsElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

protected:
	ES_GetState GetControlForLabel(ES_Value* value, ES_Runtime* origining_runtime);
};

/**
	Common properties for anchor & area. Properties defined in JavaScript 1.3:
    http://10.20.20.32/library/ecma_and_javascript/html/js_1.3_client_ref/link.htm#1193376
*/
class DOM_HTMLJSLinkElement : public DOM_HTMLElement
{
	static inline BOOL IsValidDomainChar(uni_char c){return uni_isalnum(c)||c=='.'||c=='-';}
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

class DOM_HTMLAnchorElement : public DOM_HTMLJSLinkElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

class DOM_HTMLImageElement : public DOM_HTMLElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

class DOM_HTMLMarqueeElement
{
public:
	DOM_DECLARE_FUNCTION_WITH_DATA(startOrStop);
};

class DOM_HTMLPluginElement : public DOM_HTMLElement
{
#ifdef _PLUGIN_SUPPORT_
private:
	BOOL		is_connected;

	/**
	 * Tries to get the "scriptable" from the plugin if available. Returns it if it's there,
	 * returns nothing if it's clearly not there. If it might be there really soon now, and
	 * allow_suspend is TRUE, suspends the script. In that case suspended_object is set to
	 * a restart_object. If this is called in a restarted function, is_resumed_from_connection_wait
	 * will say if the function was restarted because of a suspended ConnectToPlugin. Otherwise
	 * it was suspended somewhere else (maybe because of setting innerHTML.
	 */
	OP_STATUS	ConnectToPlugin(ES_Runtime *origining_runtime, BOOL allow_suspend, ES_Object*& suspended_object, BOOL& is_resumed_from_connection_wait, ES_Object *restart_object = NULL);

protected:
	OP_STATUS GetScriptableObject(EcmaScript_Object** scriptable);
#endif // _PLUGIN_SUPPORT_

public:
#ifdef _PLUGIN_SUPPORT_
	            DOM_HTMLPluginElement();
#endif // _PLUGIN_SUPPORT_
	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_PLUGINELEMENT || DOM_HTMLElement::IsA(type); }

#ifdef SVG_DOM
	DOM_DECLARE_FUNCTION(getSVGDocument);
#endif // SVG_DOM
};

class DOM_HTMLObjectElement : public DOM_HTMLPluginElement
{
protected:
	DOM_ValidityState *validity;

#ifdef JS_PLUGIN_SUPPORT
	ES_Object *jsplugin_object;
#endif // JS_PLUGIN_SUPPORT

	ES_GetState GetContentDocument(ES_Value* value);

public:
	DOM_HTMLObjectElement();
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

#if defined JS_PLUGIN_SUPPORT

	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);
	virtual void GCTraceSpecial(BOOL via_tree);
#endif // JS_PLUGIN_SUPPORT

#ifdef JS_PLUGIN_SUPPORT
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);

	void SetJSPluginObject(ES_Object *obj) { jsplugin_object = obj; }
	ES_Object *GetJSPluginObject() { return jsplugin_object; }
#endif // JS_PLUGIN_SUPPORT

private:
	DOM_ValidityState *GetValidityState() { return validity; }
};

#ifdef MEDIA_HTML_SUPPORT
class DOM_HTMLMediaElement : public DOM_HTMLElement, public Link
{
public:
	DOM_HTMLMediaElement() :
		m_error(NULL), m_tracks(NULL)
	{}
	virtual ~DOM_HTMLMediaElement();

	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_MEDIAELEMENT || DOM_HTMLElement::IsA(type); }
	virtual void GCTraceSpecial(BOOL via_tree);
	void GCMarkPlaying();

	static void ConstructHTMLMediaElementObjectL(ES_Object *object, DOM_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	DOM_DECLARE_FUNCTION(addTextTrack);
	DOM_DECLARE_FUNCTION(canPlayType);
	DOM_DECLARE_FUNCTION_WITH_DATA(load_play_pause);

private:

	/**
	 * Getter for 'TimeRanges' properties.
	 *
	 * @param[in] OP_ATOM_buffered, OP_ATOM_played or OP_ATOM_seekable.
	 * @param[in] media The MediaElement to retrieve the ranges from.
	 * @param[out] value The new TimeRanges object is stored here on success.
	 */
	ES_GetState GetTimeRanges(OpAtom property_name, MediaElement *media, ES_Value *value);

	OP_STATUS CreateTrackList(MediaElement* media);

	class DOM_MediaError* m_error;
	class DOM_TextTrackList* m_tracks;

	OpString m_current_src;
};

class DOM_HTMLAudioElement : public DOM_HTMLMediaElement
{
public:
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_AUDIOELEMENT || DOM_HTMLMediaElement::IsA(type); }
};

// This is for the Audio() constructor
class DOM_HTMLAudioElement_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_HTMLAudioElement_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::AUDIO_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

class DOM_HTMLVideoElement : public DOM_HTMLMediaElement
{
public:
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_VIDEOELEMENT || DOM_HTMLMediaElement::IsA(type); }

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
#ifdef DOM_STREAM_API_SUPPORT
private:
	static DOM_LocalMediaStream* GetDOMLocalMediaStream(ES_Value* value);
#endif // DOM_STREAM_API_SUPPORT
};

class DOM_HTMLTrackElement : public DOM_HTMLElement
{
public:
	DOM_HTMLTrackElement() : m_track(NULL) {}

	virtual void GCTraceSpecial(BOOL via_tree);

	static void ConstructHTMLTrackElementObjectL(ES_Object *object, DOM_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

private:
	class DOM_TextTrack* m_track;
};
#endif // MEDIA_HTML_SUPPORT

#ifdef CANVAS_SUPPORT

class DOM_HTMLCanvasElement : public DOM_HTMLElement
{
public:
	DOM_HTMLCanvasElement()
		: m_pending_invalidation(FALSE),
		  m_dirty(FALSE),
		  m_context2d(NULL)
#ifdef CANVAS3D_SUPPORT
		, m_contextwebgl(NULL)
#endif // CANVAS3D_SUPPORT
	{
	}

	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_CANVASELEMENT || DOM_HTMLElement::IsA(type); }
	virtual void GCTraceSpecial(BOOL via_tree);
#ifdef CANVAS3D_SUPPORT
	virtual ES_GetState GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
#endif //CANVAS3D_SUPPORT

	void MakeInsecure();

	OP_STATUS ScheduleInvalidation(ES_Runtime* origining_runtime);
	void Invalidate(BOOL threadFinished = FALSE);

	void RecordAllocationCost();

	DOM_DECLARE_FUNCTION(getContext);
	DOM_DECLARE_FUNCTION(toDataURL);
private:
	BOOL m_pending_invalidation;
	BOOL m_dirty;

	class DOMCanvasContext2D* m_context2d;
#ifdef CANVAS3D_SUPPORT
	class DOMCanvasContextWebGL* m_contextwebgl;
#endif // CANVAS3D_SUPPORT
};

#endif // CANVAS_SUPPORT

class DOM_HTMLMapElement : public DOM_HTMLElement
{
protected:
	DOM_Collection *areas;

public:
	DOM_HTMLMapElement() : areas(NULL) {}

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

    OP_STATUS InitAreaCollection();
	DOM_Collection *GetAreaCollection() { return areas; }
};

class DOM_HTMLScriptElement : public DOM_HTMLElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

class DOM_HTMLTableElement : public DOM_HTMLElement
{
protected:
	DOM_Collection *rows;
	DOM_Collection *cells;
	DOM_Collection *tBodies;

public:
	DOM_HTMLTableElement() : rows(NULL), cells(NULL), tBodies(NULL) {}

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_TABLEELEMENT || DOM_HTMLElement::IsA(type); }

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

    OP_STATUS InitRowsCollection();
	DOM_Collection *GetRowsCollection() { return rows; }
    OP_STATUS InitCellsCollection();
	DOM_Collection *GetCellsCollection() { return cells; }
    OP_STATUS InitTBodiesCollection();
	DOM_Collection *GetTBodiesCollection() { return tBodies; }

	DOM_DECLARE_FUNCTION_WITH_DATA(createTableItem);
	DOM_DECLARE_FUNCTION_WITH_DATA(deleteTableItem);
	DOM_DECLARE_FUNCTION(insertRow);
	DOM_DECLARE_FUNCTION(deleteRow);
};

class DOM_HTMLTableSectionElement : public DOM_HTMLElement
{
protected:
	DOM_Collection *rows;

public:
	DOM_HTMLTableSectionElement() : rows(NULL) {}

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_TABLESECTIONELEMENT || DOM_HTMLElement::IsA(type); }

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

    OP_STATUS InitRowsCollection();
	DOM_Collection *GetRowsCollection() { return rows; }

	DOM_DECLARE_FUNCTION(insertRow);
	DOM_DECLARE_FUNCTION(deleteRow);
};

class DOM_HTMLTableRowElement : public DOM_HTMLElement
{
protected:
	DOM_Collection *cells;

public:
	DOM_HTMLTableRowElement() : cells(NULL) {}

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_TABLEROWELEMENT || DOM_HTMLElement::IsA(type); }

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

	OP_STATUS InitCellsCollection();
	DOM_Collection *GetCellsCollection() { return cells; }
	OP_STATUS GetTable(DOM_HTMLTableElement *&table);
	OP_STATUS GetSection(DOM_HTMLTableSectionElement *&section);
	OP_STATUS GetRowIndex(int &result, BOOL in_section);

	DOM_DECLARE_FUNCTION(insertCell);
	DOM_DECLARE_FUNCTION(deleteCell);
};

class DOM_HTMLTableCellElement : public DOM_HTMLElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

    OP_STATUS GetCellIndex(int &i);
};

class DOM_HTMLFrameElement : public DOM_HTMLElement
{
public:
	virtual BOOL SecurityCheck(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_FRAMEELEMENT || DOM_HTMLElement::IsA(type); }

    ES_GetState GetContentDocument(ES_Value* value);
};

class DOM_HTMLOutputElement : public DOM_HTMLFormsElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
};

class DOM_HTMLFieldsetElement : public DOM_HTMLFormsElement
{
public:
	DOM_HTMLFieldsetElement() : elements(NULL) {}

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

private:
	OP_STATUS InitElementsCollection();

	DOM_Collection *elements;
};

class DOM_HTMLDataListElement : public DOM_HTMLElement
{
private:
	DOM_HTMLOptionsCollection *options;
	OP_STATUS InitOptionsCollection();

public:
	DOM_HTMLDataListElement() : options(NULL) {}
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);
};

class DOM_HTMLKeygenElement : public DOM_HTMLFormsElement
{
private:

public:
	DOM_HTMLKeygenElement() {}
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

class DOM_HTMLProgressElement : public DOM_HTMLElement
{
private:
	DOM_Collection *labels;
public:
	DOM_HTMLProgressElement() : labels(NULL) {}

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

class DOM_HTMLMeterElement : public DOM_HTMLElement
{
private:
	DOM_Collection *labels;
public:
	DOM_HTMLMeterElement() : labels(NULL) {}

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

class DOM_HTMLTimeElement : public DOM_HTMLElement
{
public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

#endif // DOM_HTMLELEM_H
