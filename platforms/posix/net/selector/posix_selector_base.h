/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_SELECTOR_BASE_H
#define POSIX_SELECTOR_BASE_H

#include "platforms/posix/posix_selector.h"
#include "platforms/posix/posix_logger.h"

/** @brief Container for watching file descriptors.
 *
 * This container can be used by both SimplePosixSelector, EpollSelector and
 * KqueueSelector. This class stores internally three lists of #Watched file
 * descriptors:
 * - m_connecting: sockets that are not yet connected,
 * - m_watched: file descriptors that are watched by the selector,
 */
class PosixSelectorBase : public PosixSelector, protected PosixLogger
{
public:
	PosixSelectorBase() : PosixLogger(ASYNC) {}

	virtual ~PosixSelectorBase() = 0;

	// From PosixSelector:
	virtual bool Poll(double timeout);

	virtual void Detach(const PosixSelectListener *whom, int fd);
	virtual void Detach(const PosixSelectListener *whom) { PosixSelectorBase::Detach(whom, -1); }
	virtual void SetMode(const PosixSelectListener * listener, int fd, Type mode);
	virtual void SetMode(const PosixSelectListener * listener, Type mode) { PosixSelectorBase::SetMode(listener, -1, mode); }

	virtual OP_STATUS Watch(int fd, Type mode, PosixSelectListener *listener, const OpSocketAddress* connect, bool accepts);
	DEPRECATED(virtual OP_STATUS Watch(int fd, Type mode, unsigned long interval, PosixSelectListener *listener, const OpSocketAddress* connect));

public:
	class Watched : public ListElement<Watched>
	{
	private:
		unsigned int m_ref_count;
		bool m_is_active;
		~Watched();

	public:
		PosixSelectListener* m_listener;
		int m_fd;
		PosixSelector::Type m_mode;
		bool m_is_watched;
#ifdef POSIX_OK_NETADDR
		PosixNetworkAddress* m_address;
#endif // POSIX_OK_NETADDR
		Watched(PosixSelectListener* listener, int fd, PosixSelector::Type mode);

		bool IsWatched() const { return m_is_watched; }
		void SetWatched(bool toggle) { m_is_watched = toggle; }

		inline void IncRefCount() { ++m_ref_count; }
		inline void DecRefCount()
		{
			OP_ASSERT(m_ref_count > 0);
			if (0 == --m_ref_count) OP_DELETE(this);
		}
		inline void Destroy()
		{
			m_listener = NULL;
			m_is_active = false;
			DecRefCount();
		}
		inline bool IsActive() const { return m_is_active; }
	};

protected:
#ifdef POSIX_OK_NETADDR
	/** Check if a watched object is waiting to be connected.
	  * @param watched object to check
	  * @return Whether the object is waiting to be connected.
	  */
	bool IsConnecting(Watched* watched) const { return m_connecting.HasLink(watched); }

	/** Attempt to connect the specified item. */
	void TryConnecting(Watched* watched);
#endif // POSIX_OK_NETADDR

	/** Set of functions to override in implementation
	 * @note These functions have default implementations to allow them being
	 * called in ~PosixSelectorBase(). The destructor calls OnDetach() in
	 * the listeners, which might lead to these functions being called.
	 */

	/** Set read/write watching for an object being watched
	  * If the object wasn't being watched yet, start watching
	  * @param watched Watched object being changed
	  * @param mode Mode to set for @c watched
	  * @return See OpStatus.
	  */
	virtual OP_STATUS SetModeInternal(Watched* watched, Type mode) { return OpStatus::OK; }

	/** Effect a poll on the watched elements, returning if an element
	  * changed state or a timeout occurs.
	  * @param timeout if >= 0, timeout in ms; otherwise no timeout applies
	  * @return Whether a state changed that we were interested in
	  */
	virtual bool PollInternal(double timeout) { return false; }

	/** Detach a previously registered listener
	 * @param watched Object to detach
	 */
	virtual void DetachInternal(Watched* watched) {}

private:
	/** Empty list while calling OnDetach() on each listener. */
	void DetachListeners(List<Watched>& list);
	/** Drop 'listener' for 'fd' in collection 'from'.  */
	void Detach(List<Watched>& from, const PosixSelectListener* listener, int fd);
	/** Set listener mode. */
	void SetMode(List<Watched>& from, const PosixSelectListener* listener, int fd, Type mode);

#ifdef POSIX_OK_NETADDR
	/** Attempt to connect each item in m_connecting. */
	void TryConnecting();
	AutoDeleteList<Watched> m_connecting;
#endif
	AutoDeleteList<Watched> m_watched;
};

template<> inline void
OpAutoPtr<PosixSelectorBase::Watched>::DeletePtr(PosixSelectorBase::Watched* p)
{
	if (p) p->DecRefCount();
}

#endif // POSIX_EDGE_TRIGGERED_H
