/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "x11_opclipboard.h"

#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/unix/base/common/unixutils.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opmessageloop.h" // GetXTime()

#include "modules/pi/OpBitmap.h"

#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

using X11Types::Atom;

X11OpClipboard* X11OpClipboard::s_x11_op_clipboard = NULL;

namespace ClipboardUtil
{
	OP_STATUS GetAtoms(X11Types::Display* display, X11Types::Window window, X11Types::Atom property, OpVector<long>& list)
	{
		int format;
		X11Types::Atom type;
		unsigned long items;
		unsigned long bytes_left;
		unsigned char *data = 0;

		int rc = XGetWindowProperty(display, window, property, 0, INT_MAX, False, AnyPropertyType, &type,
									&format, &items, &bytes_left, &data);
		if (rc != Success || format != 32 || items == 0)
		{
			if (data)
				XFree(data);
			return OpStatus::ERR;
		}

		long* l = (long*)data;
		for (unsigned long i=0; i<items; i++)
		{
			long* value = OP_NEW(long,(l[i]));
			list.Add(value);
		}

		XFree( (char*)data );
		return OpStatus::OK;
	}

	INT32 GetPropertySizeInBytes(X11Types::Display *display, X11Types::Window window, X11Types::Atom property, int& format)
	{
		X11Types::Atom type;
		unsigned long length;
		unsigned long bytes_left;
		unsigned char *data = NULL;

		int rc = XGetWindowProperty(display, window, property, 0, 0, False, AnyPropertyType, &type,
									&format, &length, &bytes_left, &data);
		if (data)
			XFree( (char*)data );

		if (rc != Success || type == None)
			return -1;
		else
			return bytes_left;
	}

	OP_STATUS ConvertToXASTRING(OpStringC input, OpString8 *output)
	{
		OP_ASSERT(output);
		unsigned int len = input.Length();
		char * buf = output->Reserve(len);
		if (!buf)
			return OpStatus::ERR_NO_MEMORY;
		const uni_char * src = input.DataPtr();
		for (unsigned int i = 0; i < len; i++)
		{
			uni_char c = src[i];
			if ((c >= 0x20 && c <= 0x7e) ||
				(c >= 0xa0 && c <= 0xfe) || // Or should ff be included?
				c == 9 || c == 10) // Horizontal tab and line feed
				buf[i] = c;
			else
				buf[i] = '?';
		}
		buf[len] = 0;
		return OpStatus::OK;
	}
}


// static
void* X11ClipboardWorker::Init(void* data)
{
	static_cast<X11ClipboardWorkerData*>(data)->m_worker->Exec();
	return NULL;
}

void X11ClipboardWorker::Exec()
{
	if (pthread_detach(pthread_self()))
	{
		m_data.m_detach_failed = true;
		sem_post(&m_data.m_semaphore);
		pthread_exit(NULL);
	}

	m_display = XOpenDisplay(NULL);
	if (!m_display)
	{
		sem_post(&m_data.m_semaphore);
		pthread_exit(NULL);
	}

	XSetWindowAttributes attrs;
	attrs.border_pixel = BlackPixel(m_display, 0);
	m_data.m_window = XCreateWindow(
		m_display, RootWindow(m_display, 0), 0, 0,
		1, 1, 0, CopyFromParent, InputOutput, CopyFromParent, CWBorderPixel, &attrs);
	if (m_data.m_window == None)
	{
		XCloseDisplay(m_display);
		sem_post(&m_data.m_semaphore);
		pthread_exit(NULL);
	}

	// Signal main thread that we are ready. It will check m_data.m_window to see
	// if initialization was successful
	sem_post(&m_data.m_semaphore);

	int xsocket = ConnectionNumber(m_display);

	XEvent event;
	bool stopped = false;
	while (!stopped)
	{
		timeval sec_timeout = {1, 0};
		timeval* timeout = m_incremental_transfers.GetCount() > 0 ? &sec_timeout : NULL;

		fd_set readset;
		FD_ZERO(&readset);
		FD_SET(xsocket, &readset);
		FD_SET(m_data.m_pipe[0], &readset);

		int maxfd = MAX(m_data.m_pipe[0], xsocket);

		// Last call to HandleEvent() and HandleMessage() can have queued up messages that must be
		// delivered to the server before we call select()
		XFlush(m_display);

		select(maxfd+1, &readset, NULL, NULL, timeout);

		while (XPending(m_display))
		{
			XNextEvent(m_display, &event);
			HandleEvent(&event);
		}

		if (FD_ISSET(m_data.m_pipe[0],&readset))
		{
			char buf[100];
			for (int i=0; i<sizeof(buf)-1; i++)
			{
				int count = read(m_data.m_pipe[0], &buf[i], 1);
				if (count != 1 || buf[i] == 0)
				{
					buf[i]=0;
					stopped = HandleMessage(buf);
					break;
				}
			}
		}

		CleanupTransfers();
	}

	XDestroyWindow(m_display, m_data.m_window);
	XCloseDisplay(m_display);
	pthread_exit(NULL);
}


bool X11ClipboardWorker::HandleEvent(XEvent* event)
{
	// Events arrive asynchronously wrt to program flow in the main thread so we
	// protect shared variables with a mutex

	switch (event->type)
	{
		case PropertyNotify:
		{
			// Multipart transfers occur if the clipboard data is too large for a single transfer
			BOOL handled = FALSE;
			if (m_incremental_transfers.GetCount() > 0 && event->xproperty.state == PropertyDelete)
			{
				IncrementalTransfer* inc = FindTransfer(event->xproperty.window);
				if (inc && event->xproperty.atom == inc->property)
				{
					UINT32 bytes_left = inc->size - inc->offset;
					if (bytes_left > 0)
					{
						UINT32 bytes = MIN(inc->blocksize, bytes_left);
						XChangeProperty(event->xany.display, inc->window, inc->property, inc->target, inc->format,
										PropModeReplace, inc->data + inc->offset, bytes);
						inc->offset += bytes;
						inc->last_modified = time(0);
					}
					else
					{
						XChangeProperty(event->xany.display, inc->window, inc->property, inc->target, inc->format,
										PropModeReplace, inc->data, 0);
						m_incremental_transfers.Delete(inc);
					}
					handled = TRUE;
				}
			}
			return handled;
		}

		case SelectionClear:
		{
			if (g_startup_settings.debug_clipboard)
				printf("opera: Handle event: SelectionClear\n");

			// Another application has become the selection owner.
			// Clear our buffers. It is especially important to empty
			// the m_html buffer (DSK-320972)

			pthread_mutex_lock(&m_data.m_mutex);

			XSelectionClearEvent* e = (XSelectionClearEvent*)event;
			if (e->selection == XInternAtom(e->display, "CLIPBOARD", False))
			{
				m_data.m_text[1].Empty();
				m_data.m_image.Empty();
				m_data.m_html_text.Empty();
			}
			else if (e->selection == XA_PRIMARY)
				m_data.m_text[0].Empty();

			pthread_mutex_unlock(&m_data.m_mutex);

			// TODO: We should clear any selected text on XA_PRIMARY in opera as
			// the visible selection should be in sync with the actual owner. This
			// is pending because of lacking support in core/widgets code.

			break;
		}

		case SelectionNotify:
			if (g_startup_settings.debug_clipboard)
				printf("opera: Handle event: SelectionNotify\n");
			break;

		case SelectionRequest:
		{
			if (g_startup_settings.debug_clipboard)
				printf("opera: Handle event: SelectionRequest\n");

			pthread_mutex_lock(&m_data.m_mutex);

			const XSelectionRequestEvent& req = event->xselectionrequest;
			XEvent notify;
			notify.type                 = SelectionNotify;
			notify.xselection.display   = req.display;
			notify.xselection.requestor = req.requestor;
			notify.xselection.selection = req.selection;
			notify.xselection.target    = req.target;
			notify.xselection.property  = req.property;
			notify.xselection.time      = req.time;

			X11Types::Atom multiple_atom     = XInternAtom(req.display, "MULTIPLE", False);

			if (g_startup_settings.debug_clipboard)
			{
				Atom atoms[3] = { req.target, req.selection, req.property };
				char * names[3] = { NULL, NULL, NULL };
				XGetAtomNames(req.display, atoms, 3, names);
				if (names[0] && names[1] && names[2])
					printf("opera: Responding to clipboard SelectionRequest. target: %s selection: %s property: %s\n", names[0], names[1], names[2]);
				for (int i = 0; i < 3; i++)
					if (names[i])
						XFree(names[i]);
			}

			OpAutoVector<long> atoms;
			if (req.target == multiple_atom)
			{
				/* Multiple values: We get a list of pairs of (target,
				 * property) atoms in the request property.  We
				 * transfer the clipboard data according to each
				 * pair's target and property.  Then we update the
				 * request property by setting all 'property' elements
				 * where we failed to transfer the data to None.  And
				 * finally we send a RequestNotify to signal that we
				 * are done transferring the data.
				 */
				if (OpStatus::IsSuccess(ClipboardUtil::GetAtoms(req.display, req.requestor, req.property, atoms)))
				{
					if (g_startup_settings.debug_clipboard)
					{
						UINT32 atom_count = atoms.GetCount();
						for (UINT32 i=0; i<atom_count; i++ )
						{
							char * name = XGetAtomName(req.display, *atoms.Get(i));
							if (name)
								printf("opera: MULTIPLE %d %s\n", i, name);
							else
								printf("opera: MULTIPLE %d (%lu)\n", i, *(atoms.Get(i)));
						}
						char* name = XGetAtomName(req.display, req.property);
						if (name)
						{
							printf("opera: PROPERTY %s\n", name);
							XFree(name);
						}
					} // debug_clipboard

					notify.xselection.property = req.property;
					notify.xselection.target = req.target; // i.e. MULTIPLE
					long * multiple_data = OP_NEWA(long, atoms.GetCount());
					multiple_data[atoms.GetCount()-1] = None; // Just in case GetCount() isn't even
					for (UINT32 i=0; i+1<atoms.GetCount(); i+=2)
					{
						X11Types::Atom target   = *atoms.Get(i);
						X11Types::Atom property = *atoms.Get(i+1);

						multiple_data[i+1] = property;
						X11Types::Atom used_target = target;
						if (OpStatus::IsError(PutSelectionProperty(req.selection, target, property, req.display, req.requestor, &used_target)))
							multiple_data[i+1] = None;
						multiple_data[i] = used_target;
					}
					XChangeProperty(req.display, req.requestor, req.property, XA_ATOM, 32,
									PropModeReplace, reinterpret_cast<unsigned char*>(multiple_data),
									atoms.GetCount());
					XSendEvent(req.display, req.requestor, False, NoEventMask, &notify);
					OP_DELETEA(multiple_data);
				}
			}
			else
			{
				// Main case: Send a single value.
				X11Types::Atom target = req.target;
				X11Types::Atom property = req.property;
				X11Types::Atom used_target = None;
				if (OpStatus::IsSuccess(PutSelectionProperty(req.selection, target, property, req.display, req.requestor, &used_target)))
					notify.xselection.property = property;
				else
					notify.xselection.property = None;
				notify.xselection.target = used_target;
				XSendEvent(req.display, req.requestor, False, NoEventMask, &notify);

			}

			pthread_mutex_unlock(&m_data.m_mutex);
		}
		break;

		default:
			return FALSE;
		break;
	}

	return TRUE;
}


bool X11ClipboardWorker::HandleMessage(const char* message)
{
	bool stop = false;

	// This function is called when a message arrives from the opera thread. The opera
	// thread will not access shared data until after sem_post() is called. We do not
	// process X11 events in the worker thread when HandleMessage() is called, but we
	// can call code that will trigger events to be sent to the worker thread. Those
	// events are dealt with using HandleEvent() locally (that is, not from X11ClipboardWorker::Exec())
	//
	// In short: We should not have to use m_data.m_mutex here

	if (g_startup_settings.debug_clipboard)
		printf("opera: Handle clipboard message: %s\n", message);

	if (!strcmp(message, "exit"))
	{
		X11Types::Atom atom = XInternAtom(m_display, "CLIPBOARD", False);
		if (OwnsSelection(atom))
		{
			X11Types::Atom clipboard_manager_atom = XInternAtom(m_display, "CLIPBOARD_MANAGER", False);
			X11Types::Window clipboard_manager = XGetSelectionOwner(m_display, atom);
			if (clipboard_manager != None)
			{
				int num_targets = 0;
				long targets[10];

				X11Types::Atom save_targets_atom = XInternAtom(m_display, "SAVE_TARGETS", False);
				X11Types::Atom property_atom     = XInternAtom(m_display, "OPERA_CLIPBOARD", False);

				if (m_data.m_text[0].HasContent() || m_data.m_text[1].HasContent())
				{
					targets[num_targets++] = XInternAtom(m_display, "UTF8_STRING", False);
					targets[num_targets++] = XA_STRING;
				}
				if (HasTextHTML())
					targets[num_targets++] = XInternAtom(m_display, "text/html", False);

				OP_ASSERT(num_targets <= 10);

				XChangeProperty(m_display, m_data.m_window, property_atom, XA_ATOM, 32,
								PropModeReplace, (const unsigned char *)targets, num_targets);
				XConvertSelection(m_display, clipboard_manager_atom,
								  save_targets_atom, property_atom, m_data.m_window, X11Constants::XCurrentTime);

				TimeLimit time_limit(5);
				WaitForSelectionNotify(time_limit);
			}
		}
		sem_post(&m_data.m_semaphore);
		stop = true;
	}
	else if (!strcmp(message, "acquire clipboard"))
	{
		X11Types::Atom atom = XInternAtom(m_display, "CLIPBOARD", False);
		XSetSelectionOwner(m_display, atom, m_data.m_window, X11OpMessageLoop::Self()->GetXTime());
		sem_post(&m_data.m_semaphore);
	}
	else if (!strcmp(message, "acquire primary"))
	{
		X11Types::Atom atom = XA_PRIMARY;
		XSetSelectionOwner(m_display, atom, m_data.m_window, X11OpMessageLoop::Self()->GetXTime());
		sem_post(&m_data.m_semaphore);
	}
	else if (!strcmp(message, "set clipboard"))
	{
		m_data.m_is_mouse_selection = false;
		sem_post(&m_data.m_semaphore);
	}
	else if (!strcmp(message, "set primary"))
	{
		m_data.m_is_mouse_selection = true;
		sem_post(&m_data.m_semaphore);
	}
	else if (!strcmp(message, "empty"))
	{
		X11Types::Atom atom = XA_PRIMARY;
		XSetSelectionOwner(m_display, atom, m_data.m_window, X11OpMessageLoop::Self()->GetXTime());
		atom = XInternAtom(m_display, "CLIPBOARD", False);
		XSetSelectionOwner(m_display, atom, m_data.m_window, X11OpMessageLoop::Self()->GetXTime());
		m_data.m_text[0].Empty();
		m_data.m_text[1].Empty();
		m_data.m_html_text.Empty();
		m_data.m_image.Empty();
		sem_post(&m_data.m_semaphore);
	}
	else if (!strcmp(message, "has text"))
	{
		X11Types::Atom atom = GetCurrentClipboard();
		if (OwnsSelection(atom))
		{
			m_data.m_has_text = m_data.m_text[atom == XA_PRIMARY ? 0 : 1].HasContent() ? 1 : 0;
		}
		else
		{
			// Prevent calling the potential expensive GetTarget() when we do not have to. Opera code
			// will test for text presence at least twice when opening a menu.
			X11Types::Time now = X11OpMessageLoop::Self()->GetXTime();
			if (m_remote_test_time != now)
			{
				m_remote_test_time = now;
				m_remote_has_data = -1;
			}

			if (m_remote_has_data == -1)
			{
				// Use short timeout. It can block
				// Assume selection owner holds data on timeout so that we can try to paste.
				TimeLimit time_limit(2);
				if (GetTarget(FALSE, time_limit) != None && !time_limit.HasTimedOut())
					m_remote_has_data = 1;
				else
					m_remote_has_data = 0;
			}
			m_data.m_has_text = m_remote_has_data;
		}

		sem_post(&m_data.m_semaphore);
	}
	else if (!strcmp(message, "get text"))
	{
		if (m_data.m_target)
			GetText(*m_data.m_target);
		sem_post(&m_data.m_semaphore);
	}
	else
	{
		sem_post(&m_data.m_semaphore);
	}

	return stop;
}


// This function can only be used in a block protected by 'm_data.m_mutex' or in a call stack where HandleMessage() is an ancestor
OP_STATUS X11ClipboardWorker::PutSelectionProperty(X11Types::Atom selection, X11Types::Atom target, X11Types::Atom property, X11Types::Display* display, X11Types::Window window, X11Types::Atom* ret_target_used)
{
	// In most cases, we will convert according to the requested target.
	*ret_target_used = target;
	const char * atom_names[] = {
		"SAVE_TARGETS",
		"TARGETS",
		"UTF8_STRING",
		"text/html",
		"image/bmp",
		"MULTIPLE",
		"CLIPBOARD",
		"TEXT",
		"COMPOUND_TEXT",
		"INCR"
		// We don't yet support the TIMESTAMP target.  That should be fixed.
		//"TIMESTAMP"
	};
	const int atom_count = ARRAY_SIZE(atom_names);
	X11Types::Atom atoms[atom_count];
	if (XInternAtoms(m_display, const_cast<char**>(atom_names), atom_count, False, atoms) == 0)
		return OpStatus::ERR;
	X11Types::Atom save_targets_atom = atoms[0];
	X11Types::Atom targets_atom      = atoms[1];
	X11Types::Atom utf8_atom         = atoms[2];
	X11Types::Atom html_atom         = atoms[3];
	X11Types::Atom bmp_atom          = atoms[4];
	X11Types::Atom multiple_atom     = atoms[5];
	X11Types::Atom clipboard_atom    = atoms[6];
	X11Types::Atom text_atom         = atoms[7];
	X11Types::Atom compound_atom     = atoms[8];
	X11Types::Atom incr_atom         = atoms[9];
	// We don't yet support the TIMESTAMP target.  That should be fixed.
	//X11Types::Atom timestamp_atom    = atoms[smth];
	OP_ASSERT(atom_count > 9);

	if (target == targets_atom)
	{
		// Tell requesting application what kind of data we can provide

		int num_targets = 0;
		long targets[10]; // long for compatibility with 64-bit systems

		targets[num_targets++] = targets_atom;

		if (selection == clipboard_atom)
			targets[num_targets++] = save_targets_atom;

		if (m_data.m_text[0].HasContent() || m_data.m_text[1].HasContent())
		{
			targets[num_targets++] = utf8_atom;
			targets[num_targets++] = XA_STRING;
		}
		if (HasTextHTML())
			targets[num_targets++] = html_atom;
		if (m_data.m_image.m_buffer)
			targets[num_targets++] = bmp_atom;

		targets[num_targets++] = multiple_atom;
		// We don't yet support the TIMESTAMP target.  That should be fixed.
		//targets[num_targets++] = timestamp_atom;

		if (g_startup_settings.debug_clipboard)
		{
			printf("opera: Replying supported targets: ");
			for (int i=0; i<num_targets; i++)
			{
				char* name = XGetAtomName(display, targets[i]);
				printf( "%s'%s'", i > 0 ? ", " :"", name ? name : "<null>");
				if (name)
					XFree(name);
			}
			printf("\n");
		}

		XChangeProperty(m_display, window, property, XA_ATOM, 32,
						PropModeReplace, reinterpret_cast<unsigned char*>(targets), num_targets);
		OP_ASSERT(num_targets < 10); // targets[] is 10 elements long.
		return OpStatus::OK;
	}


	OpString8 text;
	unsigned char* data = 0;
	UINT32 size_in_bytes = 0;

	// Step 1: Collect data and size. TODO: We should have some sort of mime object here that are common for all types
	if (target == bmp_atom)
	{
		data = reinterpret_cast<unsigned char *>(m_data.m_image.m_buffer);
		size_in_bytes = m_data.m_image.m_size;
	}
	else
	{
		if (target == utf8_atom)
			RETURN_IF_ERROR(text.SetUTF8FromUTF16(GetSelectionContent(selection)));
		else if (target == html_atom)
			RETURN_IF_ERROR(text.Set(m_data.m_html_text));
		else
		{
			if (target == XA_STRING || target == text_atom || target == compound_atom)
			{
				/* We convert all of these to a subset of iso-8859-1,
				 * which is more or less correct for STRING, and
				 * probably valid but rather suboptimal for TEXT and
				 * COMPOUND_TEXT.
				 *
				 * Currently, unrepresentable characters are replaced.
				 * It would probably be more correct to fail
				 * conversion if an unrepresentable character is
				 * encountered, but I (eirik) suspect that this will
				 * break more than it will fix.
				 *
				 * But these types are not in much use any more.
				 *
				 *
				 * The ICCCM says:
				 *
				 * STRING should be iso-8859-1 (+TAB+NEWLINE, but no
				 * other control characters).  So we should probably
				 * remove all control characters (except TAB and
				 * LINEFEED) and somehow eliminate all characters not
				 * in iso-8859-1 for this one.
				 *
				 * TEXT shall never be returned as a type.  Instead we
				 * should pick whatever encoding we like and send that
				 * back, using the appropriate type for that encoding.
				 * (So maybe we should just treat this as UTF8_STRING,
				 * really.  But that would probably break all
				 * applications for which it is useful that we handle
				 * TEXT anyway.)
				 *
				 * COMPOUND_TEXT is described in its own document
				 * called "Compound text encoding".  It seems to be a
				 * restricted variant of iso-2022, and rather nasty to
				 * deal with for very little gain.  However, "any
				 * legal instance of a STRING type (as defined in the
				 * ICCCM) is also a legal instance of type COMPOUND
				 * TEXT."  So maybe we should just use that.
				 *
				 * A solution for all three types could be to do a
				 * proper conversion to iso-8859-1 and remove all
				 * control characters except HT and LF.  I don't think
				 * we want to properly deal with COMPOUND_TEXT, and I
				 * suspect that most applications where handling
				 * STRING or TEXT is of any use whatsoever won't deal
				 * well with data not in its own encoding anyway.  The
				 * only remaining question is whether the presence of
				 * any non-iso-8859-1 characters should cause us to
				 * fail the conversion rather than send mangled data.
				 *
				 * The downside of changing this is that we don't know
				 * what will break, and we know of nothing that it
				 * will fix.
				 */
				RETURN_IF_ERROR(ClipboardUtil::ConvertToXASTRING(GetSelectionContent(selection), &text));
				if (target == text_atom)
					*ret_target_used = XA_STRING;
			}
			else
			{
				if (g_startup_settings.debug_clipboard)
				{
					char* name = XGetAtomName(display, target);
					printf("opera: Unsupported target request: %s\n", name);
					XFree(name);
				}
				/* If we send a failure notification or a success
				 * notification with empty data, some clients (eg
				 * gvim) will request a different target.  So trying
				 * to be "helpful" by falling back to simpler text
				 * formats (e.g. plain ascii) is counterproductive
				 * here.
				 */
				return OpStatus::ERR;
			}
		}

		data = reinterpret_cast<unsigned char*>(text.CStr());
		size_in_bytes = text.Length();
	}

	// Step 2: Set property on requester window. If data buffer is too large we will start an incremental transfer
	UINT32 max_selection_size_in_bytes = g_x11->GetMaxSelectionSize(display);
	if (size_in_bytes > max_selection_size_in_bytes)
	{
		IncrementalTransfer* inc = FindTransfer(window);
		if (inc)
		{
			if (g_startup_settings.debug_clipboard)
				printf("opera: Can not provide data to window 0x%08X. A transfer is already active\n", static_cast<unsigned int>(window));
			return OpStatus::ERR;
		}

		// This creates a new instance of 'inc'
		RETURN_IF_ERROR(AddTransfer(data, size_in_bytes, inc));

		inc->offset = 0;
		inc->blocksize = max_selection_size_in_bytes;
		inc->format = 8;
		inc->last_modified = time(0);
		inc->property = property;
		inc->target = target;
		inc->window = window;

		long bytes = inc->size;
		XSelectInput(display, window, PropertyChangeMask);
		XChangeProperty(display, window, property,
						incr_atom, 32, PropModeReplace, reinterpret_cast<unsigned char *>(&bytes), 1);
	}
	else
	{
		XChangeProperty(display, window, property, target, 8, PropModeReplace, data, size_in_bytes);
	}

	return OpStatus::OK;
}


bool X11ClipboardWorker::OwnsSelection(X11Types::Atom atom)
{
	// m_data.m_window is only set in worker process
	return XGetSelectionOwner(m_display, atom) == m_data.m_window;
}


X11Types::Atom X11ClipboardWorker::GetCurrentClipboard()
{
	// m_data.m_is_mouse_selection is only set in worker process
	return m_data.m_is_mouse_selection ? XA_PRIMARY : XInternAtom(g_x11->GetDisplay(), "CLIPBOARD", False);
}

// This function can only be used in a block protected by 'm_data.m_mutex' or in a call stack where HandleMessage() is an ancestor
X11Types::Atom X11ClipboardWorker::GetTarget(BOOL allow_fallback, TimeLimit& time_limit)
{
	X11Types::Atom utf8_atom     = XInternAtom(m_display, "UTF8_STRING", False);
	X11Types::Atom compound_atom = XInternAtom(m_display, "COMPOUND_TEXT", False);
	X11Types::Atom text_atom     = XInternAtom(m_display, "TEXT", False);
	X11Types::Atom targets_atom  = XInternAtom(m_display, "TARGETS", False);

	// Get list of text types the selection owner provides. This function performs a busy wait and will block
	X11Types::Atom prop = GetProperty(targets_atom, time_limit);
	if (prop == None)
		return None;

	// The list of text types we can parse. Ordered in rising relevance.
	X11Types::Atom preferred_targets[] = { /*fallback*/utf8_atom, text_atom, XA_STRING, compound_atom, utf8_atom };
	int found = -1;

	X11Types::Atom type;
	unsigned long items;
	unsigned long remaining;
	int format;
	unsigned char* data=0;
	// We query atoms of AnyPropertyType. We used to use XA_ATOM but some clients will set
	// other types (pwsafe sets TARGETS - DSK-322419).
	int rc = XGetWindowProperty(m_display, m_data.m_window, prop, 0, INT_MAX, False, AnyPropertyType, &type, &format, &items, &remaining, &data);
	if (rc == Success && data && type != None && format == 32)
	{
		/* 32-bit properties are stored as arrays of 'unsigned long',
		 * even if unsigned long is not 32 bits.
		 */
		unsigned long * data_long = reinterpret_cast<unsigned long*>(data);
		if (g_startup_settings.debug_clipboard)
			printf("opera: Supported clipboard types: ");

		for (unsigned long i=0; i<items; i++)
		{
			if (g_startup_settings.debug_clipboard)
			{
				Atom atom = static_cast<Atom>(data_long[i]);
				char * name = XGetAtomName(m_display, atom);
				if (name)
				{
					printf("%s ", name);
					XFree(name);
				}
				else
					printf("(%lu) ", data_long[i]);
			}
			for (int j=4; j>0; j--)
			{
				Atom atom = static_cast<Atom>(data_long[i]);
				if (atom == preferred_targets[j] && (found == -1 || j > found))
				{
					found = j;
					break;
				}
			}
		}

		if (g_startup_settings.debug_clipboard)
		{
			printf("\n");
			if (found == -1)
			{
				if (!allow_fallback)
					printf("opera: No preferred type found. Can not paste.\n");
				else
				{
					char* name = XGetAtomName(m_display, preferred_targets[0]);
					printf("opera: No preferred type found. Using %s as fallback.\n", name ? name : "<unknown>" );
					if (name)
						XFree(name);
				}
			}
			else
			{
				char* name = XGetAtomName(m_display, preferred_targets[found]);
				printf("opera: Selection type %s found to be supported. Using that one.\n", name ? name : "<unknown>" );
				if (name)
					XFree(name);
			}
		}
	}

	if (data)
		XFree(data);

	XDeleteProperty(m_display, m_data.m_window, prop);

	if (found == -1)
	{
		if (!allow_fallback)
			return None;
		found = 0;
	}

	return preferred_targets[found];
}

X11Types::Atom X11ClipboardWorker::GetProperty(X11Types::Atom target, TimeLimit& time_limit)
{
	X11Types::Atom opera_clipboard = XInternAtom(m_display, "OPERA_CLIPBOARD", False);
	if (opera_clipboard == None)
		return None;

	XConvertSelection(m_display, GetCurrentClipboard(), target, opera_clipboard,
					  m_data.m_window, X11Constants::XCurrentTime);

	return WaitForSelectionNotify(time_limit);
}


X11Types::Atom X11ClipboardWorker::WaitForSelectionNotify(TimeLimit& time_limit)
{
	timeval start;
	gettimeofday(&start, 0);
	timeval now = start;
	timeval end = { now.tv_sec + time_limit.GetSeconds(), 0 };

	XEvent event;
	int xsocket = XConnectionNumber(m_display);

	bool done = false;
	while (!done)
	{
		// SelectionRequest: Allow processing selection requests from opera itself
		// because we will copy via the protocol even when opera owns the selection
		// PropertyNotify: To handle multi transfer requests in OnExit()

		while (!done && XPending(m_display))
		{
			if (XCheckTypedEvent(m_display, SelectionRequest, &event) ||
				XCheckTypedEvent(m_display, PropertyNotify, &event))
				HandleEvent(&event);
			else if (XCheckTypedEvent(m_display, SelectionNotify, &event))
				done = true;
		}

		if (!done)
		{
			gettimeofday(&now, 0);
			if (now.tv_sec > end.tv_sec)
			{
				time_limit.SetTimedOut();
				if (g_startup_settings.debug_clipboard)
					printf("opera: Timeout while waiting for clipboard SelectionNotify\n");
				return None;
			}

			fd_set readset;
			FD_ZERO(&readset);
			FD_SET(xsocket, &readset);
			timeval timeout = { 1, 0 };
			select(xsocket+1, &readset, NULL, NULL, &timeout);
		}
	}

	return event.xselection.property;
}

// This function can only be used in a block protected by 'm_data.m_mutex' or in a call stack where HandleMessage() is an ancestor
OP_STATUS X11ClipboardWorker::GetText(OpString& text)
{
	text.Empty();

	OP_STATUS result = OpStatus::ERR;

	TimeLimit time_limit(5); // We allow a fallback so timeout is ignored should it happen
	X11Types::Atom target = GetTarget(TRUE, time_limit);
	for (int attempt=0; attempt<2 && target != None; attempt++)
	{
		TimeLimit time_limit(5);
		X11Types::Atom prop = GetProperty(target, time_limit);
		if (prop == None)
			return OpStatus::ERR;

		int format;
		X11Types::Atom type;
		unsigned long items, remaining;
		unsigned char* data = NULL;
		int rc = XGetWindowProperty(m_display, m_data.m_window, prop, 0, INT_MAX, False, target, &type, &format, &items, &remaining, &data);
		if (rc == Success)
		{
			if (type == target)
			{
				X11Types::Atom utf8_atom = XInternAtom(m_display, "UTF8_STRING", False);
				X11Types::Atom compound_atom = XInternAtom(m_display, "COMPOUND_TEXT", False);

				target = None;
				if (format == 8 && type == XA_STRING)
				{
					result = text.Set((const char *)data);
				}
				else if (format == 8 && type == compound_atom)
				{
					// Hmmmm... is this right?
					result = text.Set((const char *)data);
				}
				else if (format == 8 && type == utf8_atom)
				{
					result = text.SetFromUTF8((const char *)data);
				}
				else if (g_startup_settings.debug_clipboard)
				{
					printf("opera: Clipboard type not supported.\n");
				}
			}
			else
			{
				if (attempt == 0)
				{
					target = type;
				}
				else if (g_startup_settings.debug_clipboard)
					printf("opera: No matched clipboard type after second attempt.\n");
			}
			if (data)
				XFree(data);
		}
		XDeleteProperty(m_display, m_data.m_window, prop);
	}
	return result;
}

// This function can only be used in a block protected by 'm_data.m_mutex' or in a call stack where HandleMessage() is an ancestor
OpString& X11ClipboardWorker::GetSelectionContent(X11Types::Atom atom)
{
	return m_data.m_text[ atom == XA_PRIMARY ? 0 : 1];
}

// This function can only be used in a block protected by 'm_data.m_mutex' or in a call stack where HandleMessage() is an ancestor
bool X11ClipboardWorker::HasTextHTML()
{
	// HTML text can only be copied from the CLIPBOARD buffer (DSK-322250)
	return !m_data.m_is_mouse_selection && m_data.m_html_text.HasContent();
}


void X11ClipboardWorker::CleanupTransfers()
{
	UINT32 count = m_incremental_transfers.GetCount();
	if (count > 0)
	{
		time_t now = time(0);
		for (UINT32 i=0; i<count; i++)
		{
			IncrementalTransfer* inc = m_incremental_transfers.Get(i);
			if (now-inc->last_modified >= 5)
			{
				m_incremental_transfers.Delete(i);
				count--;
			}
			else
				i++;
		}
	}
}


OP_STATUS X11ClipboardWorker::AddTransfer(unsigned char* data, UINT32 size, IncrementalTransfer*& inc)
{
	inc = OP_NEW(IncrementalTransfer,());
	if (!inc || OpStatus::IsError(inc->Set(data, size)) || OpStatus::IsError(m_incremental_transfers.Add(inc)))
	{
		OP_DELETE(inc);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}


IncrementalTransfer* X11ClipboardWorker::FindTransfer(X11Types::Window window)
{
	for (UINT32 i=0; i<m_incremental_transfers.GetCount(); i++)
	{
		IncrementalTransfer* candidate = m_incremental_transfers.Get(i);
		if (candidate->window == window)
			return candidate;
	}

	return NULL;
}




X11OpClipboard::X11OpClipboard()
	: m_is_mouse_selection(false)
{
}


X11OpClipboard::~X11OpClipboard()
{
	s_x11_op_clipboard = NULL;
}



OP_STATUS X11OpClipboard::WriteCommand(OpStringC8 cmd)
{
	int length = cmd.Length();
	if (length > 0)
	{
		int len = write(m_data.m_pipe[1], cmd.CStr(), length+1);
		if (len != length+1)
		{
			if (g_startup_settings.debug_clipboard)
				printf("opera: Write error: %s\n", strerror(errno));
			return OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}


OP_STATUS X11OpClipboard::WaitForSemaphore(unsigned int timeout_in_seconds)
{
	if (timeout_in_seconds > 0)
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		struct timespec ts;
		ts.tv_sec = tv.tv_sec + timeout_in_seconds;
		ts.tv_nsec = tv.tv_usec * 1000;
		if (sem_timedwait(&m_data.m_semaphore, &ts))
		{
			if (g_startup_settings.debug_clipboard)
				printf("opera: Semaphore timeout: %s\n", strerror(errno));
			return OpStatus::ERR;
		}
	}
	else
	{
		if (sem_wait(&m_data.m_semaphore))
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}


OP_STATUS X11OpClipboard::Init()
{
	m_data.m_has_text = -1;

	if (sem_init(&m_data.m_semaphore, 0, 0))
	{
		if (g_startup_settings.debug_clipboard)
			printf("opera: Failed to initialize semaphore: %s\n", strerror(errno));
		return OpStatus::ERR;
	}

	if (pthread_mutex_init(&m_data.m_mutex, NULL))
	{
		if (g_startup_settings.debug_clipboard)
			printf("opera: Failed to initialize mutex\n");
		sem_destroy(&m_data.m_semaphore);
		return OpStatus::ERR;
	}

	m_data.m_worker = OP_NEW(X11ClipboardWorker,(m_data));
	if (!m_data.m_worker)
	{
		sem_destroy(&m_data.m_semaphore);
		pthread_mutex_destroy(&m_data.m_mutex);
		return OpStatus::ERR_NO_MEMORY;
	}

	if (pipe(m_data.m_pipe))
	{
		if (g_startup_settings.debug_clipboard)
			printf("opera: Failed to create pipe: %s\n", strerror(errno));
		OP_DELETE(m_data.m_worker);
		m_data.m_worker = NULL;
		sem_destroy(&m_data.m_semaphore);
		pthread_mutex_destroy(&m_data.m_mutex);
		return OpStatus::ERR;
	}

	int flags = fcntl(m_data.m_pipe[0], F_GETFL, 0);
	if (flags == -1 || fcntl(m_data.m_pipe[0], F_SETFL, flags | O_NONBLOCK))
	{
		if (g_startup_settings.debug_clipboard)
			printf("opera: Failed to initialize pipe\n");
		close(m_data.m_pipe[0]);
		close(m_data.m_pipe[1]);
		OP_DELETE(m_data.m_worker);
		m_data.m_worker = NULL;
		sem_destroy(&m_data.m_semaphore);
		pthread_mutex_destroy(&m_data.m_mutex);
		return OpStatus::ERR;
	}

	int rc = pthread_create(&m_data.m_worker_id, NULL, X11ClipboardWorker::Init, &m_data);
	if (rc)
	{
		if (g_startup_settings.debug_clipboard)
			printf("opera: Failed to create thread\n");
		close(m_data.m_pipe[0]);
		close(m_data.m_pipe[1]);
		OP_DELETE(m_data.m_worker);
		m_data.m_worker = NULL;
		sem_destroy(&m_data.m_semaphore);
		pthread_mutex_destroy(&m_data.m_mutex);
		return OpStatus::ERR;
	}

	// Worker thread will signal if there is an error or after window has been created
	OpStatus::Ignore(WaitForSemaphore(10));

	if (m_data.m_detach_failed)
	{
		if (g_startup_settings.debug_clipboard)
			printf("opera: Failed to detach thread\n");
		void *status;
		(void)pthread_join(m_data.m_worker_id, &status);
		close(m_data.m_pipe[0]);
		close(m_data.m_pipe[1]);
		OP_DELETE(m_data.m_worker);
		m_data.m_worker = NULL;
		sem_destroy(&m_data.m_semaphore);
		pthread_mutex_destroy(&m_data.m_mutex);
		return OpStatus::ERR;
	}

	if (m_data.m_window == None)
	{
		if (g_startup_settings.debug_clipboard)
			printf("opera: Failed to initialize thread\n");
		close(m_data.m_pipe[0]);
		close(m_data.m_pipe[1]);
		OP_DELETE(m_data.m_worker);
		m_data.m_worker = NULL;
		sem_destroy(&m_data.m_semaphore);
		pthread_mutex_destroy(&m_data.m_mutex);
	}

	return OpStatus::OK;
}



BOOL X11OpClipboard::HasText()
{
	RETURN_IF_ERROR(WriteCommand("has text"));
	OpStatus::Ignore(WaitForSemaphore(10));

	// 'm_data.m_has_text' is only set in worker thread when it reponds to the command above
	return m_data.m_has_text == 1;
}

OP_STATUS X11OpClipboard::PlaceText(const uni_char *text, UINT32 token)
{
	X11Types::Atom atom = GetCurrentClipboard();

	pthread_mutex_lock(&m_data.m_mutex);

	if (atom != XA_PRIMARY)
	{
		// To the user there is only one clipboard. Must clear other data if any
		m_data.m_image.Empty();
		m_data.m_html_text.Empty();
	}

	m_owner_token = token;
	OP_STATUS rc = m_data.m_text[m_data.m_is_mouse_selection ? 0 : 1].Set(text);

	pthread_mutex_unlock(&m_data.m_mutex);

	RETURN_IF_ERROR(rc);

	RETURN_IF_ERROR(WriteCommand(atom == XA_PRIMARY ? "acquire primary" : "acquire clipboard"));
	OpStatus::Ignore(WaitForSemaphore(10));

	return OpStatus::OK;
}

OP_STATUS X11OpClipboard::GetText(OpString &text)
{
	text.Empty();

	pthread_mutex_lock(&m_data.m_mutex);
	m_data.m_target = &text;
	pthread_mutex_unlock(&m_data.m_mutex);

	RETURN_IF_ERROR(WriteCommand("get text"));
	OpStatus::Ignore(WaitForSemaphore(10));

	pthread_mutex_lock(&m_data.m_mutex);
	m_data.m_target = NULL;
	pthread_mutex_unlock(&m_data.m_mutex);

	// Even if caller has used HasText() before calling GetText() a remote source
	// can have removed its data before we manage to retrieve them. We must return
	// an error if the string is empty (opera will otherwise crash in OpEdit::InternalInsertText()
	// called from OpEdit::Paste()
	return text.HasContent() ? OpStatus::OK : OpStatus::ERR;
}


BOOL X11OpClipboard::HasTextHTML()
{
	// HTML text can only be copied from the CLIPBOARD buffer (DSK-322250)
	if (GetMouseSelectionMode())
		return FALSE;

	pthread_mutex_lock(&m_data.m_mutex);
	BOOL has_content = m_data.m_html_text.HasContent();
	pthread_mutex_unlock(&m_data.m_mutex);

	return has_content;
}


OP_STATUS X11OpClipboard::PlaceTextHTML(const uni_char* htmltext, const uni_char* text, UINT32 token)
{
	if (GetMouseSelectionMode())
		return OpStatus::ERR;

	RETURN_IF_ERROR(PlaceText(text, token));

	pthread_mutex_lock(&m_data.m_mutex);
	OP_STATUS rc = m_data.m_html_text.Set(htmltext);
	pthread_mutex_unlock(&m_data.m_mutex);

	return rc;
}


OP_STATUS X11OpClipboard::GetTextHTML(OpString &text)
{
	// HTML text can only be copied from the CLIPBOARD buffer (DSK-322250)
	if (GetMouseSelectionMode())
	{
		text.Empty();
		return OpStatus::OK;
	}

	pthread_mutex_lock(&m_data.m_mutex);
	OP_STATUS rc = text.Set(m_data.m_html_text);
	pthread_mutex_unlock(&m_data.m_mutex);

	return rc;
}

OP_STATUS X11OpClipboard::PlaceBitmap(const class OpBitmap *bitmap, UINT32 token)
{
	if (!bitmap)
		return OpStatus::ERR_NULL_POINTER;

	pthread_mutex_lock(&m_data.m_mutex);
	m_data.m_text[0].Empty();
	m_data.m_text[1].Empty();
	m_data.m_html_text.Empty();
	OP_STATUS rc = m_data.m_image.MakeBMP(bitmap);
	pthread_mutex_unlock(&m_data.m_mutex);
	m_owner_token = token;
	RETURN_IF_ERROR(rc);

	X11Types::Atom atom = GetCurrentClipboard();
	RETURN_IF_ERROR(WriteCommand(atom == XA_PRIMARY ? "acquire primary" : "acquire clipboard"));
	OpStatus::Ignore(WaitForSemaphore(10));

	return OpStatus::OK;
}


void X11OpClipboard::SetMouseSelectionMode(BOOL mouse_selection)
{
	// m_is_mouse_selection is local to opera thread, while m_data.m_is_mouse_selection is only set
	// in worker thread with command below.

	if (m_is_mouse_selection != mouse_selection)
	{
		RETURN_VOID_IF_ERROR(WriteCommand(mouse_selection ? "set primary" : "set clipboard"));
		OpStatus::Ignore(WaitForSemaphore(10));
		m_is_mouse_selection = m_data.m_is_mouse_selection;
	}
}


BOOL X11OpClipboard::HandleEvent(XEvent* event)
{
	// The main thread should never receive selection event for copy-paste
	// It is kept enabled to simplify debugging should this assumption break

	switch(event->type)
	{
		case SelectionClear:
			OP_ASSERT(!"X11OpClipboard::HandleEvent (MAIN) - SelectionClear");
			break;

		case SelectionNotify:
			OP_ASSERT(!"X11OpClipboard::HandleEvent (MAIN) - SelectionNotify");
			break;

		case SelectionRequest:
			OP_ASSERT(!"X11OpClipboard::HandleEvent (MAIN) - SelectionRequest");
			break;
	}

	return FALSE;
}


OP_STATUS X11OpClipboard::EmptyClipboard(UINT32 token)
{
	bool can_empty = m_owner_token == token;
	if (can_empty)
	{
		RETURN_IF_ERROR(WriteCommand("empty"));
		OpStatus::Ignore(WaitForSemaphore(10));
	}
	return can_empty ? OpStatus::OK : OpStatus::ERR;
}


void X11OpClipboard::OnExit()
{
	OpStatus::Ignore(WriteCommand("exit"));
	OpStatus::Ignore(WaitForSemaphore(10));

	close(m_data.m_pipe[0]);
	close(m_data.m_pipe[1]);

	sem_destroy(&m_data.m_semaphore);

	pthread_mutex_destroy(&m_data.m_mutex);
}


X11Types::Atom X11OpClipboard::GetCurrentClipboard()
{
	return m_is_mouse_selection ? XA_PRIMARY : XInternAtom(g_x11->GetDisplay(), "CLIPBOARD", False);
}



OP_STATUS ClipboardImageItem::MakeBMP(const OpBitmap* bitmap)
{
	// Simple BMP spec: http://en.wikipedia.org/wiki/BMP_file_format

	struct bmpfile_magic
	{
		unsigned char magic[2];
	};

	struct bmpfile_header
	{
		UINT32 filesz;
		UINT16 creator1;
		UINT16 creator2;
		UINT32 bmp_offset;
	};

	struct bmp_dib_v3_header
	{
		UINT32 header_sz;
		UINT32 width;
		UINT32 height;
		UINT16 nplanes;
		UINT16 bitspp;
		UINT32 compress_type;
		UINT32 bmp_bytesz;
		UINT32 hres;
		UINT32 vres;
		UINT32 ncolors;
		UINT32 nimpcolors;
	};

	Empty();

	UINT32 header_size_bytes =
		sizeof(bmpfile_magic) + 2 +
		sizeof(bmpfile_header) +
		sizeof(bmp_dib_v3_header);
	UINT32 fullsize_bytes = header_size_bytes +
		(bitmap->Width() * bitmap->Height()) * 4;

	bmpfile_magic magic;
	bmpfile_header h1;
	bmp_dib_v3_header h2;

	magic.magic[0] = 'B';
	magic.magic[1] = 'M';
	h1.filesz = fullsize_bytes;
	h1.creator1 = 0;
	h1.creator2 = 0;
	h1.bmp_offset  = header_size_bytes;
	h2.header_sz = 40;
	h2.width = bitmap->Width();
	h2.height = bitmap->Height();
	h2.nplanes = 1;
	h2.bitspp = 32;
	h2.compress_type = 0;
	h2.bmp_bytesz = bitmap->Width() * bitmap->Height() * 4;
	h2.hres = 0;
	h2.vres = 0;
	h2.ncolors = 0;
	h2.nimpcolors = 0;

	m_buffer = OP_NEWA(UINT32, (header_size_bytes+fullsize_bytes) / 4);
	if (!m_buffer)
		return OpStatus::ERR_NO_MEMORY;
	m_size = header_size_bytes+fullsize_bytes;

	UINT8* p = (UINT8*)m_buffer;
	op_memcpy(p, &magic, sizeof(magic)); p += sizeof(magic);
	op_memcpy(p, &h1, sizeof(h1)); p += sizeof(h1);
	op_memcpy(p, &h2, sizeof(h2));

	UINT32 width  = bitmap->Width();
	UINT32 height = bitmap->Height();
	UINT32* line  = &m_buffer[header_size_bytes/4];
	for (UINT32 i=0; i<height; i++)
	{
		bitmap->GetLineData(line, height-i-1);
		line += width;
	}

	return OpStatus::OK;
}

