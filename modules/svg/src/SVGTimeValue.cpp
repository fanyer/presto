/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGTimeValue.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/AttrValueStore.h"

#include "modules/svg/src/animation/svganimationworkplace.h"
#include "modules/svg/src/animation/svganimationinstancelist.h"

#include "modules/xmlutils/xmlnames.h"

#include "modules/svg/src/SVGTimedElementContext.h"

#include "modules/dom/domenvironment.h"

#ifdef XML_EVENTS_SUPPORT

SVGTimeEventHandler::SVGTimeEventHandler(SVGTimeObject* time_value) :
	m_time_value(time_value)
{
	OP_ASSERT(m_time_value && m_time_value->TimeType() == SVGTIME_EVENT);
	SVGObject::IncRef(m_time_value);
}

/* virtual */
SVGTimeEventHandler::~SVGTimeEventHandler()
{
	SVGObject::DecRef(m_time_value);
}

/* virtual */ BOOL
SVGTimeEventHandler::HandlesEvent(DOM_EventType known_type,
								  const uni_char *type,
								  ES_EventPhase phase)
{
	BOOL handles_phase =
		(phase == ES_PHASE_AT_TARGET || // - We are the direct
										//   receiver of this event.
		 phase == ES_PHASE_ANY ||		// - This means that we
										//   listens to one or more
										//   events at all.
		 phase == ES_PHASE_BUBBLING);   // - We are the receiver of
										//   the event during the
										//   bubbeling phase.
	if (!handles_phase)
		return FALSE;

	if (type)
	{
		const uni_char *event_name = m_time_value->GetEventName();
		if (event_name)
		{
			return uni_str_eq(type, event_name);
		}

		// The custom event was not a match
		return FALSE;
	}
	else
	{
		return (m_time_value->GetEventType() == known_type);
	}
}

/* virtual */ OP_STATUS
SVGTimeEventHandler::HandleEvent(DOM_EventsAPI::Event *event)
{
	SVGTimingInterface *timed_element_ctx = m_time_value->GetNotifier();
	if (timed_element_ctx)
	{
		SVGDocumentContext *doc_ctx = AttrValueStore::GetSVGDocumentContext(timed_element_ctx->GetElement());
		SVGAnimationWorkplace *animation_workplace = doc_ctx ? doc_ctx->GetAnimationWorkplace() : NULL;

		SVG_ANIMATION_TIME time_to_add = animation_workplace->VirtualDocumentTime() + m_time_value->GetOffset();
		RETURN_IF_ERROR(m_time_value->AddInstanceTime(time_to_add));

		animation_workplace->UpdateTimingParameters(timed_element_ctx->GetElement());
	}

	return OpStatus::OK;
}

/* virtual */ void
SVGTimeEventHandler::RegisterHandlers(DOM_Environment *environment)
{
	environment->AddEventHandler(m_time_value->GetEventType());
}

/* virtual */ void
SVGTimeEventHandler::UnregisterHandlers(DOM_Environment *environment)
{
	m_time_value->SetNotifier(NULL);
	environment->RemoveEventHandler(m_time_value->GetEventType());
}
#endif // XML_EVENTS_SUPPORT

SVGTimeObject::SVGTimeObject(SVGTimeType time_type) :
		SVGObject(SVGOBJECT_TIME),
		m_time_type(time_type),
		m_offset(0),
#ifdef XML_EVENTS_SUPPORT
		m_event_handler(NULL),
		m_event_target(NULL),
#endif
		m_element_id(NULL),
		m_event_name(NULL),
		m_event_prefix(NULL),
		m_notifier(NULL),
		m_access_key(0),
		m_event(SVGSYNCBASE_BEGIN),
		m_repetition(0)
{
}

/* virtual */ BOOL
SVGTimeObject::IsEqual(const SVGObject &other) const
{
	return FALSE;
}

SVGTimeObject::~SVGTimeObject()
{
	OP_DELETEA(m_element_id);
	OP_DELETEA(m_event_name);
	OP_DELETEA(m_event_prefix);
}

OP_STATUS SVGTimeObject::RegisterTimeValue(SVGDocumentContext* doc_ctx, HTML_Element* target_elm)
{
	OP_ASSERT(doc_ctx);
	OP_ASSERT(target_elm);

	if (TimeType() == SVGTIME_SYNCBASE || TimeType() == SVGTIME_REPEAT)
	{
		SVGTimingInterface* ctx = AttrValueStore::GetSVGTimingInterface(target_elm);

		if (!ctx)
			/* The element this time value is attached to is currently
			   in error.  But subsequent parsing or DOM operations may
			   make the element valid again. */

			return OpSVGStatus::INVALID_ANIMATION;

		OP_STATUS status;

		if (TimeType() == SVGTIME_SYNCBASE)
			status = ctx->AnimationSchedule().AddSyncbase(this);
		else /* TimeType() == SVGTIME_REPEAT */
			status = ctx->AnimationSchedule().AddRepeat(this);
		RETURN_IF_ERROR(status);
	}
	else if (TimeType() == SVGTIME_EVENT)
	{
		DOM_Environment *dom_environment = doc_ctx->GetDOMEnvironment();
		if (!dom_environment)
		{
			OP_STATUS status = doc_ctx->ConstructDOMEnvironment();
			RETURN_IF_MEMORY_ERROR(status);
			if (OpStatus::IsSuccess(status))
				dom_environment = doc_ctx->GetDOMEnvironment();
		}

#ifdef XML_EVENTS_SUPPORT
		if (dom_environment)
		{
			DOM_Object* node;
			RETURN_IF_ERROR(dom_environment->ConstructNode(node, target_elm));

			DOM_EventsAPI::EventTarget *event_target;
			RETURN_IF_ERROR(DOM_EventsAPI::GetEventTarget(event_target, node));

			if (!m_event_handler)
			{
				m_event_handler = OP_NEW(SVGTimeEventHandler, (this));
				if (!m_event_handler)
					return OpStatus::ERR_NO_MEMORY;
			}

			if (OpStatus::IsMemoryError(event_target->AddEventHandler(m_event_handler)))
			{
				OP_DELETE(m_event_handler);
				return OpStatus::ERR_NO_MEMORY;
			}

			OP_ASSERT(!m_event_target);
			m_event_target = event_target;
		}
		else
#endif // XML_EVENTS_SUPPORT
		{
			SVGElementContext* e_ctx = AttrValueStore::AssertSVGElementContext(target_elm);
			if (!e_ctx)
			{
				return OpStatus::ERR_NO_MEMORY;
			}
			RETURN_IF_ERROR(e_ctx->RegisterListener(target_elm, this));
		}
	}
	return OpStatus::OK;
}

OP_STATUS SVGTimeObject::UnregisterTimeValue(SVGDocumentContext* doc_ctx, HTML_Element* target_elm)
{
	SetNotifier(NULL);

	if (TimeType() == SVGTIME_EVENT)
	{
#ifdef XML_EVENTS_SUPPORT
		if (m_event_handler && m_event_target)
		{
			m_event_target->RemoveEventHandler(m_event_handler);
			m_event_target = NULL;
		}
		else
#endif
		{
			// We may have fallbacked on our own event processing
			SVGElementContext* e_ctx = AttrValueStore::GetSVGElementContext(target_elm);
			if (e_ctx)
			{
				RETURN_IF_ERROR(e_ctx->UnregisterListener(this));
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS
SVGTimeObject::GetInstanceTimes(SVGAnimationInstanceList& instance_list, BOOL allow_indefinite)
{
	if (m_time_type == SVGTIME_OFFSET)
	{
		return instance_list.Add(m_offset);
	}
	else if (m_time_type == SVGTIME_EVENT || m_time_type == SVGTIME_ACCESSKEY ||
			 m_time_type == SVGTIME_SYNCBASE || m_time_type == SVGTIME_REPEAT)
	{
		instance_list.HintAddition(m_activation_times.GetCount());
		for (unsigned i=0;i<m_activation_times.GetCount();i++)
		{
			RETURN_IF_ERROR(instance_list.Add(m_activation_times[i]));
		}
		return OpStatus::OK;
	}
	else if (m_time_type == SVGTIME_INDEFINITE)
	{
	    if (allow_indefinite)
		{
			instance_list.Add(SVGAnimationTime::Indefinite());
		}
		return OpStatus::OK;
	}
	else
	{
		OP_ASSERT(!"Not reached");
		return OpStatus::ERR;
	}
}

static OP_STATUS GetAnimationTimeString(TempBuffer* buffer, SVG_ANIMATION_TIME ms)
{
	double time = static_cast<double>(ms);
	const char* unit = "ms";
	if(time > 1000)
	{
		time = time * 0.001;
		unit = "s";
	}

	RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(time));
	return buffer->Append(unit);
}

/* virtual */ OP_STATUS
SVGTimeObject::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	if (m_time_type == SVGTIME_OFFSET)
	{
		return GetAnimationTimeString(buffer, m_offset);
	}
	else if (m_time_type == SVGTIME_EVENT ||
			 m_time_type == SVGTIME_SYNCBASE ||
			 m_time_type == SVGTIME_REPEAT ||
			 m_time_type == SVGTIME_ACCESSKEY)
	{
		OpString16 a;
		if(m_element_id)
		{
			RETURN_IF_ERROR(buffer->Append(m_element_id));
			RETURN_IF_ERROR(buffer->Append("."));
		}

		if (m_time_type == SVGTIME_EVENT)
		{
			if (m_event_prefix != NULL)
			{
				RETURN_IF_ERROR(buffer->Append(m_event_prefix));
				RETURN_IF_ERROR(buffer->Append(":"));
			}
			RETURN_IF_ERROR(buffer->Append(m_event_name));
		}
		else if (m_time_type == SVGTIME_ACCESSKEY)
		{
			RETURN_IF_ERROR(buffer->Append("accessKey("));
			RETURN_IF_ERROR(buffer->Append(m_access_key));
			RETURN_IF_ERROR(buffer->Append(")"));
		}
		else if (m_time_type == SVGTIME_SYNCBASE)
		{
			switch(m_event)
			{
				case SVGSYNCBASE_BEGIN:
					RETURN_IF_ERROR(buffer->Append("begin"));
					break;
				case SVGSYNCBASE_END:
					RETURN_IF_ERROR(buffer->Append("end"));
					break;
			}
		}
		else if (m_time_type == SVGTIME_REPEAT)
		{
			RETURN_IF_ERROR(buffer->Append("repeat(%d)", m_repetition));
		}

		if(m_offset != 0)
		{
			if(m_offset > 0)
			{
				RETURN_IF_ERROR(buffer->Append("+"));
			}
			RETURN_IF_ERROR(GetAnimationTimeString(buffer, m_offset));
		}
		return OpStatus::OK;
	}
	else if (m_time_type == SVGTIME_INDEFINITE)
	{
		return buffer->Append("indefinite");
	}
	else
	{
		OP_ASSERT(!"Not reached");
		return OpStatus::ERR;
	}
}

/* virtual */ SVGObject *
SVGTimeObject::Clone() const
{
	SVGTimeObject* time_object = OP_NEW(SVGTimeObject, (m_time_type));
	if (!time_object)
		return NULL;

	time_object->CopyFlags(*this);
	if (m_time_type == SVGTIME_OFFSET)
	{
		time_object->SetOffset(m_offset);
	}
	else if (m_time_type == SVGTIME_EVENT ||
			 m_time_type == SVGTIME_SYNCBASE ||
			 m_time_type == SVGTIME_REPEAT)
	{
		if (m_element_id && OpStatus::IsMemoryError(time_object->SetElementID(m_element_id, uni_strlen(m_element_id))))
		{
			OP_DELETE(time_object);
			return NULL;
		}

		if (m_time_type == SVGTIME_EVENT)
		{
			time_object->SetEventName(m_event_name, m_event_name ? uni_strlen(m_event_name) : 0,
									  m_event_prefix, m_event_prefix ? uni_strlen(m_event_prefix) : 0);
		}
		else if (m_time_type == SVGTIME_ACCESSKEY)
		{
			time_object->SetAccessKey(&m_access_key, 1);
		}
		else if (m_time_type == SVGTIME_SYNCBASE)
		{
			time_object->SetSyncbaseType(m_event);
		}
		else if (m_time_type == SVGTIME_REPEAT)
		{
			time_object->SetRepetition(m_repetition);
		}

		time_object->SetOffset(m_offset);
	}

	return time_object;
}

OP_STATUS
SVGTimeObject::AddInstanceTime(SVG_ANIMATION_TIME activation_time)
{
	OP_ASSERT(SVGAnimationTime::IsNumeric(activation_time));
	OP_ASSERT(m_time_type != SVGTIME_OFFSET);
	OP_ASSERT(m_time_type != SVGTIME_WALLCLOCK);
	OP_ASSERT(m_time_type != SVGTIME_INDEFINITE);

	RETURN_IF_ERROR(m_activation_times.Add(activation_time));
	return OpStatus::OK;
}

OP_STATUS
SVGTimeObject::SetEventName(const uni_char* event_name, unsigned event_length,
							const uni_char* prefix_name, unsigned prefix_length)
{
	OP_ASSERT(m_event_name == NULL);
	OP_ASSERT(m_event_prefix == NULL);
	if (OpStatus::IsMemoryError(UniSetStrN(m_event_name, event_name, event_length)) ||
		OpStatus::IsMemoryError(UniSetStrN(m_event_prefix, prefix_name, prefix_length)))
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		return OpStatus::OK;
	}
}

DOM_EventType
SVGTimeObject::GetEventType()
{
	if (!m_event_name)
		return DOM_EVENT_NONE;

	const uni_char *dom_event_name = m_event_name;
	unsigned event_name_length = uni_strlen(dom_event_name);

	// "focusin" matches the DOM name "DOMFocusIn" and so on.
	if (event_name_length == 7 && uni_str_eq(m_event_name, "focusin"))
	{
		dom_event_name = UNI_L("DOMFocusIn");
	}
	else if (event_name_length == 8 && uni_str_eq(m_event_name, "focusout"))
	{
		dom_event_name = UNI_L("DOMFocusOut");
	}
	else if (event_name_length == 8 && uni_str_eq(m_event_name, "activate"))
	{
		dom_event_name = UNI_L("DOMActivate");
	}

	DOM_EventType event_type = DOM_Environment::GetEventType(dom_event_name);
	return event_type;
}

const uni_char*
SVGTimeObject::GetEventNS()
{
	if (m_event_prefix != NULL && m_notifier != NULL)
	{
		HTML_Element *element = m_notifier->GetElement();
		XMLCompleteNameN name(NULL, 0, m_event_prefix, uni_strlen(m_event_prefix), UNI_L("dummy"), 5);
		if (XMLNamespaceDeclaration::ResolveNameInScope(element, name))
		{
			return name.GetUri();
		}
	}
	return NULL;
}

OP_STATUS
SVGTimeObject::SetElementID(const uni_char* element_id, int strlen)
{
	RETURN_IF_ERROR(UniSetStrN(m_element_id, element_id, strlen));

	int j = 0, i;
	for (i=0; j<strlen; i++, j++)
	{
		if (m_element_id[i] == '\\')
			j++;

		m_element_id[i] = m_element_id[j];
	}
	m_element_id[i] = '\0';
	return OpStatus::OK;
}

#endif // SVG_SUPPORT
