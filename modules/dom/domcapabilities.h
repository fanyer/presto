/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_CAPABILITIES_H
#define DOM_CAPABILITIES_H

/** @file domcapabilities.h
  *
  * DOM capabilities file.
  *
  * @author Jens Lindstrom
  */

/* The DOM module has the DOM_Environment class. */
#define DOM_CAP_ENVIRONMENT

/* The DOM module has its own DOM_EventType defintion. */
#define DOM_CAP_EVENTTYPES

/* The DOM module has its own ES_EventType defintion. */
#define DOM_CAP_EVENTTHREAD

/* The DOM module has a header declaring or defining simple types
   needed by other modules. */
#define DOM_CAP_TYPES_HEADER

/* The DOM module supports namespaces for DOM Events (DOM 3 Events).
   This affects the signature of functions dealing with event types. */
#define DOM_CAP_EVENT_NAMESPACES

/* The function DOM_Environment::IsHandlingScriptElement is supported. */
#define DOM_CAP_IS_HANDLING_SCRIPT_ELEMENT

/* The DOM_Environment class has an API for registering callback
   functions on the global object and the opera object.  The old
   "API" in the JS_Opera class has been removed. */
#define DOM_CAP_CALLBACK_API

/* The DOM module want to subclass ES_Runtime rather than to
   baseclass it.  The class ES_DOMMixin (previously the base of
   ES_Runtime) has changed name to DOM_Runtime. */
#define DOM_CAP_ES_RUNTIME_SUBCLASS

/* The DOM_Environment class has an API for adding and removing
   remote event streams. */
#define DOM_CAP_REMOTE_EVENTS

/* The DOM_Environment class has a CreateStatic method that should
   be called when Opera starts. */
#define DOM_CAP_CREATE_STATIC

/* DOM parts of WebForms2 has been updated to the Call-for-Comments
   version of the specification. */
#define DOM_CAP_WF2CC

/* DOM_Utils::GetDOM_Object method has a type parameter. */
#define DOM_CAP_GET_DOM_OBJECT_TYPE_PARAM

/* The DOM_Environment class has a SkipScriptElements function. */
#define DOM_CAP_SKIP_SCRIPT_ELEMENTS

/* The Opera Platform class implements the alarm set functionality. */
#define DOM_CAP_OPFM_HAS_ALARM_EVENT

/* Some normally recursive functions like have an extra argument
   "recurse" that controls their recursiveness. */
#define DOM_CAP_RECURSE_ARGUMENT

/* ElementInsertedIntoDocument has the 'hidden' argument. */
#define DOM_CAP_HIDDEN_ARGUMENT

/* The DOM_Environment class has a GetTempBuffer function. */
#define DOM_CAP_GET_TEMPBUFFER

/* DOM_ProxyEnvironment::RealWindowProvider exists, supporting lazy
   construction of real window for proxy windows. */
#define DOM_CAP_REAL_WINDOW_PROVIDER

/* DOM_Environment::GetUserJS{Enabled,FilesCount,Filename} exist. */
#define DOM_CAP_USER_JS_FILES

/* DOM_Environment::HandleJavascriptURL{,Finished} exist. */
#define DOM_CAP_HANDLE_JAVASCRIPT_URL

/* DOM_Environment::OpenURLWithTarget exists. */
#define DOM_CAP_OPEN_URL_WITH_TARGET

/* The event type ONREADYSTATECHANGE is supported. */
#define DOM_CAP_ONREADYSTATECHANGE

/* DOM_Environment::ElementCharacterDataChanged exists and should be called. */
#define DOM_CAP_ELEMENTCHARACTERDATACHANGED

/* DOM_Utils::GetEventTarget exists. */
#define DOM_CAP_GET_EVENT_TARGET

/* DOM_EventType::ONINPUT exists (WF2 event) */
#define DOM_CAP_ONINPUT

/* The DOMCONTENTLOADED event type is supported. */
#define DOM_CAP_DOMCONTENTLOADED

/* Before sending the click event to checkboxes, the state is stored. */
#define DOM_CAP_STORES_CHECK_STATE

/* DOM_EventType::ONMOUSEWHEEL exists. */
#define DOM_CAP_ONMOUSEWHEEL

/* DOM_Environment::OpenURLWithTarget has 'user_initiated' argument. */
#define DOM_CAP_OPENURLWITHTARGET_USER_INITIATED

/* Gadgets can now call javascript-defined hooks on events (like onshow, onhide, ondragstart...) */
#define DOM_CAP_GADGET_EVENT_HANDLING

/* DOM_Environment::IsCalledFromUnrequestedScript is supported. */
#define DOM_CAP_ISCALLEDFROMUNREQUESTEDSCRIPT

/* Supports delayed event handler registration. */
#define DOM_CAP_DELAYED_SETEVENTHANDLER

/* DOM_Environment::OpenWindowWithData is supported. */
#define DOM_CAP_OPENWINDOWWITHDATA

/* DOM_Utils::GetActualEventTarget is supported. */
#define DOM_CAP_GETACTUALEVENTTARGET

/* DOM_Environment::ResetUserJSonHTTPSConfirmation is supported. */
#define DOM_CAP_USERJS_ON_HTTPS

/* DOM_Utils::ReleaseSVGDOMObjectFromLists exists and should be called. */
#define DOM_CAP_SVG_HAS_RELEASE_OBJECT

/* DOM_Environment::BeforeDestroy. */
#define DOM_CAP_DOMENVIRONMENT_BEFOREDESTROY

/* DOM_Environment::HasLocalEventHandler is supported. */
#define DOM_CAP_DOMENVIRONMENT_HASLOCALEVENTHANDLER

/* DOM_Environment::CheckBrowserJSSignature is supported (#ifdef DOM_BROWSERJS_SUPPORT). */
#define DOM_CAP_DOMENVIRONMENT_CHECKBROWSERJSSIGNATURE

/* DOM_Utils::HasDebugPrivileges is supported. */
#define DOM_CAP_DOMUTILS_HASDEBUGPRIVILEGES

/* Has stopped using HTML_Element::DOMHasTextContent. */
#define DOM_CAP_SKIPS_HASTEXTCONTENT

/* DOM_Environment::PluginInitializedElement is supported (#if defined(USER_JAVASCRIPT) && defined(_PLUGIN_SUPPORT_)). */
#define DOM_CAP_DOMENVIRONMENT_PLUGININITIALIZEDELEMENT

/* DOM_Utils::GetEventTargetElement() is supported. */
#define DOM_CAP_DOMUTILS_GETEVENTTARGETELEMENT

/* Has stopped using HTML_Element::GetSelectTextValue */
#define DOM_CAP_NO_GETSELECTTEXTVALUE

/* DOM_Environment::OpenURLWithTarget supports plugin_unrequested_popup argument. */
#define DOM_CAP_OPENURLWITHTARGET_PLUGIN_POPUP

/* A static DOM_Environment::IsEnabled() version exists. */
#define DOM_CAP_DOMENVIRONMENT_STATIC_ISENABLED

/* The event types ONONLINE and ONOFFLINE are supported. */
#define DOM_CAP_ONONLINE_ONOFFLINE

/* Uses the SVGDOM::CreateSVGDOMItem() method. */
#define DOM_CAP_USES_CREATESVGDOMITEM

/* DOM_Environment::BeforeUnload() is supported. */
#define DOM_CAP_DOMENVIRONMENT_BEFOREUNLOAD

/* dom expects there to be both a FramesDocElm::GetFrameId() and a FramesDocElm::GetName() */
#define DOM_CAP_SPLITFRAMENAMEANDID

/* DOM_EventsAPI::IsWindowEvent() is supported. */
#define DOM_CAP_DOMEVENTSAPI_ISWINDOWEVENT

/* DOM_Environment::ElementCollectionStatusChanged() is supported. */
#define DOM_CAP_DOMENVIRONMENT_ELEMENTCOLLECTIONSTATUSCHANGED

/* DOM_Environment::COLLECTION_CLASS is supported. */
#define DOM_CAP_DOMENVIRONMENT_COLLECTION_CLASSNAME

/* DOM_Environment::Element{Inserted,Removed} exist, and replaces the (now) deprecated DOM_Environment::Element{InsertedInto,RemovedFrom}Document. */
#define DOM_CAP_DOMENVIRONMENT_ELEMENTINSERTED_ELEMENTREMOVED

/* This module includes layout/layout_workplace.h where needed, instead of relying on doc/frm_doc.h doing it. */
#define DOM_CAP_INCLUDE_CSSPROPS

/* This module implements opera.postError, so the ecmascript module can stop doing that. */
#define DOM_CAP_OPERA_POSTERROR

/* DOM_Environment::ConstructCSSRule() is supported. */
#define DOM_CAP_DOMENVIRONMENT_CONSTRUCTCSSRULE

/* This module supports the StaticNodeList interface. */
#define DOM_CAP_HAS_STATICNODELIST

/* This module supports widgets mode. */
#define DOM_CAP_GADGET_MODE

/* This module supports widgets getAttention, showNotification, show and hide */
#define DOM_CAP_GADGET_GETATTENTION

/* No longer calls HTML_Element::DOMCreateTextNode with a force_linebreak argument. */
#define DOM_CAP_NOFORCEDLINEBREAK

/* DOM_Utils::SetDomain is supported. */
#define DOM_CAP_DOMUTILS_SETDOMAIN

/* DOM_Environment::ElementCharacterDataChanged has informational arguments to be able to update ranges better. */
#define DOM_CAP_TEXT_CHANGED_INFO

/* DOM_Range is fixed to initialize correctly so that it won't crash if there is a DOCTYPE node in the document. */
#define DOM_CAP_RANGE_SURVIVES_DOCTYPE

/* DOM_Environment::ElementAttributeChanged() is supported. */
#define DOM_CAP_DOMENVIRONMENT_ELEMENTATTRIBUTECHANGED

/* Supports ES_CAP_PROPERTY_TRANSLATION and the new EcmaScript_Object::{Get,Put}{Index,Name}() signatures. */
#define DOM_CAP_NEW_GETNAME

/* Calls to GetDOMObject in webfeeds module includes the DOM_Environment parameter */
#define DOM_CAP_USES_FEED_WITH_ENVIRONMENT

/* DOM_Utils::HasOverriddenDomain is supported. */
#define DOM_CAP_DOMUTILS_HASOVERRIDDENDOMAIN

/* Has DOM_Environment::HandleError() */
#define DOM_CAP_HANDLEERROR

/* Has DOM_EventType::ONCONTEXTMENU */
#define DOM_CAP_ONCONTEXTMENU

/* HandleEvent can return the ES_Thread created */
#define DOM_CAP_HANDLE_EVENT_RETURNS_THREAD

/* DOM_Environment::EventData has might_be_click */
#define DOM_CAP_MIGHTBECLICK

/* Dom isn't using HTML_Element::DOMGetFrameEnvironment and uses HTML_Element::DOMGetFrameProxyEnvironment */
#define DOM_CAP_PREFERS_GET_PROXY_ENVIRONMENT

/* Dom isn't using ATTR_PIXELRATIO anymore */
#define DOM_CAP_NOT_USING_ATTR_PIXELRATIO

/* Stopped using the TemplateHandler from forms. */
#define DOM_CAP_NO_MORE_REPEAT_TEMPLATES

/* JS_Opera::setOverridePreference can be used to set value of cross domain access preference */
#define DOM_CAP_SET_OVERRIDE_PREFERENCE

/* DOM_Utils::GetWindowLocationObject exists. */
#define DOM_CAP_HAS_GETWINDOWLOCATIONOBJECT

/* All the DOM3 properties and functions are now always there */
#define DOM_CAP_DOM3CORE_ALWAYS_THERE

/* DOM_Environment::HasWindowEventHandler exists */
#define DOM_CAP_HAS_HAS_WINDOW_EVENT_HANDLER

/* The form seeding code is removed */
#define DOM_CAP_REMOVED_FORM_SEEDING

/* The method DOM_Environment::ForceNextEvent exists. */
#define DOM_CAP_HAS_FORCE_NEXT_EVENT

/* Uses the OpString variant of feed title, instead of the deprecated uni_char one */
#define DOM_CAP_USE_OPSTRING_FEED_TITLE

/* DOM module is prepared for Carakan */
#define DOM_CAP_PREPARE_FOR_CARAKAN

#endif // DOM_CAPABILITIES_H
