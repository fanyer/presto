/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_PROFILER

#include "modules/probetools/probetimeline.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/url/url_api.h"

/**
 * Default timer.
 */
class OpProbeSystemInfoTimer
	: public OpProbeTimeline::Timer
{
public:
	virtual double GetTime() { return g_op_time_info->GetRuntimeMS(); }
};

/**
 * Utility class for strdup'ed strings. This makes error handling
 * a little less painful in some cases.
 */
class OpProbeStringDuplicate
{
public:

	/**
	 * Create a new OpProbeStringDuplicate.
	 *
	 * This object will duplicate the string, and op_free it when this object
	 * is destroyed, unless 'release' is called. Note that the string
	 * duplication may fail due to OOM, and the caller must check that CStr
	 * does not return NULL after the object was constructed.
	 * 
	 * @param str The string to duplicate. NULL is illegal.
	 */
	OpProbeStringDuplicate(const uni_char *str)
		: m_str(str ? uni_strdup(str) : NULL)
	{
		OP_ASSERT(str);
	}

	/**
	 * @copydoc OpProbeStringDuplicate(const uni_char*)
	 *
	 * This is the 'char' version of the constructor. Will create a Unicode
	 * copy of the latin1 encoded string.
	 */
	OpProbeStringDuplicate(const char *str)
		: m_str(str ? uni_up_strdup(str) : NULL)
	{
		OP_ASSERT(str);
	}

	~OpProbeStringDuplicate()
	{
		if (m_str)
			op_free(m_str);
	}

	uni_char *CStr() { return m_str; }

	uni_char *Release()
	{
		uni_char *ret = m_str;
		m_str = NULL;
		return ret;
	}

private:
	uni_char *m_str;
};

OpProbeTimeline::OpProbeTimeline()
	: m_aggregator(&m_root)
	, m_timer(NULL)
	, m_default_timer(NULL)
	, m_next_id(1)
	, m_unique(1)
{
}

OpProbeTimeline::~OpProbeTimeline()
{
	if (m_timer && m_stack.GetSize() > 0)
		m_stack.GetTop()->End(m_timer->GetTime());

	OP_DELETE(m_default_timer);
}

OP_STATUS
OpProbeTimeline::Construct(Timer *timer)
{
	RETURN_IF_ERROR(m_stack.Push(&m_root));

	if (!timer)
	{
		// Use default timer if none was provided.
		m_default_timer = OP_NEW(OpProbeSystemInfoTimer, ());
		RETURN_OOM_IF_NULL(m_default_timer);
		timer = m_default_timer;
	}

	m_timer = timer;

	double now = timer->GetTime();

	m_stack.GetTop()->SetStart(now);
	m_stack.GetTop()->Begin(now);

	return OpStatus::OK;
}

double
OpProbeTimeline::GetRelativeTime(double time) const
{
	return (time - m_root.GetStart());
}

double
OpProbeTimeline::BeginOverhead()
{
	double now = m_timer->GetTime();

	m_stack.GetTop()->End(now);

	return now;
}

void
OpProbeTimeline::EndOverhead(double start)
{
	double now = m_timer->GetTime();

	m_stack.GetTop()->AddOverhead(now - start);
	m_stack.GetTop()->Begin(now);
}

OP_STATUS
OpProbeTimeline::FindEvent(unsigned id, Event *&event)
{
	return m_root.FindChild(id, event);
}

OpProbeTimeline::Collection *
OpProbeTimeline::TopCollection()
{
	return m_stack.GetTop()->GetCollection();
}

const uni_char *
OpProbeTimeline::FindString(const uni_char *in)
{
	if (!in)
		return NULL;

	void *void_data;

	if (OpStatus::IsSuccess(m_string_pool.GetData(in, &void_data)))
		return static_cast<const uni_char *>(void_data);

	return NULL;
}

OP_STATUS
OpProbeTimeline::AddString(uni_char *in)
{
	// Accept 'NULL', but don't pool it (no need).
	if (!in)
		return OpStatus::OK;

	RETURN_IF_MEMORY_ERROR(m_string_pool.Add(in, in));

	return OpStatus::OK;
}

OP_STATUS
OpProbeTimeline::GetString(const uni_char *src, const uni_char *&dst)
{
	dst = NULL;

	if (src == NULL)
		return OpStatus::OK;

	dst = FindString(src);

	if (dst)
		return OpStatus::OK;

	OpProbeStringDuplicate dup(src);
	RETURN_OOM_IF_NULL(dup.CStr());

	RETURN_IF_MEMORY_ERROR(AddString(dup.CStr()));

	dst = dup.Release();

	return OpStatus::OK;
}

OP_STATUS
OpProbeTimeline::GetString(const char *src, const uni_char *&dst)
{
	dst = NULL;

	if (src == NULL)
		return OpStatus::OK;

	OpProbeStringDuplicate dup(src);
	RETURN_OOM_IF_NULL(dup.CStr());

	dst = FindString(dup.CStr());

	if (dst)
		return OpStatus::OK; // String already pooled.

	RETURN_IF_MEMORY_ERROR(AddString(dup.CStr()));

	dst = dup.Release();

	return OpStatus::OK;
}

OP_STATUS
OpProbeTimeline::GetString(URL *url, const uni_char *&dst)
{
	OpString url_str;

	if (url)
		RETURN_IF_MEMORY_ERROR(url->GetAttribute(URL::KName_Username_Password_Hidden, url_str));

	return GetString(url ? url_str.CStr() : NULL, dst);
}

OP_STATUS
OpProbeTimeline::Aggregate(unsigned iterations)
{
	return m_aggregator.Aggregate(iterations);
}

BOOL
OpProbeTimeline::IsAggregationFinished() const
{
	return m_aggregator.IsFinished();
}

OpProbeTimeline::Event::Event(OpProbeEventType type)
	: m_id(0)
	, m_suc(NULL)
	, m_start(-1.0)
	, m_end(0)
	, m_time(0)
	, m_overhead(0)
	, m_aggregated_time(0)
	, m_aggregated_overhead(0)
	, m_type(type)
	, m_hits(1)
	, m_collection(NULL)
{
}

OpProbeTimeline::Event::~Event()
{
	OP_DELETE(m_collection);
}


OP_STATUS
OpProbeTimeline::Event::AddChild(const void *key, Event *event)
{
	if (!m_collection)
	{
		m_collection = OP_NEW(OpProbeTimeline::Collection, ());
		RETURN_OOM_IF_NULL(m_collection);
	}

	// Ownership transfer on OpStatus::OK.
	return m_collection->InsertEvent(key, event);
}

OpProbeTimeline::Event *
OpProbeTimeline::Event::FirstChild() const
{
	return m_collection ? m_collection->First() : NULL;
}

OpProbeTimeline::Event *
OpProbeTimeline::Event::LastChild() const
{
	return m_collection ? m_collection->Last() : NULL;
}

unsigned
OpProbeTimeline::Event::CountChildren() const
{
	unsigned count = 0;

	OpProbeTimeline::Event *e = FirstChild();

	while (e)
	{
		++count;
		e = e->Suc();
	}

	return count;
}

OP_STATUS
OpProbeTimeline::Event::FindChild(unsigned id, Event *&event)
{
	event = NULL;

	Iterator i(this);

	while (i.Next())
		if (static_cast<unsigned>(i.GetEvent()->GetId()) == id)
		{
			event = i.GetEvent();
			break;
		}

	RETURN_IF_ERROR(i.GetStatus());

	return event ? OpStatus::OK : OpStatus::ERR;
}

OpProbeTimeline::Collection *
OpProbeTimeline::Event::GetCollection()
{
	if (!m_collection)
		m_collection = OP_NEW(Collection, ());

	return m_collection;
}

BOOL
OpProbeTimeline::Event::Equals(Event *event)
{
	if (m_type != event->GetType())
		return FALSE;

	unsigned count = GetPropertyCount();

	if (count != event->GetPropertyCount())
		return FALSE;

	for (unsigned i = 0; i < count; ++i)
		if (!GetProperty(i)->Equals(event->GetProperty(i)))
			return FALSE;

	return TRUE;
}

void
OpProbeTimeline::Event::Begin(double now)
{
	m_end = now;
}

void
OpProbeTimeline::Event::End(double now)
{
	m_time += now - m_end;
	m_end = now;
}


OpProbeTimeline::Property::Property()
	: m_type(OpProbeTimeline::PROPERTY_UNDEFINED)
	, m_name(NULL)
{
}

OpProbeTimeline::Property::Property(const char *name, const uni_char *string)
	: m_type(OpProbeTimeline::PROPERTY_STRING)
	, m_name(name)
{
	m_u.string = string;
}

OpProbeTimeline::Property::Property(const char *name, int integer)
	: m_type(OpProbeTimeline::PROPERTY_INTEGER)
	, m_name(name)
{
	m_u.integer = integer;
}

BOOL
OpProbeTimeline::Property::Equals(const Property *prop) const
{
	if (m_type != prop->m_type)
		return FALSE;

	switch (m_type)
	{
	default:
		OP_ASSERT(!"Unknown property type.");
	case PROPERTY_UNDEFINED:
		return TRUE;
	case PROPERTY_STRING:
		// This works, because all strings stored in the property are
		// internalized (the same strings will have the same pointer value).
		return (m_u.string == prop->GetString());
	case PROPERTY_INTEGER:
		return (m_u.integer == prop->GetInteger());
	}
}

OpProbeTimeline::Collection::Collection()
	: m_first(NULL)
	, m_last(NULL)
{
}

OpProbeTimeline::Collection::~Collection()
{
	// Free the entire collection.
	Event *r = m_first;

	while (r)
	{
		Event *next = r->m_suc;
		OP_DELETE(r);
		r = next;
	}
}

OP_STATUS
OpProbeTimeline::Collection::InsertEvent(const void *key, Event *event)
{
	OP_ASSERT(event);

	RETURN_IF_ERROR(m_events[event->GetType()].Add(key, event));

	if (!m_first)
		m_first = event;

	if (m_last)
		m_last->m_suc = event;

	m_last = event;

	return OpStatus::OK;
}

OpProbeTimeline::Event *
OpProbeTimeline::Collection::GetEvent(OpProbeEventType type, const void *key)
{
	Event *event;

	if (OpStatus::IsSuccess(m_events[type].GetData(key, &event)))
		return event;

	return NULL;
}

OpProbeTimeline::Event *
OpProbeTimeline::Stack::GetIndex(int idx) const
{
	// Accept negative indices, so that -1 is the top of the stack.
	int index = (idx < 0) ? m_vec.GetCount() + idx : idx;

	return m_vec.Get(index);
}

OpProbeTimeline::Iterator::Iterator(OpProbeTimeline *timeline, int max_depth)
	: m_max_depth(max_depth)
	, m_status(m_stack.Push(&timeline->m_root))
{
	OP_ASSERT(timeline);
}

OpProbeTimeline::Iterator::Iterator(Event *start, int max_depth)
	: m_max_depth(max_depth)
	, m_status(m_stack.Push(start))
{
	OP_ASSERT(start);
}

BOOL
OpProbeTimeline::Iterator::Next()
{
	if (OpStatus::IsError(m_status))
		return FALSE;

	// Depth of current Event. (Depth of root node is '0').
	int depth = static_cast<int>(m_stack.GetSize()) - 1;

	// Does the stack top have a child?
	if (m_max_depth < 0 || depth < m_max_depth)
	{
		Event *event = m_stack.GetTop()->FirstChild();

		if (event)
			return OpStatus::IsSuccess(m_status = m_stack.Push(event));
	}

	// If there is no child, and this is the root Event, then we're done.
	// (Siblings of the root must not be included in the iteration).
	if (m_stack.GetSize() == 1)
		return FALSE;

	// A sibling, then?
	while (m_stack.GetSize() > 1)
	{
		Event *event = m_stack.Pop()->Suc();

		if (event)
			return OpStatus::IsSuccess(m_status = m_stack.Push(event));
	}

	return FALSE; // No more elements.
}


OpProbeTimeline::Aggregator::Aggregator(Event *root)
	: m_root(root)
	, m_current(NULL)
	, m_finished(FALSE)
	, m_status(m_stack.Push(root))
{
	OP_ASSERT(root);
}

OP_STATUS
OpProbeTimeline::Aggregator::Aggregate(unsigned iterations)
{
	RETURN_IF_ERROR(m_status);

	unsigned i = 0;

	while (i < iterations && Next())
	{
		Event *parent = m_stack.GetIndex(-1);

		OP_ASSERT(m_current);
		OP_ASSERT(parent);

		// Add self time and self overhead.
		m_current->m_aggregated_time += m_current->m_time;
		m_current->m_aggregated_overhead += m_current->m_overhead;

		// Propagate to parent.
		parent->m_aggregated_time += m_current->m_aggregated_time;
		parent->m_aggregated_overhead += m_current->m_aggregated_overhead;

		++i;
	}

	m_finished = (i < iterations);

	return m_status;
}

BOOL
OpProbeTimeline::Aggregator::Next()
{
	if (!m_current)
		m_current = m_root->FirstChild();
	else if (!m_current->Suc())
	{
		if (m_stack.GetSize() <= 1)
			return FALSE; // Done.

		m_current = m_stack.Pop();
		return TRUE;
	}
	else if (m_current->Suc())
		m_current = m_current->Suc();

	while (m_current && m_current->FirstChild())
	{
		RETURN_VALUE_IF_ERROR(m_status = m_stack.Push(m_current), FALSE);
		m_current = m_current->FirstChild();
	}

	return (m_current != NULL ? TRUE : FALSE);
}

OP_STATUS
OpProbeTimeline::Push(const void *key, OpProbeEventType type)
{
	Collection *collection = TopCollection();

	if (!collection)
		return OpStatus::ERR_NO_MEMORY;

	Event *event = collection->GetEvent(type, key);

	if (event)
	{
		event->Hit();
		RETURN_IF_ERROR(m_stack.Push(event));
		return OpActivationStatus::REUSED;
	}

	return OpStatus::ERR;
}

OP_STATUS
OpProbeTimeline::PushNew(const void *key, Event *event, double start)
{
	RETURN_OOM_IF_NULL(event);

	OpAutoPtr<Event> event_anchor(event);

	event->SetId(m_next_id++);
	event->SetStart(start);
	
	// Push onto stack.
	RETURN_IF_ERROR(m_stack.Push(event));

	// Insert into its proper parent (which takes over ownership on
	// success).
	if (OpStatus::IsError(m_stack.GetIndex(-2)->AddChild(key, event)))
	{
		// Failed, so remove it from stack.
		m_stack.Pop();
		return OpStatus::ERR_NO_MEMORY;
	}

	// The parent owns the event now.
	event_anchor.release();

	return OpActivationStatus::CREATED;
}

OP_STATUS
OpProbeTimeline::PushNew(const void *key, OpProbeEventType type, double start)
{
	return PushNew(key, OP_NEW(OpProbeTimeline::TypeEvent, (type)), start);
}

void
OpProbeTimeline::Pop()
{
	double now = m_timer->GetTime();

	m_stack.Pop()->End(now);
	m_stack.GetTop()->Begin(now);
}

#endif // SCOPE_PROFILER
