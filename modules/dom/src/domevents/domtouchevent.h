/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_TOUCHEVENT_H
#define DOM_TOUCHEVENT_H

#ifdef TOUCH_EVENTS_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/util/adt/opvector.h"

/*
 * Touch, Safari DOM extension.
 *
 * Properties: clientX, clientY, identifier, pageX, pageY, screenX, screenY, target.
 *
 * See <URL: http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariJSRef/Touch/Touch.html>.
 */
class DOM_Touch
	: public DOM_Object
{
protected:
	DOM_Touch();

	friend class DOM_TouchEvent;

	int client_x, client_y;
	int page_x, page_y;
	int screen_x, screen_y;
	int identifier;
	DOM_Object* target;
	int radius;

	/** The touchstart event introducing this touch was prevented. */
	BOOL prevented;

	/** This touch has never left the comforts of its immediate origin. */
	BOOL tap;

	/* Coordinates of the touchstart event introducing this touch. */
	int start_offset_x, start_offset_y;
	int start_client_x, start_client_y;

public:

	/**
	 * Allocate a new DOM_Touch object, register it with the DOM runtime and initialize it.
	 *
	 * If client coordinates are (-1, -1) and page coordinates are not, client coordinates will be
	 * computed from page coordinates, and vice versa.
	 *
	 * @param[out] touch Pointer to touch object, pointer will be initialized by this call.
	 * @param[in] target The HTML element that is the target of the touch action.
	 * @param[in] identifier Numeric identifier used to separate concurrent touch objects.
	 * @param[in] client_x,client_y Coordinates relative to the DOM implementation's client area.
	 * @param[in] offset_x,offset_y Coordinates relative to the target element's upper left corner.
	 * @param[in] page_x,page_y Coordinates relative to the document.
	 * @param[in] screen_x,screen_y Coordinates relative to the origin of the screen coordinate system.
	 * @param[in] radius The tap activation radius.
	 * @param[in] runtime DOM runtime.
	 *
	 * @return OpStatus::OK on success.
	 */
	static OP_STATUS Make(DOM_Touch *&touch, DOM_Object *target, int identifier, int client_x, int client_y,
						  int offset_x, int offset_y, int page_x, int page_y, int screen_x, int screen_y, int radius, DOM_Runtime *runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_TOUCH || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	DOM_Object *GetTarget() { return target; }
	HTML_Element *GetTargetElement();
	int GetIdentifier() { return identifier; }
	BOOL GetWithinTapRadius() { return tap; }

	void UpdateTapStatus(BOOL had_tap);
	/**< Update the object's is-tap-local flag based on previous setting for the
	     same touch. */

	DOM_DECLARE_FUNCTION(createTouch);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

/*
 * TouchList, Safari DOM extension.
 *
 * Properties: length.
 * Methods: item.
 *
 * See <URL: http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariJSRef/TouchList/TouchList.html>.
 */
class DOM_TouchList
	: public DOM_Object
{
protected:
	OpVector<DOM_Touch> touches;

public:
	static OP_STATUS Make(DOM_TouchList *&list, DOM_Runtime *runtime);

	virtual ~DOM_TouchList()
	{
	}

	virtual BOOL IsA(int type) { return type == DOM_TYPE_TOUCHLIST || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState	PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	DOM_Touch *Get(unsigned int index) { return touches.Get(index); }
	unsigned GetCount() { return touches.GetCount(); }
	OP_STATUS Add(DOM_Touch *touch) { return touches.Add(touch); }
	OP_STATUS Replace(unsigned i, DOM_Touch *touch) { return touches.Replace(i, touch); }
	OP_STATUS RemoveByItem(DOM_Touch *touch) { return touches.RemoveByItem(touch); }
	void *Remove(unsigned int index) { return touches.Remove(index, 1); }
	void Clear() { touches.Clear(); }

	DOM_DECLARE_FUNCTION(getItem);
	DOM_DECLARE_FUNCTION(getIdentifier);
	DOM_DECLARE_FUNCTION(createTouchList);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };
};

/*
 * TouchEvent, Safari DOM extension.
 *
 * Properties: altKey, ctrlKey, metaKey, shiftKey, touches, targetTouches, changedTouches, scale, rotation.
 * Methods: initTouchEvent.
 *
 * See <URL: http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariJSRef/TouchEvent/TouchEvent.html>.
 */
class DOM_TouchEvent
	: public DOM_UIEvent
{
protected:
	BOOL alt_key, ctrl_key, meta_key, shift_key;
	int offset_x, offset_y;
	DOM_TouchList *touches;
	DOM_TouchList *target_touches;
	DOM_TouchList *changed_touches;
	double scale, rotation;

	void* user_data;

public:
	DOM_TouchEvent();
	virtual ~DOM_TouchEvent();

	virtual BOOL IsA(int type) { return type == DOM_TYPE_TOUCHEVENT || DOM_UIEvent::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual OP_STATUS DefaultAction(BOOL cancelled);

	/**
	 * Initialize an existing touch event by updating the information in the touch object identified
	 * by identifier. In case of TOUCHSTART, a touch object with the given identfier must not already
	 * exist but for all other event types exactly one touch object with the given identifier must exist.
	 *
	 * May be called multiple times to update multiple touch objects, but target must remain identical.
	 *
	 * @param[in] shift_key TRUE if shift-key is pressed.
	 * @param[in] ctrl_key TRUE if ctrl-key is pressed.
	 * @param[in] alt_key TRUE if alt-key is pressed.
	 * @param[in] identifier Numeric ID of touch object requiring update.
	 * @param[in] target DOM object representing the targeted HTML element.
	 * @param[in] type DOM event type.
	 * @param[in] radius The radius of the touch, given in document units.
	 * @param[in] active_touches List of all touches active on current document.
	 * @param[in] runtime Origining DOM runtime.
	 *
	 * @return OpStatus::OK or OpStatus::OOM.
	 */
	OP_STATUS InitTouchEvent(int screen_x, int screen_y, int client_x, int client_y, int offset_x, int offset_y,
	                         ShiftKeyState modifiers, int identifier, DOM_Object *target, DOM_EventType type,
	                         int radius, DOM_TouchList *active_touches, DOM_Runtime *runtime, void *user_data = NULL);

	DOM_DECLARE_FUNCTION(initTouchEvent);
	enum {
		FUNCTIONS_initTouchEvent = 1,
		FUNCTIONS_ARRAY_SIZE
	};
};

#endif // TOUCH_EVENTS_SUPPORT
#endif // !DOM_TOUCHEVENT_H
