/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_TIME_VALUE_H
#define SVG_TIME_VALUE_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGObject.h"

#include "modules/svg/src/animation/svganimationtime.h"
#include "modules/svg/src/animation/svganimationinstancelist.h"

#include "modules/dom/domeventtypes.h"
#include "modules/dom/domevents.h"

class SVGSyncbaseTimeValue;

enum SVGSyncbaseEvent
{
    SVGSYNCBASE_BEGIN = 0,
    SVGSYNCBASE_END
};

enum SVGSyncbaseSetupType
{
    SVGSYNCBASESETUP_CREATED = 0,
    SVGSYNCBASESETUP_CHANGED
};

enum SVGTimeType
{
  	SVGTIME_OFFSET,
  	SVGTIME_EVENT,
	SVGTIME_SYNCBASE,
  	SVGTIME_WALLCLOCK, // not well supported yet
  	SVGTIME_INDEFINITE,
	SVGTIME_REPEAT,
	SVGTIME_ACCESSKEY,
  	SVGTIME_UNKNOWN
};

class SVGAnimationInstanceList;

class SVGTimeObject;
class SVGDocumentContext;
class SVGTimingInterface;

#ifdef XML_EVENTS_SUPPORT
class SVGTimeEventHandler : public DOM_EventsAPI::EventHandler
{
public:
	SVGTimeEventHandler(SVGTimeObject* time_value);
	virtual ~SVGTimeEventHandler();
	virtual BOOL HandlesEvent(DOM_EventType known_type,
							  const uni_char *type,
							  ES_EventPhase phase);

	virtual OP_STATUS HandleEvent(DOM_EventsAPI::Event *event);

	virtual void RegisterHandlers(DOM_Environment *environment);

	virtual void UnregisterHandlers(DOM_Environment *environment);
private:
	SVGTimeObject* m_time_value;
};
#endif

/**
 * A Time value class
 */
class SVGTimeObject : public SVGObject
{
public:
	/**
	 * Construct a time value with a type. Used by the constructors of
	 * the subclasses.
	 */
	SVGTimeObject(SVGTimeType type);

	/**
	 * Virtual destructor
	 */
	virtual ~SVGTimeObject();

	/**
	 * Get instances from time value.
	 *
	 * @param instance_list The instances are added to this list
	 * @param allow_indefinite Should indefinite instances be allowed
	 *
	 */
	OP_STATUS GetInstanceTimes(SVGAnimationInstanceList& instance_list, BOOL allow_indefinite);

    /**
	 * Get the type of the time value
	 */
	SVGTimeType TimeType() { return m_time_type; }
	OP_STATUS SetElementID(const uni_char* element_id, int strlen);
	void SetOffset(SVG_ANIMATION_TIME animation_time) { m_offset = animation_time; }
	OP_STATUS SetEventName(const uni_char* event_name, unsigned event_length,
						   const uni_char* prefix_name, unsigned prefix_length);
	void SetSyncbaseType(SVGSyncbaseEvent syncbase_type)
		{ m_event = syncbase_type; }
	void SetRepetition(UINT32 repetition)
		{ m_repetition = repetition; }
	void SetAccessKey(const uni_char c) { m_access_key = c; }
	void SetAccessKey(const uni_char* str, int strlen)
		{ if (str != NULL && strlen > 0) m_access_key = str[0]; }
	void Reset() { m_activation_times.Clear(); }

	OP_STATUS AddInstanceTime(SVG_ANIMATION_TIME activation_time);

	OP_STATUS RegisterTimeValue(SVGDocumentContext* doc_ctx, HTML_Element* target_elm);
	OP_STATUS UnregisterTimeValue(SVGDocumentContext* doc_ctx, HTML_Element* target_elm);

	const uni_char *GetEventName() { return m_event_name; }
	DOM_EventType GetEventType();
	const uni_char* GetEventNS();
	SVG_ANIMATION_TIME GetOffset() { return m_offset; }
	uni_char GetAccessKey() { return m_access_key; }
	uni_char* GetElementId() { return m_element_id; }
	UINT32 GetRepetition() { return m_repetition; }
	SVGTimingInterface* GetNotifier() { return m_notifier; }
	void SetNotifier(SVGTimingInterface* timing_if) { m_notifier = timing_if; }
    SVGSyncbaseEvent GetSyncbaseEvent() { return m_event; }

	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;
	virtual SVGObject *Clone() const;
	virtual BOOL IsEqual(const SVGObject &other) const;

protected:
	/**
	 * The type of time value
	 */
	SVGTimeType m_time_type;

	/**
	 * Offset. Used in types offset, event, syncbase and repeat. Not used in indefinite
	 */
	SVG_ANIMATION_TIME m_offset;

#ifdef XML_EVENTS_SUPPORT
	DOM_EventsAPI::EventHandler *m_event_handler;
	DOM_EventsAPI::EventTarget  *m_event_target;
#endif

	/**
	 * Element id to the element that the event relates to. Used in event, sync, repeat.
	 */
    uni_char* m_element_id;

	uni_char* m_event_name;

	uni_char* m_event_prefix;

	/**
	 * Activation times.
	 */
	SVGAnimationInstanceList m_activation_times;

	/**
	 * Our owner.
	 */
	SVGTimingInterface* m_notifier;

	/**
	 * Access key. Used by event.
	 */
	uni_char m_access_key;

    /**
     * Tell which we should refer to, begin or end, of the interval that the
     * element creates. Used by syncbase.
     */
    SVGSyncbaseEvent m_event;

    /**
     * Tell which repetition we should refer to of the interval that
     * the element creates. Used by repetition.
     */
    UINT32 m_repetition;

};

#endif // SVG_SUPPORT
#endif // SVG_TIME_VALUE_H
