/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_DOMGLOBALDATA_H
#define DOM_DOMGLOBALDATA_H

#include "modules/dom/src/dominternaltypes.h"
#include "modules/dom/src/domruntime.h"
#include "modules/dom/src/domlibrary.h"
#include "modules/util/tempbuf.h"

# include "modules/dom/src/canvas/domcontext2d.h"
# include "modules/dom/src/canvas/domcontextwebgl.h"
# include "modules/dom/src/domcore/chardata.h"
# include "modules/dom/src/domcore/comment.h"
# include "modules/dom/src/domcore/domconfiguration.h"
# include "modules/dom/src/domcore/domstringlist.h"
# include "modules/dom/src/domcore/domtokenlist.h"
# include "modules/dom/src/domcore/implem.h"
# include "modules/dom/src/domcore/text.h"
# include "modules/dom/src/domcore/domxmldocument.h"
# include "modules/dom/src/domcore/domstaticnodelist.h"
# include "modules/dom/src/domcore/docfrag.h"
# include "modules/dom/src/domcss/cssmediarule.h"
# include "modules/dom/src/domcss/csskeyframesrule.h"
# include "modules/dom/src/domcss/cssrulelist.h"
# include "modules/dom/src/domcss/cssstyledeclaration.h"
# include "modules/dom/src/domcss/cssstylesheet.h"
# include "modules/dom/src/domcss/csstransformvalue.h"
# include "modules/dom/src/domevents/domevent.h"
# include "modules/dom/src/domevents/domeventdata.h"
# include "modules/dom/src/domevents/dommutationevent.h"
# include "modules/dom/src/domevents/domprogressevent.h"
# include "modules/dom/src/domevents/domtouchevent.h"
# include "modules/dom/src/domfile/domblob.h"
# include "modules/dom/src/domfile/domfileerror.h"
# include "modules/dom/src/domfile/domfileexception.h"
# include "modules/dom/src/domfile/domfilereader.h"
# include "modules/dom/src/domfile/domfilereadersync.h"
# include "modules/dom/src/domhtml/htmlcoll.h"
# include "modules/dom/src/domhtml/htmlmicrodata.h"
# include "modules/dom/src/domhtml/htmldoc.h"
# include "modules/dom/src/domhtml/htmlelem.h"
# include "modules/dom/src/domload/domparser.h"
# include "modules/dom/src/domload/lsparser.h"
# include "modules/dom/src/domperformance/domperformance.h"
# include "modules/dom/src/domtraversal/nodeiterator.h"
# include "modules/dom/src/domtraversal/treewalker.h"
# include "modules/dom/src/domrange/range.h"
# include "modules/dom/src/domsave/lsserializer.h"
# include "modules/dom/src/domsave/xmlserializer.h"
# include "modules/dom/src/domstylesheets/stylesheetlist.h"
# include "modules/dom/src/domstylesheets/medialist.h"
# include "modules/dom/src/domstylesheets/mediaquerylist.h"
# include "modules/dom/src/domsvg/domsvgobjectstore.h"
# include "modules/dom/src/domsvg/domsvgelement.h"
# include "modules/dom/src/domsvg/domsvgelementinstance.h"
# include "modules/dom/src/domsvg/domsvglist.h"
# include "modules/dom/src/domsvg/domsvgobject.h"
# include "modules/dom/src/domsvg/domsvgtimeevent.h"
# include "modules/dom/src/domxpath/xpathevaluator.h"
# include "modules/dom/src/domxpath/xpathexpression.h"
# include "modules/dom/src/domxpath/xpathnamespace.h"
# include "modules/dom/src/domxpath/xpathnsresolver.h"
# include "modules/dom/src/domxpath/xpathresult.h"
# include "modules/dom/src/js/history.h"
# include "modules/dom/src/js/location.h"
# include "modules/dom/src/js/navigat.h"
# include "modules/dom/src/js/js_console.h"
# include "modules/dom/src/js/window.h"
# include "modules/dom/src/opera/jsopera.h"
# include "modules/dom/src/opera/domformdata.h"
# include "modules/dom/src/opera/domhttp.h"
# include "modules/dom/src/opera/domxslt.h"
# include "modules/dom/src/opera/domselection.h"
# include "modules/dom/src/feeds/domfeed.h"
# include "modules/dom/src/feeds/domfeedentry.h"
# include "modules/dom/src/feeds/domfeedreader.h"
# include "modules/dom/src/feeds/domwebfeeds.h"

#ifdef SVG_SUPPORT
# include "modules/dom/src/domsvg/domsvgenum.h"
#endif // SVG_SUPPORT

#ifdef WEBSERVER_SUPPORT
# include "modules/dom/src/domwebserver/domwebserver.h"
#endif // WEBSERVER_SUPPORT

#ifdef UPNP_SUPPORT
# include "modules/dom/src/domwebserver/domupnp.h"
#endif // UPNP_SUPPORT

#ifdef DOM_GADGET_FILE_API_SUPPORT
# include "modules/dom/src/opera/domgadgetfile.h"
#endif // DOM_GADGET_FILE_API_SUPPORT

	/* Media */
#ifdef MEDIA_HTML_SUPPORT
# include "modules/dom/src/media/domtimeranges.h"
# include "modules/dom/src/media/domtexttrack.h"
#endif // MEDIA_HTML_SUPPORT

#ifdef DOM_GEOLOCATION_SUPPORT
# include "modules/dom/src/js/geoloc.h"
#endif // DOM_GEOLOCATION_SUPPORT

#ifdef CSS_TRANSITIONS
# include "modules/dom/src/domevents/domtransitionevent.h"
#endif // CSS_TRANSITIONS

#ifdef CSS_ANIMATIONS
# include "modules/dom/src/domevents/domanimationevent.h"
#endif // CSS_ANIMATIONS

#ifdef GADGET_SUPPORT
# include "modules/dom/src/opera/domwidget.h"
# include "modules/dom/src/opera/domwidgetmanager.h"
# include "modules/dom/src/opera/domwidgetwindow.h"
# if defined WEBSERVER_SUPPORT && defined OPERAUNITE_URL
#  include "modules/dom/src/opera/domunitedevicemanager.h"
# endif
#endif // GADGET_SUPPORT
#include "modules/dom/src/opera/domoperaaccountmanager.h"

#ifdef CLIENTSIDE_STORAGE_SUPPORT
# include "modules/dom/src/storage/storage.h"
# include "modules/dom/src/storage/storageevent.h"
#endif // CLIENTSIDE_STORAGE_SUPPORT

#ifdef DATABASE_STORAGE_SUPPORT
# include "modules/dom/src/storage/database.h"
# include "modules/dom/src/storage/sqlresult.h"
# include "modules/dom/src/storage/sqltransaction.h"
#endif // DATABASE_STORAGE_SUPPORT)

#ifdef DOM_JIL_API_SUPPORT
#include "modules/dom/src/domjil/domjilpim.h"
#include "modules/dom/src/domjil/domjiladdressbookitem.h"
#include "modules/dom/src/domjil/domjildevice.h"
#include "modules/dom/src/domjil/domjildeviceinfo.h"
#include "modules/dom/src/domjil/domjildevicestateinfo.h"
#include "modules/dom/src/domjil/domjilradioinfo.h"
#include "modules/dom/src/domjil/domjildatanetworkinfo.h"
#include "modules/dom/src/domjil/domjilmultimedia.h"
#include "modules/dom/src/domjil/domjilmessaging.h"
#include "modules/dom/src/domjil/domjilmessage.h"
#include "modules/dom/src/domjil/domjilaudioplayer.h"
#include "modules/dom/src/domjil/domjilcamera.h"
#include "modules/dom/src/domjil/domjilaccountinfo.h"
#include "modules/dom/src/domjil/domjilaccelerometerinfo.h"
#include "modules/dom/src/domjil/domjiltelephony.h"
#include "modules/dom/src/domjil/domjilwidget.h"
#endif // DOM_JIL_API_SUPPORT

#ifdef APPLICATION_CACHE_SUPPORT
# include "modules/dom/src/domapplicationcache/domapplicationcache.h"
#endif // APPLICATION_CACHE_SUPPORT

#ifdef WEBSOCKETS_SUPPORT
# include "modules/dom/src/websockets/domwebsocket.h"
#endif //WEBSOCKETS_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
# include "modules/dom/src/domwebworkers/domwebworkers.h"
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
# include "modules/dom/src/domwebworkers/domcrossmessage.h"
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#ifdef EVENT_SOURCE_SUPPORT
# include "modules/dom/src/domevents/domeventsource.h"
#endif // EVENT_SOURCE_SUPPORT

#ifdef EXTENSION_SUPPORT
# include "modules/dom/src/extensions/domextensionscope.h"
# include "modules/dom/src/extensions/domextension_userjs.h"
# include "modules/dom/src/extensions/domextension_background.h"
# include "modules/dom/src/extensions/domextensionurlfilter.h"
# include "modules/dom/src/extensions/domextensionuiitem.h"
# include "modules/dom/src/extensions/domextensionuiitemevent.h"
# include "modules/dom/src/extensions/domextensionuipopup.h"
# include "modules/dom/src/extensions/domextensionuibadge.h"
# include "modules/dom/src/extensions/domextensionpagecontext.h"
# include "modules/dom/src/extensions/domextensionspeeddialcontext.h"
#endif // EXTENSION_SUPPORT
#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
# include "modules/dom/src/extensions/dombrowsertab.h"
# include "modules/dom/src/extensions/dombrowsertabs.h"
# include "modules/dom/src/extensions/dombrowsertabevent.h"
# include "modules/dom/src/extensions/dombrowsertabgroup.h"
# include "modules/dom/src/extensions/dombrowsertabgroups.h"
# include "modules/dom/src/extensions/dombrowsertabgroupevent.h"
# include "modules/dom/src/extensions/dombrowserwindows.h"
# include "modules/dom/src/extensions/dombrowserwindow.h"
# include "modules/dom/src/extensions/dombrowserwindowevent.h"
#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
# include "modules/dom/src/extensions/domextensionmenucontext.h"
# include "modules/dom/src/extensions/domextensionmenuitem.h"
# include "modules/dom/src/extensions/domextensionmenucontext_proxy.h"
# include "modules/dom/src/extensions/domextensionmenuitem_proxy.h"
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

class HTML_Element;
class DOM_NamedElements;

#ifdef DOM_JIL_API_SUPPORT
#include "modules/dom/src/domjil/domjilpim.h"
#include "modules/dom/src/domjil/domjiladdressbookitem.h"
#include "modules/dom/src/domjil/domjildevice.h"
#include "modules/dom/src/domjil/domjildeviceinfo.h"
#include "modules/dom/src/domjil/domjildevicestateinfo.h"
#include "modules/dom/src/domjil/domjilradioinfo.h"
#include "modules/dom/src/domjil/domjildatanetworkinfo.h"
#include "modules/dom/src/domjil/domjilmultimedia.h"
#include "modules/dom/src/domjil/domjilmessaging.h"
#include "modules/dom/src/domjil/domjilmessage.h"
#include "modules/dom/src/domjil/domjilaudioplayer.h"
#include "modules/dom/src/domjil/domjilcamera.h"
#include "modules/dom/src/domjil/domjilaccountinfo.h"
#include "modules/dom/src/domjil/domjilaccelerometerinfo.h"
#include "modules/dom/src/domjil/domjilwidget.h"
#include "modules/dom/src/domjil/domjilvideoplayer.h"
#include "modules/dom/src/domjil/domjilfile.h"
#include "modules/dom/src/domjil/domjilconfig.h"
#endif // DOM_JIL_API_SUPPORT

#ifdef  DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
#include "modules/dom/src/orientation/deviceorientationevent.h"
#include "modules/dom/src/orientation/devicemotionevent.h"
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT

#if defined DRAG_SUPPORT || defined USE_OP_CLIPBOARD
#include "modules/dom/src/domdatatransfer/domdatatransfer.h"
#endif // DRAG_SUPPORT || USE_OP_CLIPBOARD

#ifdef EXTENSION_SUPPORT
#define DOM_EXTENSION_ENABLED_USERJS_FLAG BOOL have_always_enabled_extension_js;
#else
#define DOM_EXTENSION_ENABLED_USERJS_FLAG
#endif // EXTENSION_SUPPORT

#ifdef USER_JAVASCRIPT
class DOM_UserJSSource;
class DOM_UserJSScript;

# define USERJS_MEMBERS	List<DOM_UserJSScript> userScripts; \
						List<DOM_UserJSSource> userScriptSources;
#else
# define USERJS_MEMBERS
#endif // USER_JAVASCRIPT

#include "modules/dom/src/media/domstream.h"

#ifdef DOM_NO_COMPLEX_GLOBALS
# define DOM_GLOBALDATA_START() \
	class DOM_GlobalData { \
	public: \
		DOM_GlobalData(); \
		Head *globalCallbacks; \
		Head *operaCallbacks; \
		TempBuffer tempbuf; \
		DOM_EXTENSION_ENABLED_USERJS_FLAG \
		ES_ValueString string_with_length_holder; \
		USERJS_MEMBERS
# define DOM_GLOBALDATA_END() };
#else // DOM_NO_COMPLEX_GLOBALS
# define DOM_GLOBALDATA_START() \
	class DOM_GlobalData { \
	public: \
		DOM_GlobalData(); \
		Head *globalCallbacks; \
		Head *operaCallbacks; \
		TempBuffer tempbuf; \
		DOM_EXTENSION_ENABLED_USERJS_FLAG \
		ES_ValueString string_with_length_holder; \
		USERJS_MEMBERS \
	};
# define DOM_GLOBALDATA_END()
#endif // DOM_NO_COMPLEX_GLOBALS


#ifdef DOM_NO_COMPLEX_GLOBALS
# define DOM_PROTOTYPES() DOM_PrototypeDesc DOM_Runtime_prototypes[DOM_Runtime::PROTOTYPES_COUNT + 1];
# define DOM_FUNCTIONS(cls) DOM_FunctionDesc cls##_functions[cls::FUNCTIONS_ARRAY_SIZE]
# define DOM_FUNCTIONS_WITH_DATA(cls) DOM_FunctionWithDataDesc cls##_functions_with_data[cls::FUNCTIONS_WITH_DATA_ARRAY_SIZE]
# ifdef DOM_LIBRARY_FUNCTIONS
#  define DOM_FUNCTIONS_LIBRARY(cls) DOM_LibraryFunctionDesc cls##_library_functions[cls::LIBRARY_FUNCTIONS_ARRAY_SIZE]
# else // DOM_LIBRARY_FUNCTIONS
#  define DOM_FUNCTIONS_LIBRARY(cls)
# endif // DOM_LIBRARY_FUNCTIONS
# define DOM_ATOMS() const char *atomNames[OP_ATOM_ABSOLUTELY_LAST_ENUM];
# define DOM_FEATURES() DOM_FeatureInfo featureList[DOM_FeatureInfo::FEATURES_COUNT + 1];
# define DOM_CONFIGURATION_PARAMETERS() DOM_DOMConfiguration_StaticParameter configurationParameters[DOM_DOMConfiguration_StaticParameter::PARAMETERS_COUNT + 1];
# define DOM_HTML_PROPERTIES() DOM_HTMLElement::SimpleProperties htmlProperties[DOM_Runtime::HTMLELEMENT_PROTOTYPES_COUNT];
# define DOM_HTML_CLASSNAMES() const char *htmlClassNames[DOM_Runtime::HTMLELEMENT_PROTOTYPES_COUNT];
# define DOM_PROTOTYPES_CLASSNAMES() const char *prototypeClassNames[DOM_Runtime::PROTOTYPES_COUNT + 1];
# define DOM_HTML_PROTOTYPES_CLASSNAMES() const char *htmlPrototypeClassNames[DOM_Runtime::HTMLELEMENT_PROTOTYPES_COUNT];
# define DOM_CONSTRUCTOR_NAMES() const char *constructorNames[DOM_Runtime::PROTOTYPES_COUNT + 1];
# ifdef SVG_DOM
#  define DOM_SVG_OBJECT_PROTOTYPES_CLASSNAMES() const char *svgObjectPrototypeClassNames[DOM_Runtime::SVGOBJECT_PROTOTYPES_COUNT];
#  define DOM_SVG_ELEMENT_PROTOTYPES_CLASSNAMES() const char *svgElementPrototypeClassNames[DOM_Runtime::SVGELEMENT_PROTOTYPES_COUNT + 1];
# endif // SVG_DOM
# define DOM_EVENTDATA() DOM_EventData eventData[DOM_EVENTS_COUNT + 1];
# define DOM_EVENTNSDATA() DOM_EventNamespaceData eventNamespaceData[DOM_EVENTS_WITH_NS_DATA_COUNT + 1];
#ifdef SVG_SUPPORT
# define DOM_SVG_ELEMENT_INTERFACES() unsigned int svg_if_inheritance_table[DOM_SVGElementInterface::SVG_NUM_ELEMENT_INTERFACES];
# define DOM_SVG_ELEMENT_SPEC() SVGElementSpec svg_element_spec[DOM_SVGElement::DOM_SVG_ELEMENT_COUNT];
# define DOM_SVG_ELEMENT_ENTRIES() DOM_SVGInterfaceEntry svg_element_entries[DOM_SVGObjectStore::SVG_NUM_ELEMENT_INTERFACE_ENTRIES];
# define DOM_SVG_OBJECT_ENTRIES() DOM_SVGInterfaceEntry svg_object_entries[DOM_SVGObjectStore::SVG_NUM_OBJECT_INTERFACE_ENTRIES];
#endif // SVG_SUPPORT
#else // DOM_NO_COMPLEX_GLOBALS
# define DOM_PROTOTYPES() extern const DOM_PrototypeDesc DOM_Runtime_prototypes[DOM_Runtime::PROTOTYPES_COUNT + 1];
# define DOM_FUNCTIONS(cls) extern const DOM_FunctionDesc cls##_functions[cls::FUNCTIONS_ARRAY_SIZE]
# define DOM_FUNCTIONS_WITH_DATA(cls) extern const DOM_FunctionWithDataDesc cls##_functions_with_data[cls::FUNCTIONS_WITH_DATA_ARRAY_SIZE]
# ifdef DOM_LIBRARY_FUNCTIONS
#  define DOM_FUNCTIONS_LIBRARY(cls) extern const DOM_LibraryFunctionDesc cls##_library_functions[cls::LIBRARY_FUNCTIONS_ARRAY_SIZE]
# else // DOM_LIBRARY_FUNCTIONS
#  define DOM_FUNCTIONS_LIBRARY(cls)
# endif // DOM_LIBRARY_FUNCTIONS
# define DOM_ATOMS() extern const char *const g_DOM_atomNames[];
# define DOM_FEATURES() extern const DOM_FeatureInfo g_DOM_featureList[];
# define DOM_CONFIGURATION_PARAMETERS() extern const DOM_DOMConfiguration_StaticParameter configurationParameters[];
# define DOM_HTML_PROPERTIES() extern const DOM_HTMLElement::SimpleProperties g_DOM_htmlProperties[];
# define DOM_HTML_CLASSNAMES() extern const char *const g_DOM_htmlClassNames[];
# define DOM_PROTOTYPES_CLASSNAMES() extern const char *const g_DOM_prototypeClassNames[];
# define DOM_HTML_PROTOTYPES_CLASSNAMES() extern const char *const g_DOM_htmlPrototypeClassNames[];
# define DOM_CONSTRUCTOR_NAMES() extern const char *const g_DOM_constructorNames[];
# ifdef SVG_DOM
# define DOM_SVG_OBJECT_PROTOTYPES_CLASSNAMES() extern const char *const g_DOM_svgObjectPrototypeClassNames[];
# define DOM_SVG_ELEMENT_PROTOTYPES_CLASSNAMES() extern const char *const g_DOM_svgElementPrototypeClassNames[];
#endif // SVG_DOM
# define DOM_EVENTDATA() extern const DOM_EventData g_DOM_eventData[];
# define DOM_EVENTNSDATA() extern const DOM_EventNamespaceData g_DOM_eventNamespaceData[];
#ifdef SVG_SUPPORT
# define DOM_SVG_ELEMENT_INTERFACES() extern const unsigned int g_DOM_svg_if_inheritance_table[];
# define DOM_SVG_ELEMENT_SPEC() extern const SVGElementSpec g_DOM_svg_element_spec[];
# define DOM_SVG_ELEMENT_ENTRIES() extern const DOM_SVGInterfaceEntry g_DOM_svg_element_entries[];
# define DOM_SVG_OBJECT_ENTRIES() extern const DOM_SVGInterfaceEntry g_DOM_svg_object_entries[];
#endif // SVG_SUPPORT
#endif // DOM_NO_COMPLEX_GLOBALS


#define DOM_FUNCTIONS1(cls) DOM_FUNCTIONS(cls);
#define DOM_FUNCTIONS2(cls) DOM_FUNCTIONS_WITH_DATA(cls);
#define DOM_FUNCTIONS3(cls) DOM_FUNCTIONS(cls); DOM_FUNCTIONS_WITH_DATA(cls);
#define DOM_FUNCTIONS4(cls) DOM_FUNCTIONS(cls); DOM_FUNCTIONS_WITH_DATA(cls); DOM_FUNCTIONS_LIBRARY(cls);

DOM_GLOBALDATA_START()

	DOM_PROTOTYPES()

	/* One for each class that has functions and thus a prototype.  The three
	   variants DOM_FUNCTIONS{1,2,3} are for classes that only have functions
	   without data, only functions with data or both, respectively. */

#include "modules/dom/src/domglobaldata.inc"

	/* READ THIS NOW */
	/* When you add more functions, also include the header file. It's only used
	   in certain configurations so you might not notice if you forget it. */
	/* MAKE SURE YOU READ THIS JUST NOW */

	DOM_ATOMS()

	DOM_FEATURES()
	DOM_CONFIGURATION_PARAMETERS()

	DOM_HTML_PROPERTIES()
	DOM_HTML_CLASSNAMES()
	DOM_PROTOTYPES_CLASSNAMES()
	DOM_HTML_PROTOTYPES_CLASSNAMES()
#ifdef SVG_DOM
	DOM_SVG_OBJECT_PROTOTYPES_CLASSNAMES()
	DOM_SVG_ELEMENT_PROTOTYPES_CLASSNAMES()
#endif // SVG_DOM
	DOM_CONSTRUCTOR_NAMES()

#ifdef SVG_SUPPORT
	DOM_SVG_ELEMENT_INTERFACES()
	DOM_SVG_ELEMENT_SPEC()
	DOM_SVG_ELEMENT_ENTRIES()
	DOM_SVG_OBJECT_ENTRIES()
#endif // SVG_SUPPORT

	DOM_EVENTDATA()
#ifdef DOM3_EVENTS
	DOM_EVENTNSDATA()
#endif // DOM3_EVENTS

DOM_GLOBALDATA_END()


#define g_DOM_globalData (g_opera->dom_module.data)
#define g_DOM_globalCallbacks (g_DOM_globalData->globalCallbacks)
#define g_DOM_operaCallbacks (g_DOM_globalData->operaCallbacks)
#define g_DOM_userScripts (g_DOM_globalData->userScripts)
#define g_DOM_userScriptSources (g_DOM_globalData->userScriptSources)
#define g_DOM_tempbuf (g_DOM_globalData->tempbuf)
#ifdef EXTENSION_SUPPORT
#define g_DOM_have_always_enabled_extension_js (g_DOM_globalData->have_always_enabled_extension_js)
#endif // EXTENSION_SUPPORT

#ifdef DOM_NO_COMPLEX_GLOBALS
# define g_DOM_atomNames (g_DOM_globalData->atomNames)
# define g_DOM_featureList (g_DOM_globalData->featureList)
# define g_DOM_configurationParameters (g_DOM_globalData->configurationParameters)
# define g_DOM_htmlProperties (g_DOM_globalData->htmlProperties)
# define g_DOM_htmlClassNames (g_DOM_globalData->htmlClassNames)
# define g_DOM_prototypeClassNames (g_DOM_globalData->prototypeClassNames)
# define g_DOM_htmlPrototypeClassNames (g_DOM_globalData->htmlPrototypeClassNames)
# define g_DOM_svgObjectPrototypeClassNames (g_DOM_globalData->svgObjectPrototypeClassNames)
# define g_DOM_svgElementPrototypeClassNames (g_DOM_globalData->svgElementPrototypeClassNames)
# define g_DOM_constructorNames (g_DOM_globalData->constructorNames)
# define g_DOM_eventData (g_DOM_globalData->eventData)
# define g_DOM_eventNamespaceData (g_DOM_globalData->eventNamespaceData)
#ifdef SVG_SUPPORT
# define g_DOM_svg_if_inheritance_table (g_DOM_globalData->svg_if_inheritance_table)
# define g_DOM_svg_element_spec (g_DOM_globalData->svg_element_spec)
# define g_DOM_svg_element_entries (g_DOM_globalData->svg_element_entries)
# define g_DOM_svg_object_entries (g_DOM_globalData->svg_object_entries)
#endif // SVG_SUPPORT
#endif // DOM_NO_COMPLEX_GLOBALS


#undef DOM_GLOBALDATA_START
#undef DOM_GLOBALDATA_FUNCTIONS
#undef DOM_PROTOTYPES
#undef DOM_FUNCTIONS
#undef DOM_FUNCTIONS_WITH_DATA
#undef DOM_FUNCTIONS1
#undef DOM_FUNCTIONS2
#undef DOM_FUNCTIONS3
#undef DOM_EVENTDATA
#undef DOM_EVENTNSDATA
#undef DOM_GLOBALDATA_END


#endif // DOM_DOMGLOBALDATA_H
