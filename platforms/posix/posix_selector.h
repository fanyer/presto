/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_SELECTOR_H
#define POSIX_SELECTOR_H
# ifdef POSIX_OK_SELECT
#include "modules/util/simset.h"
/** \file
 * Porting interface for file handle activity monitoring.
 *
 * Provides a g_posix_selector object (during the lifetime of the g_opera
 * object, when InitL has returned more recently than Destroy has been called)
 * which clients can ask to watch file handles, reporting when read or write and
 * any errors occur on each watched handle.  Handles can also be asynchronously
 * connect()ed before listening for read or write begins.
 */

/** Base-class for things that need to know when file I/O happens.
 *
 * See PosixSelector for details.  Registering a listener for any file handle
 * implicitly causes the SIGIO signal to be listened for (at least for the
 * simple and Qt implementations of PosixSelector).  The OnError and OnDetach
 * methods are pure virtual; all derived classes need to implement them.  The
 * other call-back methods have no-op default implementations, so derived
 * classes merely need to override the ones they're interested in.
 *
 * Note that more than one of OnReadReady(), OnWriteReady() and OnError() may be
 * called in connection with a single select() result-set.  It shall be safe for
 * any one of them to (however indirectly) call Detach(); no others shall
 * subsequently be called.  It shall be safe for them to call SetMode(); if this
 * turns off a mode that would otherwise have been about to receive a call-back,
 * that call-back shall be skipped.  See OnConnected() for its largely similar
 * lack of restrictions, aside from calls during Watch().
 *
 * Listener methods should not generally perform substantial or time-consuming
 * computation.  They are called from g_posix_selector->Poll(), not during any
 * call to g_opera->Run(); the call to Poll() has typically used up all the time
 * that the main loop could afford to let it use, so should return promptly, as
 * the main loop implicitly has some event it is now in a hurry to process.  The
 * natural way for a listener to handle anything which might prove
 * time-consuming is to post a message to the message queue and have the handler
 * that receives that message deal with the time-consuming operation, during a
 * call to g_opera->Run(), i.e. as part of Opera's processing of its message
 * queue.  In no event should any listener method call Poll().
 */
class PosixSelectListener
{
public:
	/** Destructor.
	 *
	 * This base class performs the necessary Detach() as part of deletion; this
	 * is harmless if this has already been Detach()ed, but does mean clients
	 * don't need to worry about Detach()ing in preparation for deletion (and
	 * doing so would waste a few CPU cycles).
	 */
	virtual ~PosixSelectListener();

#ifdef POSIX_OK_NETADDR
	/** Called when an asynchronous connect() has completed.
	 *
	 * If the listener is being Watch()ed in conjunction with an
	 * OpSocketAddress, its file descriptor is not checked for ReadReady or
	 * WriteReady until connect() is found to have completed.  At that point,
	 * OnConnected() shall be called and no further checks for connection shall
	 * be made; thereafter, the file descriptor shall be tested for read/write
	 * readiness as specified to Watch() or, subsequently, to SetMode().
	 *
	 * OnConnected() may be called during the initial call to Watch(): if this
	 * happens, it MUST NOT attempt to delete or detach the listener - it is not
	 * yet being watched - and any call to SetMode() shall be ignored.  Once
	 * Watch() has returned, later calls to OnConnected() may detach or delete
	 * the listener.
	 *
	 * @param fd File descriptor that's been connected.
	 */
	virtual void OnConnected(int fd) {}
#endif // POSIX_OK_NETADDR

	/** Called when read() or accept() would not block if called.
	 *
	 * For a listening socket, accept() takes the place of read(): select()
	 * reports "read ready" on a file handle on which an incoming connection
	 * request has been received.
	 *
	 * Note that read() or accept() may \em fail all the same; this only
	 * guarantees that it won't \em block.
	 *
	 * @param fd File descriptor that's ready to read.
	 */
	virtual void OnReadReady(int fd) {}

	/** Called when write() would not block if called.
	 *
	 * Note that write() may \em fail all the same; this only guarantees that it
	 * won't \em block.
	 *
	 * @param fd File descriptor that's ready to write
	 */
	virtual void OnWriteReady(int fd) {}

	/** Called when an error condition is noticed.
	 *
	 * If the listener were Watch()ed with a socket address, hence connecting,
	 * and has not yet seen OnConnected(), then the error is a connect() error
	 * (and the err parameter shall contain errno's value).  Otherwise, it's a
	 * select() error.
	 *
	 * In either case, it is probably sensible for the listener to detach itself
	 * from the PosixSelector (and close the file handle).  None the less, if it
	 * does not, the PosixSelector shall continue polling it (and, in all
	 * likelihood, calling OnError() on it).
	 *
	 * @param fd File descriptor that's seen an error.
	 * @param err Value of errno after error, if any.
	 */
	virtual void OnError(int fd, int err=0) = 0;

	/** Called when the PosixSelector stops using this listener.
	 *
	 * Only relevant when PosixSelector initiated (e.g. in its destructor) the
	 * end of listening.  The listener shall get no further call-backs, except
	 * possibly further calls to OnDetach if this listener is listening on
	 * several file descriptors.  It is, however, safe to delete this on any
	 * call to OnDetach(), not necessarily only the last.
	 *
	 * @param fd A file descriptor on which this listener has been listening.
	 */
	virtual void OnDetach(int fd) = 0;

	/** Detaches this instance from the g_posix_selector and closes the
	 * specified file descriptor.
	 * @param fd file descriptor to close.
	 * @return The result of the close() call. */
	int DetachAndClose(int fd);

protected:
	/** Detaches this instance from the g_posix_selector. Should be called
	 * before close()ing the associated file descriptor.
	 * @param fd Optionally only detach from listening to this file descriptor.
	 */
	void Detach(int fd = -1);
};

#define g_posix_selector (g_opera->posix_module.GetSelector())
/** Shared interface to select(), poll() and kindred systems.
 *
 * Platforms which use a tool-kit (e.g. Qt) which has its own select listening
 * infrastructure should implement this using a wrapper round that; otherwise,
 * activate API_POSIX_SIMPLE_SELECTOR to get a simple implementation, or devise
 * something that suits the custom requirements of your platform.
 *
 * Implementations of PosixSelector should be aware that listener call-backs are
 * apt to lead to calls to Detach(), whether on the listener whose method is
 * called or on other listeners.  This may make it necessary for Detach() to
 * know when such call-backs are in progress, so as to not actually delete
 * relevant objects but merely mark them for subsequent deletion, after the
 * call-back's return.  See DSK-254322 and DSK-265901.
 *
 * Note that PosixSelector::Button is provided by the base class; derived
 * classes need not concern themselves with it.  It uses PosixSelector only via
 * its public API.
 */
class PosixSelector
{
public:
	/** Trigger for actions on the core thread.
	 *
	 * Normal message-posting involves memory allocation, which might not be
	 * safe on a non-core thread or from within a message-handler.
	 * Consequently, code called from these contexts cannot safely post messages
	 * to the core thread.  By setting up (on the core thread) a Button object,
	 * they can use it as a channel via which to trigger calling a piece of code
	 * of their chosing (typically to post a message to the core message queue)
	 * on the core thread.  Derive a class from this one, implement an OnPressed
	 * that does the job you want, create an instance (on the core thread),
	 * check it's .Ready() and have your code running in restricted contexts
	 * Press() it.
	 */
	class Button : public ListElement<Button>
	{
		union {
			class ButtonGroup *m_group; ///< when m_channel is set
			OP_STATUS m_cause; ///< when m_channel is 0
		} d;
	public:
		const char m_channel; ///< identifier within 255-Button group.
		/** Constructor
		 *
		 * Note that Ready (q.v.) must be called after construction, to check
		 * success.
		 */
		Button();
		~Button(); ///< Destructor

		/** Check construction succeeded.
		 *
		 * This method should be treated as a second-stage constructor; the
		 * actual work has been done in the constructor, but this provides the
		 * means to check that it succeeded.  A Button must not be used unless
		 * Ready() returns success.
		 *
		 * @return OK precisely if this Button is ready for use;
		 * ERR_NULL_POINTER if Opera (or the posix module) is not suitably
		 * initialized (or has been shut down), other status codes according to
		 * OpStatus (q.v.).
		 */
		OP_STATUS Ready();

		/** Safe action in restricted contexts.
		 *
		 * Call this from a restricted context (e.g. signal handler or non-core
		 * thread) to cause the OnPressed method to be called on the core thread
		 * during PosixSelector's next bout of handling I/O events (typically on
		 * the next cycle of the event-loop).
		 *
		 * Note that several Press()es may lead to just one OnPressed(), if the
		 * core thread notices them at the same time.
		 *
		 * @return True if the button worked; else errno shall be set to
		 * indicate the error; ENODEV indicates that the button was not Ready
		 * (q.v.); otherwise, the error comes from write().
		 */
		bool Press();

		/** Method called on core thread shortly after the button is pressed.
		 *
		 * Each client should over-ride this method with its response to the
		 * button being pressed.
		 */
		virtual void OnPressed() {}

		/** Method called if detached other than by Destructor.
		 *
		 * If the Button's associated group fails for any reason, the button
		 * shall be detached by the group; it shall cease receiving OnPressed()
		 * call-backs after this.  Deleting the button will also detach it, but
		 * without calling this method.
		 */
		virtual void OnDetach() {}
	};

private:
	friend class PosixModule;

	/** Factory method.
	 *
	 * The g_opera->posix_module object's InitL() calls this to initialize
	 * g_posix_selector; platforms should implement this method to construct a
	 * suitable instance of the platform's class derived from PosixSelector.  If
	 * API_POSIX_SIMPLE_SELECTOR is imported, its implementation of
	 * PosixSelector takes care of this for you.
	 */
	static OP_STATUS Create(PosixSelector * &selector);

#ifdef POSIX_SELECTOR_SELECT_FALLBACK
	/** Internal utility.
	 *
	 * Factory method to create a posix selector based on select() if
	 * PosixSelector::Create() fails. This is used by PosixModule::Init() to
	 * detect at runtime if the default posix selector (which is created by
	 * PosixSelector::Create()) is available on the system. If it is not
	 * available, a SimplePosixSelector instance is created.
	 */
	static OP_STATUS CreateWithFallback(PosixSelector * &selector);
#endif

#ifdef POSIX_OK_NETADDR
protected:
	/** Internal utility.
	 *
	 * Connect()s a copy of given to the supplied file descriptor, returning the
	 * copy in posix on success, else reporting problems via listener.  Saves
	 * duplication in derived classes when over-riding Watch().
	 *
	 * @param posix Reference to (initially NULL) pointer to posix network address.
	 * @param given Address of the socket to be Connect()ed.
	 * @param fd File descriptor of an open socket, to be Watch()ed.
	 * @param listener Listener on which to report connection or any problems.
	 * @return See OpStatus; may OOM.
	 */
	static OP_STATUS PrepareSocketAddress(class PosixNetworkAddress *&posix,
										  const class OpSocketAddress *given,
										  int fd, PosixSelectListener *listener);

	/** Internal utility.
	 *
	 * Connect()s address given to the supplied file descriptor, reporting problems
	 * via listener. Saves duplication in derived classes when over-riding Poll().
	 *
	 * @param address References a pointer to the network address to which to
	 *                connect; this address will be OP_DELETE()'d and set to NULL
	 *                by this function on successful connection and on some errors
	 *                where trying again is pointless.
	 * @param fd File descriptor of an open socket, to be connected.
	 * @param listener Listener on which to report connection or any problems.
	 * @return See OpStatus; may OOM.
	 */
	static OP_STATUS ConnectSocketAddress(PosixNetworkAddress*& address,
										  int fd, PosixSelectListener *listener);
#endif // POSIX_OK_NETADDR

public:
	virtual ~PosixSelector() {}

	/** Provoke potentially-blocking check for activity.
	 *
	 * This method should normally be called by the platform code in its main
	 * loop, alternating with g_opera->Run() and kindred top-level activities.
	 * Particular implementations are at liberty to have the actual calls to
	 * select() or similar happen elsewhere; and, advertising this, spare their
	 * users from the need to include it in their main loop.  However, by
	 * default, it should participate in the main loop.
	 *
	 * No thought has been given in the design of Poll() to coping with being
	 * called other than from the main loop, although it is likely to work if
	 * called (sensibly) during g_opera->Run().  In any case, calling Poll()
	 * from within any PosixSelectListener method is unsupported, unsafe and
	 * likely to produce unwelcome results; if you think you need to do that,
	 * have the listener post a message and have the handler of that message do
	 * the subordinate call.
	 *
	 * Note that a window-management system may have a file-handle it uses for
	 * tracking when events happen; if so, that file-handle (e.g. obtained via
	 * XConnectionNumber(), for X11) should be Watch()ed to ensure its messages
	 * get noticed, triggering Poll() to return even if the associated listener
	 * doesn't have a call-back.  (In the case of X, however, the listener could
	 * perfectly sensibly package the usual XPending/XNextEvent/handle cycle.)
	 *
	 * Note that the timeout parameter is deliberately specified to be
	 * compatible with the return from Opera::Run(), albeit using a double to
	 * allow for implementations whose time granularity may be significantly
	 * finer than milliseconds (e.g. pselect uses nanoseconds).  Simply passing
	 * in the return-value from g_opera->Run() should normally be reasonable.
	 * However, the timeout parameter only specifies an upper bound; an
	 * implementation is entirely at liberty to return false sooner than the
	 * specified timeout (or, indeed, immediately), for example if it does its
	 * actual work somewhere else; this is, indeed, the base-class's
	 * implementation.  However, if timeout is negative, the implementation
	 * should take care to only return if a call to g_opera->Run() shall find
	 * something to do.
	 *
	 * Projects using GOGI have op_run_slice() in place of g_opera->Run(), but
	 * the semantics are exactly the same and this method can be
	 * called in a loop with it.
	 *
	 * Projects using Bream typically have an instance of BreamMessageLoop whose
	 * ThreadMain() is the nearest approximation to a message loop;
	 * BreamMessageThread is a standard implementation used for this purpose.
	 * You should replace it with a class, based on BreamMessageLoop and
	 * PosixSelector::Button, that implements the required APIs using
	 * g_posix_selector->Poll(); it should pass 0 as timeout each time it
	 * handles a message and a suitable timeout when it has no messages to
	 * handle.  It can use its Press() method (from Button) in its
	 * implementations of Stop() and OnPendingMessageChanged() to ensure that
	 * the Poll()ing thread notices when it should resume processing, since the
	 * very act of Press()ing will interrupt Poll(), even with the default
	 * implementation of OnPressed().
	 *
	 * @param timeout Indicates how long to wait if nothing interesting is
	 * ready; any negative value allows an indefinite wait; otherwise, the value
	 * is an upper bound measured in milliseconds.
	 * @return True precisely if something interesting is ready; relevant
	 * call-backs shall have been issued during the call to Poll and a trip
	 * round the message-loop is likely to find something to do.
	 */
	virtual bool Poll(double timeout) { return false; }

	/** Detach a listener, optionally from just one file handle.
	 *
	 * Note that these do not call the listener's OnDetach method; that's
	 * reserved for when PosixSelector initiated the detach.  Caller may call
	 * listener's OnDetach() after Detach()ing it, if appropriate.
	 *
	 * If no file descriptor is specified, all uses of the listener are
	 * detached; in particular, the listener base class's destructor calls this
	 * form of Detach(), so derived classes don't need to pay particular
	 * attention to ensuring they are detached when destroyed.  If a file
	 * descriptor is specified, all uses of the given listener for that file
	 * descriptor shall be detached (this behaviour shall change if someone
	 * proposes a reasonable use case).
	 *
	 * @param whom PosixSelectListener to be detached.
	 * @param fd Optionally only detach from listening to this file descriptor.
	 */
	virtual void Detach(const PosixSelectListener *whom, int fd) = 0;
	/**
	 * @overload
	 */
	virtual void Detach(const PosixSelectListener *whom) = 0;

	/** Type of listening to perform. */
	enum Type {
		NONE = 0,		///< Ignore (for now; may SetMode later)
		READ = 1,		///< Watch for incoming data.
		WRITE = 2,		///< Watch for readiness to deliver outgoing data.
		CONVERSE = 3	///< Watch for both of the above.
	};

	/** Revise the type of listening to use for a given listener.
	 *
	 * This is a no-op unless this listener has been passed to Watch() and not
	 * yet Detach()ed from all file-handles passed with it to Watch().  If the
	 * listener is being used for several file descriptors at the same time, and
	 * no file descriptor parameter is passed, the change is applied to all file
	 * descriptors for which it is listening.
	 *
	 * Note that setting mode to NONE is not the same as Detach()ing: it does
	 * not actually detach the listener, which shall still get OnError()
	 * call-backs, if any error is detected.  After a Detach(), if you want to
	 * resume watching, you need to call Watch(), specifying the file descriptor
	 * and interval.  After setting mode to NONE, however, you can resume
	 * watching simply by setting mode back to the desired type.  Likewise, you
	 * can Watch() with an initial mode of NONE and enable actual listening with
	 * a call to SetMode(), e.g. after you receive the OnConnected() message -
	 * which is delivered even when mode is NONE.  However, see OnConnected()
	 * for a restriction to this; SetMode() won't work if called during Watch().
	 *
	 * If a listener method, OnReadReady() or OnWriteReady(), does not actually
	 * read the data, accept the incoming connection or write data until the
	 * socket blocks, it is apt to be called repeatedly until it does so: it
	 * should not ignore such repeat calls but, instead, use SetMode() to
	 * transiently save the selector the need (to check whether it needs) to
	 * deliver them.  Failure to do this may significantly degrade performance.
	 * Once the data has been read, the incoming connection has been accepted or
	 * data has been written until the socket blocks, SetMode() should be used
	 * to re-enable relevant call-backs.
	 *
	 * @param listener The listener whose mode is to be changed.
	 * @param fd Optional file descriptor to which the change is to apply.
	 * @param mode Type of activity to watch for hereafter.
	 */
	virtual void SetMode(const PosixSelectListener * listener, int fd, Type mode) = 0;
	/**
	 * @overload
	 */
	virtual void SetMode(const PosixSelectListener * listener, Type mode) = 0;

	/** Begin watching a file descriptor.
	 *
	 * All file descriptors being Watch()ed are checked for errors.  Supplying a
	 * socket address shall cause PosixSelector to connect() the given file
	 * descriptor (in fact, to attempt this repeatedly; the POSIX APIs are
	 * ambiguous about the existence of any other way to determine when
	 * connect() has completed, if it fails to complete immediately).  Only
	 * after connect() has completed shall this file descriptor be checked for
	 * read or write readiness (but it still shall be checked for errors).
	 *
	 * The PosixSelector shall, from time to time, check for activity on all
	 * file handles it's Watch()ing.
	 *
	 * @param fd File descriptor.
	 * @param mode Type of activity to watch for (see enum Type).
	 * @param listener Entity to notify when activity is noticed.
	 * @param connect Optional OpSocketAddress for connecting, else NULL
	 * (ignored, but asserted NULL, unless API_POSIX_SOCKADDR is imported).
	 * @param accepts Must be true for a listen()ing socket that accept()s
	 * connections instead of reading data (needed on Mac); defaults false.
	 * @return See OpStatus.
	 */
	virtual OP_STATUS Watch(int fd, Type mode, PosixSelectListener *listener,
							const class OpSocketAddress *connect=0,
							bool accepts=false) = 0;
	/*
	 * @overload
	 * @deprecated
	 * @param interval Upper bound on number of milliseconds between checks.
	 */
	DEPRECATED(virtual OP_STATUS Watch(int fd, Type mode, unsigned long interval,
									   PosixSelectListener *listener,
									   const class OpSocketAddress *connect=0)) = 0;
};
# endif // POSIX_OK_SELECT
#endif // POSIX_SELECTOR_H
