/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_EXTENSIONMANAGER_H
#define DOM_EXTENSIONS_EXTENSIONMANAGER_H

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/domruntime.h"

#include "modules/util/OpHashTable.h"
#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/dom/src/extensions/domextensionmenuitem.h"

class DOM_ExtensionBackground;

/** DOM_ExtensionManager - tracking active extensions.

    In order to support communication between a window and its background extension,
    an extension is notified when a new window is activated. This is recorded via the
    'extension manager', which the opera.* object of DOM environments within the window
    will see. If found, it can then set up the connection and start communicating.

    When the environments on the window's side disappear, they unregister via this manager,
    causing the association between the window and extension to be dropped.

    The extension manager cuts across DOM environments, and the single instance is kept at the DOM
    module level. It may not end up being the long-term solution for brokering UI window <-> extension
    communication. */
class DOM_ExtensionManager
	: public OpHashTableForEachListener
{
public:
	static OP_STATUS Make(DOM_ExtensionManager *&new_obj);

	virtual ~DOM_ExtensionManager();

	OP_STATUS AddExtensionContext(Window *window, DOM_Runtime *runtime, DOM_ExtensionBackground **background);
	/**< Create and add the correct extension context (opera.extension.*) to the
		 opera object of the corresponding runtime.
		 No context will be added if the window is not associated with a gadget.

		 @param window the window.
		 @param runtime the runtime of this background extension context.
		 @param[out] background the background extension object.
		 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM condition. */

	void RemoveExtensionContext(DOM_ExtensionSupport *support);
	/**< Remove the background process' extension context and notify the
		 extension contexts of all other environments associated to
		 the given extension background object that it's being shutdown (so
		 they can dispatch disconnection events and the such). */

	void RemoveExtensionContext(DOM_EnvironmentImpl *environment);
	/**< Remove the extension context associated with the given environment.
		 This is called by the environment itself when shutting down. */

	DOM_ExtensionSupport *GetExtensionSupport(OpGadget *gadget);
	/**< Get the extension support for the given gadget. */

	static void Shutdown(DOM_ExtensionManager *manager);
	/**< Perform clean up and shut down; run upon module shutdown. */

	static OP_STATUS Init();
	/**< Create and initialise the singleton extension manager instance */

	// From OpHashTableForEachListener
	virtual void HandleKeyData(const void *key, void *data);

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	OP_STATUS AppendExtensionMenu(OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItemList& menu_items, OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element);
	/**< Fills a vector of menu items which extensions(via ContextMenu API) requested
	 *   to add to the context menu.
	 *
	 *   @param menu_items List of menu items to which the menu items will be added.
	 *   @param document_context Context in which the menu has been requested.
	 *   @param document Document for which the menu has been requested.
	 *   @param element Element for which the menu has been requested. May be NULL.
	 *   @return
	 *      OpStatus::OK
	 *      OpStatus::ERR_NO_MEMORY
	 */
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
private:

	class ExtensionBackgroundInfoElement
	{
	public:
		ExtensionBackgroundInfoElement(DOM_ExtensionSupport *extension_support)
			: m_extension_support(extension_support)
		{
		}

		OpVector<DOM_EnvironmentImpl> m_connections;
		/**< The environments associated with this extension background that
		 * may need something done when the extension background shuts down
		 * (e.g. sending ondisconnect events to the DOM_ExtensionPageContext). */

		OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
	};

	OpPointerHashTable<OpGadget, ExtensionBackgroundInfoElement> m_extensions;
	/**< Mapping from a gadget to the background process' extension context. */
};

#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_EXTENSIONMANAGER_H
