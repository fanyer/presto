/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef PROBE_TIMELINE_H
#define PROBE_TIMELINE_H

#ifdef SCOPE_PROFILER

#include "modules/util/OpHashTable.h"
#include "modules/util/adt/opvector.h"

class URL;
class DocumentManager;
class OpProbeTimeline;

/**
 * Describes the type of a event produced by a OpProbeTimeline::Probe. This
 * may be used to extract details about probes which carry extra information
 * relevant to the profiler UI.
 */
enum OpProbeEventType
{
	/**
	 * Unknown purpose.
	 */
	PROBE_EVENT_GENERIC,

	/**
	 * Type for the (hidden) root probe used by OpProbeTimeline.
	 */
	PROBE_EVENT_ROOT,

	/**
	 * Measures time spent during CSS style recalculation. The event is
	 * used as a parent for individual CSS_SELECTOR events.
	 */
	PROBE_EVENT_STYLE_RECALCULATION,

	/**
	 * Used to measure CSS selector matching. The event carries information
	 * about which CSS selector we are trying to match.
	 */
	PROBE_EVENT_CSS_SELECTOR_MATCHING,

	/**
	 * Measures time spent evaluating a certain thread. The event carries
	 * information about what kind of thread was evaluated, and, if the
	 * the thread was an event thread, the name of the (DOM) event that
	 * fired.
	 */
	PROBE_EVENT_SCRIPT_THREAD_EVALUATION,

	/**
	 * Measures time during a reflow. This Event does not *always* trigger
	 * very bad stuff (but it *may* indeed); if no element was found 'dirty',
	 * layout is not necessary.
	 */
	PROBE_EVENT_REFLOW,

	/**
	 * Measures time spent laying out the document.
	 */
	PROBE_EVENT_LAYOUT,

	/**
	 * Measures the time spent painting the document.
	 */
	PROBE_EVENT_PAINT,

	/**
	 * Parsing of the main document (typically XHTML or HTML).
	 */
	PROBE_EVENT_DOCUMENT_PARSING,

	/**
	 * Time spent parsing stylesheets. The Event carries information
	 * about the URL of the stylesheet that was parsed. If the stylesheet is
	 * inline, the parent document is used as the URL.
	 */
	PROBE_EVENT_CSS_PARSING,

	/**
	 * Time spent compiling scripts. The Event carries information
	 * about the URL of the script that was compiled. If the script is inline,
	 * the parent document is used as the URL.
	 */
	PROBE_EVENT_SCRIPT_COMPILATION,

	/**
	 * Counts the elements in the enum (and must therefore
	 * be the last element).
	 */
	PROBE_EVENT_COUNT
};

/**
 * Represents a profiling session.
 *
 * Sessions knows how to create new OpProbeTimelines. OpProbeTimelines added
 * to the OpProfilingSession is also owned by the OpProfilingSession.
 */
class OpProfilingSession
{
public:

	/**
	 * Destructor.
	 */
	virtual ~OpProfilingSession() {}

	/**
	 * Add a OpProbeTimeline to the OpProfilingSession.
	 *
	 * @param docman The DocumentManager the OpProbeTimeline is for.
	 * @return A new OpProbeTimeline, or NULL on OOM.
	 */
	virtual OpProbeTimeline *AddTimeline(DocumentManager *docman) = 0;
};

/**
 * A timeline is a collection of events, ordered linearly according
 * to the start time of each interval.
 *
 * The object acts as a "profiling manager"; it remembers all information
 * collected by its probes for later retrieval by a UI that wants to present
 * the data.
 */
class OpProbeTimeline
{
public:

	// Forward declarations.
	class Iterator;
	class Timer;
	class Event;
	class Property;
	class Aggregator;
	class Collection;

	// Friends.
	friend class Iterator;

	/**
	 * Constructor. Initializes members.
	 */
	OpProbeTimeline();

	/**
	 * Destructor.
	 */
	~OpProbeTimeline();

	/**
	 * Second stage constructor.
	 *
	 * @param timer A OpProbeTimer::Timer to measure time with. If NULL, the
	 *              default timer will be used automatically. If non-NULL, the
	 *              timer will be used, and timer must exist until the
	 *              OpProbeTimeline is destroyed (ownership is not
	 *              transferred).
	 *
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS Construct(Timer *timer = NULL);

	/**
	 * Get a unique key (per OpProbeTimeline). The key can be used by probes
	 * which want every invocation to correspond to an event in the timeline.
	 */
	void *GetUniqueKey() { return reinterpret_cast<void*>(++m_unique); }

	/**
	 * Get the current time since some time in the past.
	 *
	 * @return The time, in milliseconds.
	 */
	double GetTime() { return m_timer->GetTime(); }

	/**
	 * Get the time relative to the start time of the timeline.
	 *
	 * @param time The time (as returned by GetTime()) to convert to relative
	 *             time.
	 * @return The time (in milliseconds) relative to the timeline.
	 */
	double GetRelativeTime(double time) const;

	/**
	 * Indicate that profiling overhead will follow. This will stop time
	 * recording on Event currently on the stack top.
	 *
	 * @return A timestamp of the current time.
	 */
	double BeginOverhead();

	/**
	 * Indicate that there will be no more significant profiling overhead
	 * (until the next BeginOverhead()). This will start time recording on
	 * the Event currently on the stack top.
	 *
	 * It will also add the overhead to the Event on the stack top.
	 *
	 * @param start The timestamp returned from BeginOverhead().
	 */
	void EndOverhead(double start);

	/**
	 * Find the Event with the specified ID.
	 *
	 * @param [in] id The ID for the Event.
	 * @param [out] event Will be set to the Event, if found.
	 * @return OpStatus::OK if the Event was found; OpStatus::ERR if the Event
	 *         was *not* found; or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS FindEvent(unsigned id, Event *&event);

	/**
	 * Return the Collection for the Event on the stack top. This function
	 * will try to allocate a new Collection, if not already present.
	 *
	 * @return The Collection, or NULL on OOM.
	 */
	Collection *TopCollection();

	/**
	 * @return The root Event for this OpProbeTimeline.
	 */
	Event *GetRoot() { return &m_root; }

	/**
	 * Check whether a certain Event is the root Event of this timeline.
	 *
	 * @param event The Event to check.
	 * @return TRUE if the Event is root, FALSE otherwise.
	 */
	BOOL IsRoot(const Event *event) const { return event == &m_root; }

	/**
	 * @return The Event on the top of the stack.
	 */
	Event *GetTop(){ return m_stack.GetTop(); }

	/**
	 * Find a string (already) in the string pool.
	 *
	 * This function will return a non-NULL value if the provided string
	 * already exists in the string pool. It will not add the string to
	 * pool if it's missing.
	 *
	 * @param in The string to search for.
	 * @return The pooled string, or NULL if the string was not pooled.
	 */
	const uni_char *FindString(const uni_char *in);

	/**
	 * Add a string to the string pool. The string is *not* copied, and must
	 * be allocated and readied by the caller. Ownership is immediately
	 * transferred regardless of return value.
	 *
	 * This function *must not* be called if the string is already pooled
	 * (i.e. if 'FindString' returns non-NULL).
	 *
	 * @param in The string to add. Ownership is transferred to the timeline
	 *           only if OpStatus::OK is returned.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS AddString(uni_char *in);

	/**
	 * Acquire a string from the string pool (uni_char version).
	 *
	 * The returned string is guaranteed to be alive as long as the parent
	 * OpProbeTimeline is alive (and no longer). The same pointer value will
	 * be returned for the same input string, so, for pooled strings, strings
	 * are equal if the pointer values are equal.
	 *
	 * @param [in] src The string to put in the string pool.
	 * @param [out] dst The pooled version of the string (on OpStatus::OK).
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS GetString(const uni_char *src, const uni_char *&dst);

	/**
	 * Acquire a string from the string pool (char version).
	 *
	 * The returned string is guaranteed to be alive as long as the parent
	 * OpProbeTimeline is alive (and no longer). The same pointer value will
	 * be returned for the same input string, so, for pooled strings, strings
	 * are equal if the pointer values are equal.
	 *
	 * @param [in] src The string to put in the string pool.
	 * @param [out] dst The pooled version of the string (on OpStatus::OK).
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS GetString(const char *src, const uni_char *&dst);

	/**
	 * Get a pooled string representation of a URL.
	 *
	 * The returned string is guaranteed to be alive as long as the parent
	 * OpProbeTimeline is alive (and no longer). The same pointer value will
	 * be returned for the same input URLs. Therefore, pooled string
	 * representations of two URLs are equal if their pointer values are equal.
	 *
	 * @param [in] src The URL to get a pooled string for.
	 * @param [out] dst The pooled string representation of the URL.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS GetString(URL *url, const uni_char *&dst);

	/**
	 * Compute aggregated values for a while.
	 *
	 * When profiling is finished, the OpProbeTimeline may contain a large tree
	 * of Events. For each Event, we need to know the aggregated time, overhead
	 * and hits. This can most efficiently be computed after profiling has
	 * stopped, and the Event tree is complete.
	 *
	 * This function goes through the Event tree depth-first, bottom-up, and
	 * propagates timing and hit information upwards. The iteration visits
	 * each Event only once, and the number of Events to visit on each function
	 * call is specified as a parameter.
	 *
	 * Use IsAggregationFinished() to check whether all nodes have been
	 * visisted.
	 *
	 * @see IsAggregationFinished
	 *
	 * @param iterations How many Events to visit in this function call.
	 * @return OpStatus::OK on success, however, this does not mean that
	 *         aggregation finished. Only that the specified number of
	 *         iterations wre completed. Otherwise, OpStatus::ERR_NO_MEMORY is
	 *         returned on OOM.
	 */
	OP_STATUS Aggregate(unsigned iterations = 1024);

	/**
	 * Check whether aggregation is completed for this OpProbeTimeline.
	 *
	 * @return TRUE if aggregation finished, FALSE otherwise.
	 */
	BOOL IsAggregationFinished() const;

	/**
	 * An Event is a record produced by a Probe construction/destruction. One
	 * Probe may produce multiple events, depending on parameters.
	 *
	 * Each event contains a time variable, which indicates how much time
	 * has been spent on a certain task. The same task can be performed many
	 * times, and the time spent on the task is accumulated on each destruction
	 * of an (active) probe.
	 *
	 * Additionally, each event contains the interval from when the task was
	 * first performed, to when it was last performed.
	 */
	class Event
	{
	public:
		friend class OpProbeTimeline;
		friend class Aggregator;
		friend class Collection;

		/**
		 * Constructor. Begins the measurement.
		 */
		Event(OpProbeEventType type);

		/**
		 * Destructor.
		 */
		virtual ~Event();

		/**
		 * Get the number of properties on this Event.
		 *
		 * @return The number of properties (which may also be zero).
		 */
		virtual unsigned GetPropertyCount() const = 0;

		/**
		 * Get the properties of this Event.
		 *
		 * @return An array of Properties. The length of the array is equal to
		 *         the return value of GetPropertyCount(). The properties are
		 *         only guaranteed to live a long as the Event that owns them.
		 */
		virtual const Property *GetProperties() const = 0;

		/**
		 * Get the Property with the specified name.
		 *
		 * @return The Property, or NULL if there is no property with that
		 *         name.
		 */
		virtual const Property *GetProperty(const char *name) const = 0;

		/**
		 * Get the n'th Property.
		 *
		 * This function does not (necessarily) perform bounds checking.
		 * Thefore, you should always call GetPropertyCount() first to
		 * guarantee that the provided index is inside of bounds.
		 *
		 * @param index The index of the Property to access.
		 * @return The Property at the specified index. If the index is out of
		 *         bounds, the return value is undefined.
		 */
		virtual const Property *GetProperty(unsigned index) const = 0;

		/**
		 * Add a Property to the Event.
		 *
		 * If there is not enough space for another Property on this Event,
		 * the call will fail silently.
		 *
		 * @param prop The property to add to this Event.
		 */
		virtual void AddProperty(const Property &prop) = 0;

		/**
		 * Next Event in linked list.
		 */
		Event *Suc() const { return m_suc; }

		/**
		 * Get the start of the interval.
		 */
		double GetStart() const { return m_start; }

		/**
		 * Get the end of the interval.
		 */
		double GetEnd() const { return m_end; }

		/**
		 * The time spent on this Event.
		 */
		double GetTime() const { return m_time; }

		/**
		 * @return The overhead associated with this Event.
		 */
		double GetOverhead() const { return m_overhead; }

		/**
		 * Get the total self-time spent on this Event, plus total self-time
		 * spent on all child Events.
		 *
		 * @return The aggregated self-time.
		 */
		double GetAggregatedTime() const { return m_aggregated_time; }

		/**
		 * Get the total overhead spent on this Event, plus total overhead
		 * spent on all child Events.
		 *
		 * @return The aggregated overhead.
		 */
		double GetAggregatedOverhead() const { return m_aggregated_overhead; }

		/**
		 * The probe type that created this event.
		 */
		OpProbeEventType GetType() const { return m_type; }

		/**
		 * Get the number of times this event was hit.
		 */
		unsigned GetHits() const { return m_hits; }

		/**
		 * Add a child Event to this Event. Ownership is transferred to the
		 * OpProbeTimeline if (and only if) the function returns OpStatus::OK.
		 *
		 * @param key The key for the Event.
		 * @param event The Event to add.
		 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS AddChild(const void *key, Event *event);

		/**
		 * Get the first child of this Event, or NULL if this
		 * Event has no children.
		 *
		 * @return The first child Event, or NULL.
		 */
		Event *FirstChild() const;

		/**
		 * Get the last child of this Event, or NULL if this
		 * Event has no children.
		 *
		 * @return The last child Event, or NULL.
		 */
		Event *LastChild() const;

		/**
		 * Count the number of immedidate children (not grandchildren).
		 *
		 * @return The number of children for this Event.
		 */
		unsigned CountChildren() const;

		/**
		 * Find a child Event with the specified ID. This functions searches the
		 * entire Event subtree, starting with this Event.
		 *
		 * @param [in] id The ID for the Event to find.
		 * @param [out] event The Event, if found, is stored here. The value is
		 *              only valid if OpStatus::OK is returned.
		 * @return OpStatus::OK if the event was found, OpStatus::ERR if not.
		 */
		OP_STATUS FindChild(unsigned id, Event *&event);

		/**
		 * Get the ID for this Event.
		 *
		 * @return The nonzero, positive ID for this Event.
		 */
		int GetId() const { return m_id; }

		/**
		 * Get, and possibly create on demand, the Collection for this Event.
		 *
		 * @return The Collection, or NULL on OOM.
		 */
		Collection *GetCollection();

		/**
		 * Return TRUE if this Event has the same type as the incoming Event,
		 * and the two Events have the same properties (same values, same
		 * order).
		 *
		 * @param event The Event to check for equality.
		 * @return TRUE if the Events are equal, FALSE otherwise.
		 */
		BOOL Equals(Event *event);

	private:

		/**
		 * Increase the hit count by one.
		 */
		void Hit() { ++m_hits; }

		/**
		 * Set the ID for this Event.
		 *
		 * @param id The ID for the Event.
		 */
		void SetId(int id) { m_id = id; }

		/**
		 * Set the start of the interval for this Event.
		 *
		 * @param start The start of the interval.
		 */
		void SetStart(double start) { if (m_start < 0) m_start = start; }

		/**
		 * Begin measurement.
		 *
		 * @param now The current time.
		 */
		void Begin(double now);

		/**
		 * End measurement. (Accumulate time spent on the task).
		 *
		 * @param now The current time.
		 */
		void End(double now);

		/**
		 * Add overhead to this Event.
		 *
		 * @param overhead The time to add to the overhead.
		 */
		void AddOverhead(double overhead){ m_overhead += overhead; }

	private:

		/// Non-zero integer ID for this Event. Unique for each OpProbeTimeline.
		int m_id;

		/// Next item in linked list (or NULL).
		Event *m_suc;

		/// Start of interval.
		double m_start;

		/// End of interval.
		double m_end;

		/// Time spent on Event.
		double m_time;

		/// Overhead for this Event.
		double m_overhead;

		/// Time for this Event + time for all child Events.
		double m_aggregated_time;

		/// Overhead for this Event + overhead for all child Events.
		double m_aggregated_overhead;

		/// The type of this Event.
		OpProbeEventType m_type;

		/// Number of times this event has been committed to.
		unsigned m_hits;

		/// A collection of events, or NULL if this event has no children.
		Collection *m_collection;
	};

	/**
	 * Enumeration for different types of Properties. The type of the
	 * Property determines which field of the union should be accessed.
	 */
	enum PropertyType
	{
		/**
		 * The Property has no value. (For default constructors).
		 */
		PROPERTY_UNDEFINED,

		/**
		 * The Property is a string. Note that all strings are either static,
		 * or internalized by the the OpProbeTimeline. Therefore, the string
		 * is only guaranteed to live as long as its parent timeline.
		 */
		PROPERTY_STRING,

		/**
		 * The Propety is an integer.
		 */
		PROPERTY_INTEGER
	};

	/**
	 * A Property is a name/value pair associated with an Event.
	 *
	 * For some Events, it is necessary to carry additional information about
	 * the circumstances of the Event. For instance, for CSS selector probes,
	 * we need to know the selector text, and for thread evaluation probes,
	 * we would like to know the thread type, and the event name (if the
	 * thread is an event thread). This kind of information is stored in
	 * properties.
	 */
	class Property
	{
	public:

		/**
		 * Default constructor, which produces a Property with no name,
		 * and of type PROPERTY_UNDEFINED. Needed for arrays of properties.
		 */
		Property();

		/**
		 * Create a string property.
		 *
		 * @param name The name of the property. Must be static.
		 * @param string The string value. Must either be static, or a
		 *               a string internalized by the OpProbeTimeline.
		 */
		Property(const char *name, const uni_char *string);

		/**
		 * Create an integer property.
		 *
		 * @param name The name of the property. Must be static.
		 * @param integer The integer value.
		 */
		Property(const char *name, int integer);

		/**
		 * @return The name of the Property. Never NULL.
		 */
		const char *GetName() const { return m_name; }

		/**
		 * @return The type of this property. Can be used to determine which
		 *         Get*-function to call.
		 */
		PropertyType GetType() const { return m_type; }

		/**
		 * Get the string contained in this Property. Use this only if you know
		 * that GetType() == PROPERTY_STRING.
		 *
		 * @return The string contained in this Property.
		 */
		const uni_char *GetString() const
		{
			OP_ASSERT(m_type == PROPERTY_STRING);
			return m_u.string;
		}

		/**
		 * Get the unsigned integer contained in this Property. Use this only if
		 * you know that GetType() == PROPERTY_INTEGER.
		 *
		 * @return The unsigned integer contained in this Property.
		 */
		unsigned GetUnsigned() const
		{
			OP_ASSERT(m_type == PROPERTY_INTEGER);
			return static_cast<unsigned>(m_u.integer);
		}

		/**
		 * Get the integer contained in this Property. Use this only if
		 * you know that GetType() == PROPERTY_INTEGER.
		 *
		 * @return The integer contained in this Property.
		 */
		int GetInteger() const
		{
			OP_ASSERT(m_type == PROPERTY_INTEGER);
			return m_u.integer;
		}

		/**
		 * Check whether this Property is equal to another Property.
		 *
		 * The Properties are equal if the types and values are equal.
		 *
		 * @return TRUE if equal, or FALSE otherwise.
		 */
		BOOL Equals(const Property *prop) const;

	private:
		/// The type of this Property. Determines which union field to access.
		PropertyType m_type;

		/// The name of this Property. Points to static memory.
		const char *m_name;

		/// Used to store the value.
		union
		{
			/// PROPERTY_STRING. The string is either static, or internalized
			/// by the OpProbeTimeline.
			const uni_char *string;

			/// PROPERTY_INTEGER.
			int integer;
		} m_u;
	};

	/**
	 * A Collection of Events. It keeps track of Events both in a hash
	 * table, and a linked list.
	 *
	 * Each Event may optionally contain a Collection of other Events
	 * (its children).
	 */
	class Collection
	{
	public:
		/**
		 * Constructor.
		 */
		Collection();

		/**
		 * Destructor. Frees all Events in the Collection.
		 */
		~Collection();

		/**
		 * Insert an Event into the collection. The Event will be inserted
		 * into and linked list and a hash table.
		 *
		 * @param key The key for this Event.
		 * @param event The Event to insert into the collection.
		 * @return OpStatus::OK on success or OpStatus::ERR_NO_MEMORY. It may also
		 *         return OpStatus::ERR, but only if this function has been called
		 *         when it shouldn't (i.e. it is called when the key already exists
		 *         for that Event type. But if that is the case, this function does
		 *         not need to be called).
		 */
		OP_STATUS InsertEvent(const void *key, Event *event);

		/**
		 * Get an Event with the specified type and key.
		 *
		 * @param type The type of the Event to find.
		 * @param key The 'key' for the Event to find.
		 * @return An Event with the specified type/key, or NULL if there's no Event
		 *         with that type/key.
		 */
		Event *GetEvent(OpProbeEventType type, const void *key);

		/**
		 * Get the first Event in this Collection (according to the order which
		 * the Events were inserted).
		 *
		 * @return An Event, or NULL if there are no children.
		 */
		Event *First() const { return m_first; }

		/**
		 * Get the last Event in this Collection (according to the order which
		 * the Events were inserted).
		 *
		 * @return An Event, or NULL if there are no children.
		 */
		Event *Last() const { return m_last; }

	private:

		/// First Event in linked list.
		Event *m_first;

		/// Last Event in linked list.
		Event *m_last;

		/// Hash tables for reusing Event objects.
		OpPointerHashTable<void, Event> m_events[PROBE_EVENT_COUNT];
	};

	/**
	 * An implementation of this interface will be used by the OpProbeTimeline
	 * to measure time.
	 *
	 * It is useful to keep the timing mechanism abstract, because it allows
	 * tests to implement a Timer which is easy to control.
	 */
	class Timer
	{
	public:
		virtual ~Timer() {}

		/**
		 * @return The time (in milliseconds) since some point in time.
		 */
		virtual double GetTime() = 0;
	};

	/**
	 * A Stack of Events. Used while profiling to create parent-child
	 * relationships between Events, and used by iterators to traverse
	 * the Event tree in a depth-first fasion.
	 */
	class Stack
	{
	public:

		/**
		 * Push an Event to the top of the Stack. Will grow the Stack as needed.
		 *
		 * @param event The Event to push on the stack.
		 * @return OpStatus::OK on success, or OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS Push(Event *event) { return m_vec.Add(event); }

		/**
		 * Remove the top Stack item, and return it.
		 *
		 * It is illegal to pop the last item from the Stack, since the rest
		 * of the profiling code will assume that GetTop() always returns a
		 * valid pointer.
		 *
		 * @return The Event that used to be on the top of the Stack.
		 */
		Event *Pop() { return m_vec.Remove(m_vec.GetCount() - 1); }

		/**
		 * Get the nth item on the Stack, or NULL if the index is out of
		 * bounds. Zero points to the bottom of the stack, and  'GetSize() - 1'
		 * points to the top. Negative indices may be used, such that '-1'
		 * points to the top of the stack, '-2' points to the item just below
		 * that, and so forth.
		 *
		 * @param idx The index of the item to retrieve.
		 * @return A pointer to the Event at the specified location, or NULL
		 *         if the index was out of bounds.
		 */
		Event *GetIndex(int idx) const;

		/**
		 * Get the item at the top of the stack. This will never return NULL,
		 * because it is illegal to pop the last item from this stack.
		 *
		 * @return The Event at the top of the stack. Never NULL.
		 */
		Event *GetTop() const { return GetIndex(-1); }

		/**
		 * Get the current size of the stack.
		 *
		 * @return The current size of the stack. The value is never smaller
		 *         than '1'.
		 */
		unsigned GetSize() const { return m_vec.GetCount(); }

	private:

		/// The actual stack of Events.
		OpVector<Event> m_vec;
	};

	/**
	 * Depth-first, top-down iterator for Event trees.
	 *
	 * The Iterator can optionally be configured to only include Events down
	 * to a certain depth (relative to the initial root Event).
	 *
	 * The initial root Event is never included in the iteration. If you have
	 * an event A with children B and C, and create an Iterator with A as the
	 * root node, only B and C will be retrieved.
	 */
	class Iterator
	{
	public:
		/**
		 * Construct an Iterator, using the OpProbeTimeline's root Event as
		 * the root for the traversal.
		 *
		 * @param timeline The OpProbeTimeline to iterate over.
		 * @param max_depth Do not traverse beyond this depth. The depth is
		 *                  relative to root Event of the OpProbeTimeline.
		 *                  A value of '1' yields only Events on the level
		 *                  immediately beneath the root (and no deeper).
		 */
		Iterator(OpProbeTimeline *timeline, int max_depth = -1);

		/**
		 * Construct an Iterator, using the specified Event as the root for
		 * the traversal.
		 *
		 * @param start The root Event for the traversal.
		 * @param max_depth Do not traverse beyond this depth. The depth is
		 *                  relative to the chosen root Event.
		 *                  A value of '1' yields only Events on the level
		 *                  immediately beneath the root (and no deeper).
		 */
		Iterator(Event *start, int max_depth = -1);

		/**
		 * Move to the next position in the Event tree.
		 *
		 * @see GetStatus
		 * @return TRUE if we could move to the next Event. FALSE if there
		 *         where no more events, or if an error occurred. Use
		 *         GetStatus to check for errors.
		 */
		BOOL Next();

		/**
		 * Check the status of the Iterator, to determine the meaning of the
		 * return value from 'Next'.
		 *
		 * @return OpStatus::OK on success, or OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS GetStatus() const { return m_status; };

		/**
		 * Get the current Event pointed to by the Iterator. Only valid if the
		 * last call to 'Next' returned TRUE.
		 *
		 * @return The current Event pointed to by the Iterator.
		 */
		Event *GetEvent() const { return m_stack.GetIndex(-1); }

		/**
		 * Get the parent of the current Event pointed to by the Iterator.
		 * Only valid if the last call to 'Next' returned TRUE.
		 *
		 * @return The current Event pointed to by the Iterator.
		 */
		Event *GetParent() const { return m_stack.GetIndex(-2); }

	private:

		/// Stack used for depth-first iteration.
		Stack m_stack;

		/// Max depth for the iteration. ('-1' means "unlimited").
		int m_max_depth;

		/// The current error status for the iterator.
		OP_STATUS m_status;
	};

	/**
	 * An Aggregator is a special iterator, which goes through the Event tree
	 * depth-first, bottom-up, and propgates timing information upwards to
	 * form aggregate values.
	 */
	class Aggregator
	{
	public:
		/**
		 * Constructor.
		 *
		 * @param root The root for the iteration.
		 */
		Aggregator(Event *root);

		/**
		 * Compute aggregate values for a while.
		 *
		 * @see OpProbeTimeline::Aggregate.
		 *
		 * @param iterations How many Events to visit in this function call.
		 * @return OpStatus::OK on success, however, this does not mean that
		 *         aggregation finished. Only that the specified number of
		 *         iterations wre completed. Otherwise, OpStatus::ERR_NO_MEMORY is
		 *         returned on OOM.
		 */
		OP_STATUS Aggregate(unsigned iterations);

		/**
		 * Check whether aggregation is finished.
		 *
		 * @return TRUE if finished, FALSE if not.
		 */
		BOOL IsFinished() const { return m_finished; }

	private:

		/**
		 * Move the iterator to the next position.
		 *
		 * @return TRUE if successful, FALSE if there are no more elements,
		 *         or if we ran out of memory.
		 */
		BOOL Next();

		/// Stack used for iteration.
		Stack m_stack;

		/// The root of the iteration.
		Event *m_root;

		/// The current Event in the iteration.
		Event *m_current;

		/// Whether aggregation is finished or not.
		BOOL m_finished;

		/// Status flag set by constructor and 'Next' function.
		OP_STATUS m_status;
	};

	/**
	 * Try to push an existing Event (with the same key/type pair) onto
	 * the stack.
	 *
	 * @param key The key for the Event.
	 * @param type The type of the Event.
	 * @return OpStatus::OK if an Event was found and pushed successfully;
	 *         OpStatus::ERR if no Event with the specified key/type pair
	 *         was found; or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS Push(const void *key, OpProbeEventType type);

	/**
	 * Push a new Event onto the stack. The timeline takes ownership of
	 * the Event *immediately*, regardless of return value.
	 *
	 * Note that the function will return OpStatus::ERR_NO_MEMORY if the
	 * value of the 'event' parameter is NULL.
	 *
	 * @param key The key for the Event.
	 * @param event The event to push. Ownership is transferred. If NULL is
	 *              passed, OpStatus::ERR_NO_MEMORY is returned.
	 * @param start The start time for the event (from BeginOverhead()).
	 * @return OpStatus:OK, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS PushNew(const void *key, Event *event, double start);

	/**
	 * Create and push a new TypeEvent.
	 *
	 * @param key The key for the Event.
	 * @param type The type of the Event to create.
	 * @param start The start time for the event (from BeginOverhead()).
	 * @return OpStatus:OK, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS PushNew(const void *key, OpProbeEventType type, double start);

	/**
	 * Remove the top Event from the stack, stop recording time on that
	 * Event, and start recording time on the new stack top.
	 */
	void Pop();

protected:

	/**
	 * Reset the reference to the timer provided to Construct().  This leaves
	 * the timeline without a timer, which is generally invalid, so should only
	 * be used immedately before the timeline is destroyed, typically if the
	 * timer is destroyed just before the timeline.
	 */
	void ResetTimer() { m_timer = NULL; }

private:

	/**
	 * A kind of event which contains nothing except its type.
	 */
	class TypeEvent
		: public Event
	{
	public:
		/**
		 * Constructor.
		 */
		TypeEvent(OpProbeEventType type) : Event(type) {}

		// Event
		virtual unsigned GetPropertyCount() const { return 0; }
		virtual const Property *GetProperties() const { return NULL; }
		virtual const Property *GetProperty(const char *name) const { return NULL; }
		virtual const Property *GetProperty(unsigned index) const { return NULL; }
		virtual void AddProperty(const Property & prop) { }
	};

	/**
	 * Type for root Event. The root Event is the top-most Event in the
	 * Event tree.
	 */
	class RootEvent
		: public TypeEvent
	{
	public:
		/**
		 * Constructor.
		 */
		RootEvent() : TypeEvent(PROBE_EVENT_ROOT) {}
	};

	/**
	 * Used for string interning. The same string may be used many times by
	 * different Events. There is no need to store those more than once.
	 */
	class StringPool
		: public OpGenericStringHashTable
	{
	public:
		/**
		 * Destructor. Deletes all strings.
		 */
		virtual ~StringPool(){ this->DeleteAll(); }

		// OpGenericStringHashTable
		virtual void Delete(void *data) { op_free(static_cast<uni_char*>(data)); }
	};

	/// The top-most Event in the Event tree.
	RootEvent m_root;

	/// Used for string interning.
	StringPool m_string_pool;

	/// Used to create parent-child relationships in the Event tree.
	Stack m_stack;

	/// Used to aggregate the Event tree.
	Aggregator m_aggregator;

	/// Points to the Timer this OpProbeTimeline is using.
	Timer *m_timer;

	/// If a Timer is not provided by the 'outside', then we allocate a default
	/// Timer, and store it here.
	Timer *m_default_timer;

	/// Each Event has an ID. The next available ID is provided by this variable.
	int m_next_id;

	/// For Probes that "wish" to produce a new Event on each activation, the
	/// OpProbeTimeline provides unique keys. This keeps track of the next
	/// available ID.
	/// @see OpProbeTimeline::GetUniqueKey
	INTPTR m_unique;
};

/**
 * Return values for functions that activates probes. It tells the caller
 * whether the probe activation resulted in a new Event, or the reusage
 * of an existing Event.
 */
class OpActivationStatus
	: public OpStatus
{
public:
	enum
	{
		/**
		 * The type/key pair specfied by the probe has been seen before, and
		 * the Event for that pair was reused. It is not necessary to add
		 * properties in this case. The Event that was reused already has
		 * the properties set.
		 */
		REUSED = USER_SUCCESS + 1,

		/**
		 * The type/key pair resulted in a new Event on the timeline. In this
		 * case, the caller should set properties (if any).
		 */
		CREATED = USER_SUCCESS + 2
	};
};

/**
 * A Probe is an object placed in code we want to measure. Probes are
 * stack allocated, and submit timing information to an OpProbeTimeline
 * on initialization and destruction.
 *
 * A Probe activation will either produce an new Event in the
 * OpProbeTimeline, or submit additional timing information to an existing
 * Event. Whether a Probe should create a new Event or not is determined by
 * its 'key'. Data submitted under the same key will end up in the same
 * Event. This is necessary because some Events happen over multiple time
 * slices (e.g. thread evaluation), but should still appear as a single
 * in the timeline.
 *
 * OpPropertyProbes may produce Events which contain a certain number of
 * properties. Properties carry additional (and specific) information
 * about an Event, such as the type of a script thread. The max allowed
 * number of properties is defined when the OpPropertyProbe is activated.
 *
 * @see OpProbeTimeline::Property
 * @see OpPropertyProbe::Activator
 */
class OpPropertyProbe
{
public:

	/**
	 * An implementation of OpProbeTimeline::Event which can contain
	 * a certain number of Properties (decided compile time by MAX_PROPERTIES).
	 */
	template<unsigned MAX_PROPERTIES>
	class Event
		: public OpProbeTimeline::Event
	{
	public:

		/**
		 * Constructor. Sets the Event type, and initalizes the number of
		 * Properties to zero.
		 *
		 * @param type The type of the event.
		 */
		Event(OpProbeEventType type) : OpProbeTimeline::Event(type), m_size(0) {}

		/**
		 * @copydoc OpProbeTimeline::GetPropertyCount
		 */
		virtual unsigned GetPropertyCount() const { return m_size; }

		/**
		 * @copydoc OpProbeTimeline::GetProperty(const char*)
		 */
		virtual const OpProbeTimeline::Property *GetProperty(const char *name) const
		{
			for (unsigned i = 0; i < m_size; ++i)
				if (op_strcmp(name, m_properties[i].GetName()) == 0)
					return m_properties + i;

			return NULL;
		}

		/**
		 * @copydoc OpProbeTimeline::GetProperty(unsigned)
		 */
		virtual const OpProbeTimeline::Property *GetProperty(unsigned index) const
		{
			OP_ASSERT(index < m_size);
			return m_properties + index;
		}

		/**
		 * @copydoc OpProbeTimeline::GetProperties
		 */
		virtual const OpProbeTimeline::Property *GetProperties() const
		{
			return m_properties;
		}

		/**
		 * @copydoc OpProbeTimeline::AddProperty
		 */
		virtual void AddProperty(const OpProbeTimeline::Property &prop)
		{
			if (m_size >= MAX_PROPERTIES)
				return; // Silent fail.

			m_properties[m_size++] = prop;
		}

	private:

		// Storage for the Properties.
		OpProbeTimeline::Property m_properties[MAX_PROPERTIES];

		// The number of Properties we are actually using.
		unsigned m_size;

	}; // Event

	/**
	 * An Activator object is stack allocated and initialized by client code
	 * when a probe needs to activate.
	 *
	 * The object also contains functions for adding properties which the
	 * resulting event needs to carry. The event produced by this activation
	 * can at most contain the number of properties defined by the template
	 * parameter @c MAX_PROPERTIES.
	 *
	 * An Activator object makes error handling code significantly less
	 * painful, and guarantees that the OpProbeTimeline is left in a consistent
	 * state in all cases.
	 */
	template<unsigned MAX_PROPERTIES>
	class Activator
	{
	public:

		/**
		 * Create a new Activator for the specified probe.
		 *
		 * @param probe The probe to activate.
		 */
		Activator(OpPropertyProbe &probe)
			: m_probe(probe)
			, m_timeline(NULL)
			, m_start(0)
		{
		}

		/**
		 * Destructor. Reports that activation is complete, and that
		 * no more overhead will follow. The time that is spent after
		 * this call is the actual time used by the target function.
		 */
		~Activator()
		{
			if (m_timeline)
				m_timeline->EndOverhead(m_start);
		}

		/**
		 * Activate the probe. Activating a probe means that it will submit timing
		 * and hit count information to the OpProbeTimeline.
		 *
		 * @param timeline The OpProbeTimeline to activate the probe on. (Which
		 *                 OpProbeTimeline should receive the timing information).
		 * @param type The type of Event produced by this Probe.
		 * @param key The key for the Event produced by this probe activation.
		 * @return CREATED if a new Event was pushed to the activation stack;
		 *         REUSED if an existing Event with the specified type/key pair
		 *         was pushed to the activation stack; or OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS Activate(OpProbeTimeline *timeline, OpProbeEventType type, const void *key)
		{
			m_timeline = timeline;
			m_start = m_timeline->BeginOverhead();

			OP_STATUS status = Activate(type, key, m_start);

			if (OpStatus::IsSuccess(status))
				m_probe.Activate(m_timeline);

			return status;
		}

		/**
		 * Like the the general Activate function, but use the URL as the key
		 * instead of certain pointer value.
		 *
		 * @copydoc Activate(OpProbeTimeline*, OpProbeEventType, const void*)
		 */
		OP_STATUS Activate(OpProbeTimeline *timeline, OpProbeEventType type, URL *url)
		{
			OP_ASSERT(url);

			const uni_char *str;
			RETURN_IF_ERROR(timeline->GetString(url, str));

			return Activate(timeline, type, str);
		}

		/**
		 * Like the the general Activate function, but use the integer as the
		 * key instead of certain pointer value.
		 *
		 * @copydoc Activate(OpProbeTimeline*, OpProbeEventType, const void*)
		 */
		OP_STATUS Activate(OpProbeTimeline *timeline, OpProbeEventType type, int integer)
		{
			void *key = reinterpret_cast<void*>(static_cast<INTPTR>(integer));
			return Activate(timeline, type, key);
		}

		/**
		 * Add a string as a Property. The string will be copied and internalized
		 * by the timeline.
		 *
		 * @param name The name for the property.
		 * @param string The string to add as a property.
		 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS AddString(const char *name, const uni_char *string)
		{
			OP_ASSERT(m_timeline);

			const uni_char *pooled;
			RETURN_IF_MEMORY_ERROR(m_timeline->GetString(string, pooled));

			OpProbeTimeline::Property prop(name, pooled);

			m_timeline->GetTop()->AddProperty(prop);

			return OpStatus::OK;
		}

		/**
		 * @copydoc AddString(const char*, const uni_char*)
		 */
		OP_STATUS AddString(const char *name, const char *string)
		{
			OP_ASSERT(m_timeline);

			const uni_char *pooled;
			RETURN_IF_MEMORY_ERROR(m_timeline->GetString(string, pooled));

			OpProbeTimeline::Property prop(name, pooled);

			m_timeline->GetTop()->AddProperty(prop);

			return OpStatus::OK;
		}

		/**
		 * Add an integer as a property.
		 *
		 * @param name The name of the property.
		 * @param integer The integer to store in the property.
		 */
		void AddInteger(const char *name, int integer)
		{
			if (!m_timeline)
				return;

			OpProbeTimeline::Property prop(name, integer);

			m_timeline->GetTop()->AddProperty(prop);
		}

		/**
		 * @copydoc AddInteger(const char*, int)
		 */
		void AddUnsigned(const char *name, unsigned integer)
		{
			AddInteger(name, static_cast<int>(integer));
		}

		/**
		 * Add the string representation of a URL as property.
		 *
		 * @param name The name of the property.
		 * @param url The URL to convert to a string, then add as a property.
		 */
		OP_STATUS AddURL(const char *name, URL *url)
		{
			OP_ASSERT(m_timeline);

			const uni_char *pooled;
			RETURN_IF_MEMORY_ERROR(m_timeline->GetString(url, pooled));

			OpProbeTimeline::Property prop(name, pooled);

			m_timeline->GetTop()->AddProperty(prop);

			return OpStatus::OK;
		}

	private:

		/**
		 * Private helper function.
		 *
		 * @copydoc Activate(OpProbeTimeline*, OpProbeEventType, const void*)
		 */
		OP_STATUS Activate(OpProbeEventType type, const void *key, double start)
		{
			OP_STATUS status = m_timeline->Push(key, type);

			// Could not reuse a previous event ... create a new one.
			if (status == OpStatus::ERR)
				status = m_timeline->PushNew(key, OP_NEW(Event<MAX_PROPERTIES>, (type)), start);

			return status;
		}

		// The probe to activate.
		OpPropertyProbe &m_probe;

		// The timeline that owns the probe we are activating.
		OpProbeTimeline *m_timeline;

		// The at the beginning of the activation (before overhead).
		double m_start;
	};

	/**
	 * Constructor. Sets m_timeline to NULL. As long as m_timeline is NULL,
	 * nothing happens.
	 */
	OpPropertyProbe() : m_timeline(NULL) {}

	/**
	 * Destructor. Reports back to timeline, which in turns submits relevant
	 * timing information.
	 *
	 * If m_timeline is NULL (which will be the case during normal execution
	 * when profiling is not enabled), nothing happens.
	 */
	~OpPropertyProbe()
	{
		if (m_timeline)
			m_timeline->Pop();
	}

	/**
	 * The following function 'Activate' should have been private, but ADS
	 * does not implement nested classes properly.
	 */

// private:

	/**
	 * Activate the probe, such that the object *will* notify the timeline
	 * on probe destruction.
	 *
	 * This function is for internal use only. Do not use this to activate the
	 * probe; use an instance of the nested Activator class instead.
	 *
	 * @param timeline The timeline to report to.
	 */
	void Activate(OpProbeTimeline *timeline)
	{
		m_timeline = timeline;
	}

private:

	/// When not NULL, the Probe will submit timing information to this
	/// OpProbeTimeline on destruction. If NULL, nothing happens.
	OpProbeTimeline *m_timeline;
};

/**
 * A Probe for events which do not need to carry additional information
 * about the circumstances of the event (other than it's type).
 *
 * This class provides a convenience function ('Probe'), which creates
 * the Event (if necessary) while measuring overhead.
 */
class OpTypeProbe
{
public:

	/**
	 * Constructor. Sets m_timeline to NULL. As long as m_timeline is NULL,
	 * nothing happens.
	 */
	OpTypeProbe() : m_timeline(NULL) { }

	/**
	 * Destructor. Reports back to timeline, which in turns submits relevant
	 * timing information.
	 *
	 * If m_timeline is NULL (which will be the case during normal execution
	 * when profiling is not enabled), nothing happens.
	 */
	~OpTypeProbe()
	{
		if (m_timeline)
			m_timeline->Pop();
	}

	/**
	 * Activate the probe. Activating a probe means that it will submit timing
	 * and hit count information to the OpProbeTimeline.
	 *
	 * This function creates the timeline Event as needed, and keeps track of
	 * overhead while doing it.
	 *
	 * @param timeline The OpProbeTimeline to activate the probe on. (Which
	 *                 OpProbeTimeline should receive the timing information).
	 * @param type The type of Event produced by this Probe.
	 * @param key The key for the Event produced by this probe activation.
	 * @return CREATED if a new Event was pushed to the activation stack;
	 *         REUSED if an existing Event with the specified type/key pair
	 *         was pushed to the activation stack; or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS Activate(OpProbeTimeline *timeline, OpProbeEventType type, const void *key)
	{
		double start = timeline->BeginOverhead();

		OP_STATUS status = timeline->Push(key, type);

		// Could not reuse a previous event. Create a new one.
		if (status == OpStatus::ERR)
			status = timeline->PushNew(key, type, start);

		timeline->EndOverhead(start);
		RETURN_IF_MEMORY_ERROR(status);

		m_timeline = timeline;
		return status;
	}

private:
	/// When not NULL, the Probe will submit timing information to this
	/// OpProbeTimeline on destruction. If NULL, nothing happens.
	OpProbeTimeline *m_timeline;
};

#endif // SCOPE_PROFILER

#endif // PROBE_TIMELINE_H
