/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_EXTENSIONSUPPORT_H
#define DOM_EXTENSIONS_EXTENSIONSUPPORT_H

class DOM_ExtensionBackground;
class DOM_Extension;
class DOM_ExtensionScope;
class DOM_ExtensionSupport;
#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/dom/src/domwebworkers/domwweventqueue.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/dom/src/domevents/domevent.h"

class DOM_ExtensionBackground;
class DOM_Extension;
class DOM_ExtensionScope;

/* Overview of DOM Extension support, three parts:

    - The 'background page' Extension and the APIs available to it.

    - The 'content scripts' running within the context of a document;
      these are isolated to run in their own UserJS context with a separate
	  runtime & global object, but sharing access to the page's document / DOMEnvironment.

    - Connected browsing contexts, which can communicate with the extension background.

  For the UserJS part:

    - DOM_ExtensionScope: the toplevel, global object for the UserJS (it's 'window')
	  i.e., the global object of the DOM_UserJSExecutionContext.

    - DOM_Extension: the object implementing opera.extension.*, letting the UserJS
	  use postMessage() to communicate with the background Extension + register
	  handlers for listening to messages flowing back.

  The main Extension portion:

    - DOM_ExtensionSupport: 'manager' object that sort of sits between the
      background process object and the documents the extension is currently
      working (either via extensionjs or browsing contexts associated with the
      extension, and keeps track of all the ports these all use to communicate.

      Right now, each extension-associated browsing context has a port, which
      communicates with a port that's kept in the DOM_ExtensionSupport object,
      which in turn forwards all the messages it gets on all these "intermediate"
      ports to the background page's port.

      It also coordinates the shutdown of certain objects in a few occasions.
      This object outlives the gadget and every other object that keeps a
      reference to it (through refcounting).

    - DOM_ExtensionBackground: implements opera.extension.* in the background page.

    - DOM_ExtensionContexts: implements opera.contexts.*

    - DOM_ExtensionContext: implements opera.contexts.toolbar (and other platform-builtin contexts),
	  + DOM_ExtensionUIItem derives from DOM_ExtensionContext.

    - UIItem API: installable extension buttons on the platform's UI.

        - DOM_ExtensionUIItem: the UIItem DOM objects, small UI buttons w/ favicons, labels + optional
	      popups. Objects attached to it:
            + DOM_ExtensionUIBadge: uiitem.badge
            + DOM_ExtensionUIPopup: uiitem.popup

    - Windows + Tabs API: working with Desktop (toplevel) Windows and tabbed documents within those.

	    - DOM_ExtensionWindows: opera.extension.windows.*
		- DOM_ExtensionWindow:
	    - DOM_ExtensionTabs: opera.extension.tabs.*
        - DOM_ExtensionTab:

  The connected browsing contexts:

     - DOM_ExtensionPageContext: communicating with the extension background from
       a connected browsing context. The name suggests its first (and main)
       use, providing the messaging for popup windows of UIItems. More to
       come.

     - DOM_ExtensionManager: a DOM module-level object that tracks what
       windows are being allowed to communicate with a given extension.
       Used to handle the setup (and teardown) of connected browsing contexts
       like popup windows.
*/
class DOM_ExtensionSupport
	: public OpGadgetExtensionListener
	, public OpHashTableForEachListener
	, public OpReferenceCounter
{
public:
	static OP_STATUS Make(DOM_ExtensionSupport *&new_obj, OpGadget *extension_gadget);

	virtual ~DOM_ExtensionSupport();

	OpGadget *GetGadget() { return m_gadget; }

	DOM_ExtensionBackground *GetBackground() { return m_background; }
	/**< Get the extension background process context. Can be NULL if there
	     isn't one at the time (if this happens it should be for a very brief
	     period of time since a reload is the only thing that can make it go
	     away. */

	void SetExtensionBackground(DOM_ExtensionBackground *background);
	/**< Set a new extension background process context and set all ports
	     currently registered to forward to its port.
	     background must be non-NULL. */

	void UnsetExtensionBackground(DOM_ExtensionBackground *background);
	/**< Unset the extension background process context (but only if it's the
	     current registered one, doesn't do anything if another background was
	     set before unsetting this one. Also disentangles all the ports that
	     were forwarding to the background's port. */

	BOOL IsExtensionActiveInWindow(Window *window);
	/**< TRUE if the extension is currently operating in the context of the given window. */

	DOM_ExtensionScope* FindExtensionUserJS(FramesDocument *frames_doc);
	/**< Returns top level host object for this extension UserJS injected
	     to specified document environment or NULL if there is none.
		 @param frames_doc - document to which environment the UserJS object
		        injected. Must NOT be NULL. */

	OP_STATUS AddExtensionJSEnvironment(DOM_EnvironmentImpl *environment, DOM_ExtensionScope *toplevel);
	/**< Register environment where the extension is currently running. Done as part
	     of starting up its execution context, and is tracked by the extension so as to be able
	     to precisely report the windows the extension can currently see.

	     @param environment the environment the extension js is running in.
	     @param toplevel the global object of the UserJS extension context.
	     @return OpStatus::ERR_NO_MEMORY if OOM condition occurs,
	             OpStatus::OK if registered successfully. */

	void RemoveExtensionJSEnvironment(DOM_EnvironmentImpl *environment, DOM_ExtensionScope *toplevel);
	/**< An instance of an extension js running on a particular page is shutting down,
	     unregister the environment as being active for the 'toplevel' context.

	     It is the strict responsibility of the extension execution context to unregister before
	     it becomes invalid and is shut down. That is, the global object reference that is kept
	     by the extension itself here is not owned and kept alive; the reference
	     to it is assumed to be valid.

	     @param environment the environment the extension js is running in. */

	DOM_ExtensionScope * GetExtensionGlobalScope(Window *window);
	/**< Locate the global object for the extension context associated with given window.

	     @param window the document's window.
	     @return NULL if the window doesn't have a context running the extension; a valid pointer
	     to its global object otherwise. */

	static OP_STATUS GetPortTarget(DOM_MessagePort *port, DOM_MessagePort *&target_port, DOM_Runtime *target_runtime);
	/**< Create a new target port, entangle it with port and return it in target_port. */

	OP_STATUS AddPortConnection(DOM_MessagePort *port);
	/**< Register a port from an associated extension browsing context that should
	     forward any messages to the background process. */

	void RemovePortConnection(DOM_MessagePort *port);
	/**< Unregister a port from an associated extension browsing context that was
	     forwarding messages to the background process. */

	// From OpGadgetExtensionListener
	virtual BOOL HasGadgetConnected() { return TRUE; }
	virtual void OnGadgetRunning() {}
	virtual void OnGadgetTerminated() { Shutdown(); }

	virtual void HandleKeyData(const void* key, void* data);

	/** Local function that constructs a DOM_MessageEvent and delivers it to the given port.
	 * Used by local variants of postMessage() and broadcastMessage().
	 */
	static int PostMessageToPort(DOM_Object *this_object, DOM_MessagePort *target_port, DOM_MessagePort *target_return_port, URL &extension_url, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *target_runtime);

	static OP_STATUS AddEventHandler(DOM_Object *target, DOM_EventListener *&handler, ES_Object *properties, ES_Value *given_value, ES_Runtime *origining_runtime, const uni_char *event_handler_name, DOM_EventType event_type, BOOL *has_changed = NULL);

	/** Retrieves a DOM_File object referring to a specified file in extension package.
	 *  @param path - Relative path to a file inside an extension.
	 *  @param parent_url - Base url of a document to resolve path.
	 *  @param return_value - The DOM_File object will be put here on success
	 *         (or null or Exception object on failure).
	 *  @param this_object - Object which requested the file.
	 *  @return ES_VALUE - If the function succeded (even if the file couldn't
	 *          be retrieved, or ES_EXCEPTION if any unexpected state occured while processing this
	 *          request.
	 */
	int GetExtensionFile(const uni_char* path, URL parent_url, ES_Value* return_value, DOM_Object* this_object);

	int GetScreenshot(FramesDocument *target, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime *origining_runtime);
private:
	friend class DOM_ExtensionBackground;

	DOM_ExtensionSupport(OpGadget *extension_gadget)
		: m_gadget(extension_gadget)
		, m_background(NULL)
		{}

	void Shutdown();

	OpGadget *m_gadget;

	DOM_ExtensionBackground *m_background;

	class ActiveExtensionElement
	{
	public:
		OP_STATUS AddExtension(DOM_ExtensionScope *s);

		~ActiveExtensionElement();

		Head/*DOM_ExtensionScope*/ m_extensions;
	};

	OpPointerHashTable<DOM_EnvironmentImpl, ActiveExtensionElement> m_active_environments;
	/**< The Window IDs of the document windows where the extension is currently running. */

	OpVector<DOM_MessagePort> m_ports;
	/**< The list of ports from the userjs and page instances connected to this extension.
	     Used to implement broadcasting.
	     These ports are the actual ports from the connected extension contexts, and each
	     will be entangled with a port that has the background as a forwarding port
	     (this is set up in AddPortConnection()). */
};

// Helpers for extensions

/** PUT_FAILED_IF_ERROR() extension, mapping PUT_FAILED to a DOMException
  * and returning a PUT_EXCEPTION. As it expands into more code
  * than what it was derived from, be judicious in its use.
  */
#define PUT_FAILED_IF_ERROR_EXCEPTION(expr, excn) \
	do { \
		OP_STATUS PUT_FAILED_IF_ERROR_E_TMP = expr; \
		if (OpStatus::IsMemoryError(PUT_FAILED_IF_ERROR_E_TMP)) \
			return PUT_NO_MEMORY; \
		if (OpStatus::IsError(PUT_FAILED_IF_ERROR_E_TMP)) \
			return DOM_PUTNAME_DOMEXCEPTION(excn); \
   } while(0)

/* Specialised, but common here. */
#define CALL_INVALID_IF_ERROR(status) \
	do { \
		OP_STATUS CALL_INVALID_IF_E_TMP = status; \
		if (OpStatus::IsMemoryError(CALL_INVALID_IF_E_TMP)) \
			return ES_NO_MEMORY; \
		if (OpStatus::IsError(CALL_INVALID_IF_E_TMP)) \
			return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR); \
   } while(0)

#define GET_INVALID_IF_ERROR(status) \
	do { \
		OP_STATUS GET_INVALID_IF_E_TMP = status; \
		if (OpStatus::IsMemoryError(GET_INVALID_IF_E_TMP)) \
			return GET_NO_MEMORY; \
		if (OpStatus::IsError(GET_INVALID_IF_E_TMP)) \
			return DOM_GETNAME_DOMEXCEPTION(INVALID_STATE_ERR); \
   } while(0)




#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_EXTENSIONSUPPORT_H
