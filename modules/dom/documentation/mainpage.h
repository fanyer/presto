/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


/** @mainpage DOM module
 *
 * This is the auto-generated API documentation for the DOM module.  For more
 * information about the module, see the module's <a
 * href="http://wiki.intern.opera.no/cgi-bin/wiki/wiki.pl?Modules/dom">Wiki
 * page</a>.
 *
 * @section api Public API
 *
 * The DOM module's external API consists of one forward declared class
 * (DOM_Object), a number of enumerations (DOM_EventType, ES_EventPhase,
 * DOM_HTMLElement_Group, DOM_ObjectType and DOM_NodeType), two environment
 * encapsulating classes (DOM_Environment and DOM_ProxyEnvironment), one class
 * containing utility and conversion functions (DOM_Utils) and one optional DOM
 * Events API class (DOM_EventsAPI).
 *
 * The entire external API is declared and defined in header files in the
 * module's root directory.  In particular, all header files in the src
 * subdirectory are internal and must not be included by code outside the DOM
 * module (source files generated from files in the DOM module excluded).  If
 * definitions in internal header files appear to be needed outside of the
 * module, they should be moved to the external API or the external API should
 * be otherwise extended.
 *
 * @section system External requirements
 *
 * The DOM module requires that the functions DOM_Environment::CreateStatic and
 * DOM_Environment::DestroyStatic are called during startup and shutdown,
 * respectively.
 *
 * DOM_Environment::CreateStatic depends on nothing and can be called whenever,
 * but needs to be called before the DOM module is used at all (that is, before
 * any DOM environments are created or before the ECMAScript engine is used).
 *
 * DOM_Environment::DestroyStatic has to be called before the ECMAScript engine
 * is shut down (more specifically before the ECMAScript garbage collector makes
 * it final run, which it does when the engine is shut down).
 *
 * Furthermore, if the platform defines <code>_NO_GLOBALS_</code> or if it does
 * not define <code>HAS_COMPLEX_GLOBALS</code> (that is, sets
 * <code>SYSTEM_COMPLEX_GLOBALS</code> to <code>NO</code>), the platform must
 * provide a definition such that the identifier <code>g_DOM_globalData</code>
 * expands to a valid non-constant lvalue expression of the type <code>class
 * DOM_GlobalData *</code>.  The platform can forward declare the class
 * DOM_GlobalData if necessary (the DOM module does not).
 *
 * @section environment DOM environment
 *
 * A DOM environment is a standard ECMAScript environment (as provided by the
 * ES_Runtime class in the ecmascript module) extended with all the classes,
 * functions and constants defined by the DOM specifications and ancient
 * JavaScript "specifications" that Opera supports.
 *
 * Each XML/HTML document owns zero or one DOM environment (created on demand).
 * Each DOM environment is separate from all other DOM environments and has its
 * own instances of all predefined global objects (such as the window object,
 * the navigator object and the history object).
 *
 * @subsection creation_destruction Creation and destruction
 *
 * It is each document's responsability to create and destroy its DOM
 * environment using the functions DOM_Environment::Create and
 * DOM_Environment::Destroy.
 *
 * The environment can be created at any time before it is needed (that is, it
 * can be created on demand).  If User Javascript is enabled, the User
 * Javascript script is read and executed during the call to
 * DOM_Environment::Create, guaranteeing it to be the first script executed in
 * the environment.
 *
 * The DOM environment should be destroyed as soon as it is known that the
 * document it belongs to will never be the active document in any window or
 * that no script or event handlers will ever be executed in the document again.
 * Destroying a DOM environment does not immediately free it since most of it is
 * actually owned by the ECMAScript garbage collector, but rather releases the
 * external garbage collector references that keeps the environment from being
 * collected prematurely, making it available to collection.
 *
 * @subsection notification Notification of document changes
 *
 * The DOM module has internal structures that needs to be updated when the
 * document changes.  They are updated when the DOM environment is notified of
 * the changes through the notification functions listed below.  It is very
 * important that these notification function are always called when something
 * changes, as the DOM module's internal structures will otherwise be invalid.
 * They should be called regardless of why the changes occur, including if the
 * changes are triggered by the DOM module itself.
 *
 * It is, however, never necessary to create a DOM environment only to call one
 * of these notification functions.
 *
 * <dl>
 *  <dt>DOM_Environment::NewRootElement</dt>
 *  <dd>Should be called if the document's root element is created or changed
 *      after its DOM environment is created</dd>
 *
 *  <dt>DOM_Environment::ElementInsertedIntoDocument</dt>
 *  <dd>Should be called when an element is inserted into the document.
 *      Specifically it should be called when and only when an element that
 *      previously had no parent becomes a descendant of the document' root
 *      element.</dd>
 *
 *  <dt>DOM_Environment::ElementRemovedFromDocument</dt>
 *  <dd>Should be called when an element is removed from the document.
 *      Specifically it should be called when and only when an element that
 *      is a descendant of the document's root element becomes parentless.</dd>
 *
 *  <dt>DOM_Environment::ElementNameOrIdChanged</dt>
 *  <dd>Should be called when an element's name or id attribute's value
 *      is changed.</dd>
 * </dl>
 *
 * @subsection nodes DOM nodes
 *
 * Each element in a XML/HTML document, each text or CDATA block, each comment,
 * the document itself and its document type declaration are available to
 * scripts as objects of various subclasses to the DOM Node interface, as
 * defined by DOM 2 Core.  The Document node and the Element node representing
 * the root element are created immediately when the DOM environment is created,
 * but all other nodes are created on demand (that is, the first time a script
 * needs them).  This behaviour is not observable to a script; all nodes appear
 * to exist at all times.
 *
 * Most node objects are also available for garbage collection at any time after
 * their creation.  If the node belonging to an element or other document tree
 * object has been garbage collected, a new one will be created on demand the
 * next time a script accesses it.  Node objects which it, for some reason,
 * would be observable (through lost state or otherwise) in a script if they
 * were garbage collected are not available for garbage collection.  This
 * behaviour, too, is not observable to a script, and will usually limit the
 * number of live Node objects to those actually referenced by scripts.
 *
 * Nodes can, when needed, be created and returned through the DOM module's
 * public API, via the DOM_Environment::ConstructNode function.  This function
 * can only be used to create Node objects for HTML elements if the correct
 * value of the Node.ownerDocument property of the newly created node would be
 * the document returned by the DOM_Environment::GetDocument function.
 *
 * @subsection event_handling Event handling
 *
 * DOM Events events can be sent through the DOM module's public API, using the
 * function DOM_Environment::HandleEvent.  This function takes as an argument a
 * DOM_Environment::EventData object, which should be filled in with the event
 * type, any appropriate additional information for that event type and the
 * target element of the event.
 *
 * The event handling is optimized so that events that are guaranteed not to be
 * handled by any event handlers are not created, sent or further handled at
 * all.  The function DOM_Environment::HandleEvent's return value can be used to
 * determine whether the event was actually sent.
 *
 * An event's default action should be performed by the function
 * HTML_Element::HandleEvent, which is called by the DOM module's event handling
 * when the handling of an event is completed assuming the event was sent.  If
 * DOM_Environment::HandleEvent's return value indicates that the event was not
 * sent, the default action needs to be triggered by the caller.  */
