/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domruntime.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/opatom.h"
#include "modules/dom/src/domcore/chardata.h"
#include "modules/dom/src/domcore/comment.h"
#include "modules/dom/src/domcore/domconfiguration.h"
#include "modules/dom/src/domcore/domexception.h"
#include "modules/dom/src/domcore/domstringlist.h"
#include "modules/dom/src/domcore/domtokenlist.h"
#include "modules/dom/src/domcore/text.h"
#include "modules/dom/src/domcss/cssstyledeclaration.h"
#include "modules/dom/src/domcss/cssstylesheet.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/dommutationevent.h"
#include "modules/dom/src/domevents/domtouchevent.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/dom/src/domhtml/htmldoc.h"
#include "modules/dom/src/domhtml/htmlelem.h"
#include "modules/dom/src/domfile/domfileerror.h"
#include "modules/dom/src/domload/lsexception.h"
#include "modules/dom/src/domload/lsparser.h"
#include "modules/dom/src/domtraversal/nodeiterator.h"
#include "modules/dom/src/domtraversal/treewalker.h"
#include "modules/dom/src/domrange/range.h"
#include "modules/dom/src/domsave/lsserializer.h"
#include "modules/dom/src/domstylesheets/stylesheetlist.h"
#include "modules/dom/src/domstylesheets/medialist.h"
#include "modules/dom/src/domstylesheets/mediaquerylist.h"
#include "modules/dom/src/domxpath/xpathexception.h"
#include "modules/dom/src/js/history.h"
#include "modules/dom/src/js/location.h"
#include "modules/dom/src/js/navigat.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/opera/jsopera.h"
#include "modules/dom/src/opera/domhttp.h"
#include "modules/dom/src/opera/domio.h"
#include "modules/dom/src/domsvg/domsvgelement.h"
#include "modules/dom/src/media/dommediaerror.h"

#include "modules/doc/frm_doc.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/util/simset.h"
#include "modules/url/url_man.h"
#ifdef EXTENSION_SUPPORT
# include "modules/gadgets/OpGadget.h"
#endif // EXTENSION_SUPPORT

#ifdef PI_UIINFO_TOUCH_EVENTS
#include "modules/pi/ui/OpUiInfo.h"
#endif // PI_UIINFO_TOUCH_EVENTS

#include "modules/prefs/prefsmanager/collections/pc_js.h"

#ifdef DOM_NO_COMPLEX_GLOBALS
# define DOM_PROTOTYPE_CLASSNAMES_START() void DOM_prototypeClassNames_Init(DOM_GlobalData *global_data) { const char **classnames = global_data->prototypeClassNames;
# define DOM_PROTOTYPE_CLASSNAMES_ITEM(name) *classnames = name "Prototype"; ++classnames;
# define DOM_PROTOTYPE_CLASSNAMES_ITEM_LAST(name) *classnames = name "Prototype";
# define DOM_PROTOTYPE_CLASSNAMES_END() }
# define DOM_CONSTRUCTOR_NAMES_START() void DOM_constructorNames_Init(DOM_GlobalData *global_data) { const char **names = global_data->constructorNames;
# define DOM_CONSTRUCTOR_NAMES_ITEM(name) *names = name; ++names;
# define DOM_CONSTRUCTOR_NAMES_ITEM_LAST(name) *names = name;
# define DOM_CONSTRUCTOR_NAMES_END() }
#else // DOM_NO_COMPLEX_GLOBALS
# define DOM_PROTOTYPE_CLASSNAMES_START() const char *const g_DOM_prototypeClassNames[] = {
# define DOM_PROTOTYPE_CLASSNAMES_ITEM(name) name "Prototype",
# define DOM_PROTOTYPE_CLASSNAMES_ITEM_LAST(name) name "Prototype"
# define DOM_PROTOTYPE_CLASSNAMES_END() };
# define DOM_CONSTRUCTOR_NAMES_START() const char *const g_DOM_constructorNames[] = {
# define DOM_CONSTRUCTOR_NAMES_ITEM(name) name,
# define DOM_CONSTRUCTOR_NAMES_ITEM_LAST(name) name
# define DOM_CONSTRUCTOR_NAMES_END() };
#endif // DOM_NO_COMPLEX_GLOBALS

#include "modules/dom/src/domruntime.cpp.inc"

DOM_Runtime::DOM_Runtime()
	: environment(NULL)
	, origin(NULL)
	, owns_environment(TRUE)
	, cached_target_runtime(NULL)
	, cached_source_runtime(NULL)
	, prototypes(NULL)
	, constructors(NULL)
#ifdef SVG_DOM
	, svgobject_prototypes(NULL)
	, svgelement_prototypes(NULL)
#endif // SVG_DOM
	, htmlelement_prototypes(NULL)
	, htmlelement_constructors(NULL)
#ifdef SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
	, relaxed_load_save_security(FALSE)
#endif // SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
	, type(TYPE_DOCUMENT)
{
}

DOM_Runtime::~DOM_Runtime()
{
	ClearTargetSecurityCheckCache();
	ClearSourceSecurityCheckCache();

	while (AccessedProxyEnvironment *ape = static_cast<AccessedProxyEnvironment *>(accessed_proxy_environments.First()))
	{
		ape->Out();
		ape->environment->RuntimeDestroyed(this, ape->group);
		delete ape;
	}

	if (origin)
	{
		origin->DecRef();
		origin = NULL;
	}
	if (owns_environment)
		OP_DELETE(environment);
	OP_DELETEA(prototypes);
	OP_DELETEA(constructors);
	OP_DELETEA(htmlelement_prototypes);
	OP_DELETEA(htmlelement_constructors);
#ifdef SVG_DOM
	OP_DELETEA(svgelement_prototypes);
	OP_DELETEA(svgobject_prototypes);
#endif // SVG_DOM
}

/* virtual */ void
DOM_Runtime::GCTrace()
{
	/* If not an owned environment, the runtime's reference to it must remain alive
	 * until the runtime isn't otherwise traceable. Indirectly achieve this by marking
	 * the runtime of that environment. */
	if (!owns_environment)
		environment->GetRuntime()->GCMarkRuntime();

	if (ES_ThreadScheduler *scheduler = environment->GetScheduler())
		scheduler->GCTrace();
}

#ifdef DOM_WEBWORKERS_SUPPORT
OP_STATUS
DOM_Runtime::Construct(Type type, DOM_EnvironmentImpl *environment, URL base_worker_url)
{
	OP_ASSERT(type == TYPE_DEDICATED_WEBWORKER || type == TYPE_SHARED_WEBWORKER);
	this->type = type;

	prototypes = OP_NEWA(ES_Object *, PROTOTYPES_COUNT);
	if (!prototypes)
		return OpStatus::ERR_NO_MEMORY;

	constructors = OP_NEWA(DOM_Object *, PROTOTYPES_COUNT);
	if (!constructors)
		return OpStatus::ERR_NO_MEMORY;

	htmlelement_prototypes = NULL;
	htmlelement_constructors = NULL;

#ifdef SVG_DOM
	svgobject_prototypes  = NULL;
	svgelement_prototypes = NULL;
#endif // SVG_DOM

	op_memset(prototypes, 0, sizeof *prototypes * PROTOTYPES_COUNT);
	op_memset(constructors, 0, sizeof *constructors * PROTOTYPES_COUNT);

	origin = DocumentOrigin::Make(base_worker_url);
	if (!origin)
		return OpStatus::ERR_NO_MEMORY;

#ifdef ECMASCRIPT_NATIVE_SUPPORT
	BOOL use_native_dispatcher = !!g_pcjs->GetIntegerPref(PrefsCollectionJS::JIT);
#else // ECMASCRIPT_NATIVE_SUPPORT
	BOOL use_native_dispatcher = FALSE;
#endif // ECMASCRIPT_NATIVE_SUPPORT

	ES_Runtime *parent_runtime = NULL; /* No heap sharing with parent. */
	RETURN_IF_ERROR(ES_Runtime::Construct(NULL, "WorkerGlobalScope", use_native_dispatcher, parent_runtime, FALSE, TRUE));

	this->environment = environment;

	RETURN_IF_ERROR(DOM_WebWorker::ConstructGlobalScope(type == TYPE_DEDICATED_WEBWORKER, this));

	return OpStatus::OK;
}
#endif // DOM_WEBWORKERS_SUPPORT

OP_STATUS
DOM_Runtime::Construct(Type type, DOM_EnvironmentImpl *environment, const char* classname, DocumentOrigin* doc_origin, BOOL owns_environment, ES_Runtime *parent_runtime0)
{
	this->type = type;

	prototypes = OP_NEWA(ES_Object *, PROTOTYPES_COUNT);
	if (!prototypes)
		return OpStatus::ERR_NO_MEMORY;

	constructors = OP_NEWA(DOM_Object *, PROTOTYPES_COUNT);
	if (!constructors)
		return OpStatus::ERR_NO_MEMORY;

	htmlelement_prototypes = OP_NEWA(ES_Object *, HTMLELEMENT_PROTOTYPES_COUNT);
	if (!htmlelement_prototypes)
		return OpStatus::ERR_NO_MEMORY;

	// One constructor per prototype
	htmlelement_constructors = OP_NEWA(DOM_Object *, HTMLELEMENT_PROTOTYPES_COUNT);
	if (!htmlelement_constructors)
		return OpStatus::ERR_NO_MEMORY;

#ifdef SVG_DOM
	svgobject_prototypes = OP_NEWA(ES_Object *, SVGOBJECT_PROTOTYPES_COUNT);
	if (!svgobject_prototypes)
		return OpStatus::ERR_NO_MEMORY;

	svgelement_prototypes = OP_NEWA(ES_Object *, SVGELEMENT_PROTOTYPES_COUNT);
	if (!svgelement_prototypes)
		return OpStatus::ERR_NO_MEMORY;
#endif // SVG_DOM

	op_memset(prototypes, 0, sizeof *prototypes * PROTOTYPES_COUNT);
	op_memset(constructors, 0, sizeof *constructors * PROTOTYPES_COUNT);
	op_memset(htmlelement_prototypes, 0, sizeof *htmlelement_prototypes * HTMLELEMENT_PROTOTYPES_COUNT);
	op_memset(htmlelement_constructors, 0, sizeof *htmlelement_constructors * HTMLELEMENT_PROTOTYPES_COUNT);
#ifdef SVG_DOM
	op_memset(svgobject_prototypes, 0, sizeof *svgobject_prototypes * SVGOBJECT_PROTOTYPES_COUNT);
	op_memset(svgelement_prototypes, 0, sizeof *svgelement_prototypes * SVGELEMENT_PROTOTYPES_COUNT);
#endif // SVG_DOM

	origin = doc_origin;
	origin->IncRef();

#ifdef ECMASCRIPT_NATIVE_SUPPORT
	BOOL use_native_dispatcher = !!g_pcjs->GetIntegerPref(PrefsCollectionJS::JIT, DOM_EnvironmentImpl::GetHostName(GetFramesDocument()));
#else // ECMASCRIPT_NATIVE_SUPPORT
	BOOL use_native_dispatcher = FALSE;
#endif // ECMASCRIPT_NATIVE_SUPPORT

	ES_Runtime *parent_runtime = parent_runtime0;
	if (!parent_runtime && GetFramesDocument() && GetFramesDocument()->GetParentDoc())
		parent_runtime = GetFramesDocument()->GetParentDoc()->GetESRuntime();

	RETURN_IF_ERROR(ES_Runtime::Construct(NULL, classname, use_native_dispatcher, parent_runtime, FALSE, TRUE));

	this->environment = environment;
	this->owns_environment = owns_environment;
	switch (type)
	{
	default:
		OP_ASSERT(!"Unexpected runtime type");
	case TYPE_DOCUMENT:
		RETURN_IF_ERROR(JS_Window::Make(environment->window, this));
		break;
#ifdef EXTENSION_SUPPORT
	case TYPE_EXTENSION_JS:
		RETURN_IF_ERROR(DOM_ExtensionScope::Make(this));
		break;
#endif // EXTENSION_SUPPORT
	}


	return OpStatus::OK;
}

void
DOM_Runtime::Detach()
{
	for (AccessedProxyEnvironment *ape = static_cast<AccessedProxyEnvironment *>(accessed_proxy_environments.First());
		 ape;
		 ape = static_cast<AccessedProxyEnvironment *>(ape->Suc()))
		ape->environment->RuntimeDetached(this, ape->group);

	ES_Runtime::Detach();
}

void
DOM_Runtime::Reset()
{
	ES_Runtime::Detach();
	environment = NULL;
}

BOOL
DOM_Runtime::HasOverriddenDomain()
{
	return origin->has_overridden_domain == DOMAIN_OVERRIDDEN;
}

const uni_char *
DOM_Runtime::GetDomain()
{
	return origin->GetEffectiveDomain();
}

void
DOM_Runtime::GetDomain(const uni_char **the_domain, URLType *the_type, int *the_port)
{
	if (the_domain != NULL)
		*the_domain = GetDomain();

	if (the_type != NULL)
		*the_type = origin->security_context.Type();

	if (the_port != NULL)
		*the_port = origin->security_context.GetAttribute(URL::KServerPort, FALSE);
}

OP_STATUS
DOM_Runtime::SetDomainChecked(const uni_char *new_domain)
{
	if (origin->IsUniqueOrigin())
		return OpStatus::ERR;

	const uni_char *old_domain = GetDomain();

	if (uni_stricmp(old_domain, new_domain) != 0)
	{
		size_t old_length = uni_strlen(old_domain);
		size_t new_length = uni_strlen(new_domain);

		// New domain must be a suffix of old domain.
		if (new_length <= 1 || // importantly catching "."
			old_length <= new_length ||
			uni_strnicmp(old_domain + (old_length - new_length), new_domain, new_length) != 0 ||
			old_domain[old_length - new_length - 1] != '.')
			return OpStatus::ERR;

		// Check if we only had an IP number (so that the suffix comparison doesn't work)
		unsigned digits = 0, periods = 0;
		for (unsigned index = 0; index < old_length; ++index)
			if (old_domain[index] >= '0' && old_domain[index] <= '9')
				++digits;
			else if (old_domain[index] == '.')
				++periods;
		if (periods == 3 && digits + periods == old_length)
			return OpStatus::ERR;

		BOOL is_public_domain;
		RETURN_IF_ERROR(g_secman_instance->IsPublicDomain(new_domain, is_public_domain));
		if (is_public_domain)
			return OpStatus::ERR; // Don't allow setting domain to com or co.uk or similar
	}

	OP_ASSERT(new_domain);
	ClearTargetSecurityCheckCache();
	ClearSourceSecurityCheckCache();

	return origin->OverrideEffectiveDomain(new_domain);
}

ES_Object *
DOM_Runtime::GetPrototype(Prototype type)
{
	if (prototypes[type])
		return prototypes[type];
	else
	{
#ifdef DOM_NO_COMPLEX_GLOBALS
		const DOM_PrototypeDesc &prototype_desc = g_DOM_globalData->DOM_Runtime_prototypes[type];
#else // DOM_NO_COMPLEX_GLOBALS
		const DOM_PrototypeDesc &prototype_desc = DOM_Runtime_prototypes[type];
#endif // DOM_NO_COMPLEX_GLOBALS

		ES_Object *prototype_prototype;

		if (prototype_desc.prototype == -1)
			prototype_prototype = GetObjectPrototype();
		else if (prototype_desc.prototype == -2)
			prototype_prototype = GetErrorPrototype();
		else
			prototype_prototype = GetPrototype((Prototype) prototype_desc.prototype);

		ES_Object *prototype;
		const char *constructor_name = GetConstructorName(type);
		DOM_Object **constructor_ptr = &constructors[type];

		if (!prototype_prototype || OpStatus::IsError(DOM_Prototype::Make(prototype, prototype_prototype, GetPrototypeClass(type), constructor_name, prototype_desc.functions, prototype_desc.functions_with_data, this)))
			return NULL;

		if (OpStatus::IsError(PreparePrototype(prototype, type)))
			return NULL;

		DOM_Object *global_object = environment->GetWindow();

#ifdef EXTENSION_SUPPORT
		if (HasSharedEnvironment())
			global_object = DOM_GetHostObject(GetGlobalObjectAsPlainObject());
#endif // EXTENSION_SUPPORT

		OP_ASSERT(global_object);

		ES_Value constructor_value;
		if (*constructor_ptr)
			DOM_Object::DOMSetObject(&constructor_value, *constructor_ptr);
		else if (OpStatus::IsMemoryError(CreateConstructor(&constructor_value, global_object, constructor_name, CTN_BASIC, type)))
			return NULL;

		if (OpStatus::IsError(PutName(prototype, UNI_L("constructor"), constructor_value, PROP_DONT_ENUM)))
			return NULL;

		if (OpStatus::IsError(KeepAliveWithRuntime(prototype)))
			return NULL;

		return prototypes[type] = prototype;
	}
}

ES_Object *
DOM_Runtime::GetHTMLElementPrototype(HTMLElementPrototype type)
{
	if (htmlelement_prototypes[type])
		return htmlelement_prototypes[type];
	else
	{
		ES_Object *prototype_prototype;

#ifdef MEDIA_HTML_SUPPORT
		if (type == AUDIO_PROTOTYPE || type == VIDEO_PROTOTYPE)
			prototype_prototype = GetHTMLElementPrototype(MEDIA_PROTOTYPE);
		else
#endif // MEDIA_HTML_SUPPORT
			prototype_prototype = GetPrototype(HTMLELEMENT_PROTOTYPE);

		ES_Object *prototype;
		const char *class_name = GetHTMLPrototypeClass(type);

		if (!prototype_prototype || OpStatus::IsError(DOM_Prototype::Make(prototype, prototype_prototype, class_name, class_name, NULL, NULL, this)))
			return NULL;

		ES_Value constructor;
		if (htmlelement_constructors[type])
			DOM_Object::DOMSetObject(&constructor, htmlelement_constructors[type]);
		else
		{
			TRAPD(status, DOM_Object::DOMSetObject(&constructor, DOM_HTMLElement::CreateConstructorL(this, environment->GetWindow(), type)));
			if (OpStatus::IsError(status))
				return NULL;
		}

		if (OpStatus::IsError(PutName(prototype, UNI_L("constructor"), constructor, PROP_DONT_ENUM)))
			return NULL;

		if (OpStatus::IsError(DOM_HTMLElement_Prototype::Construct(prototype, type, this)))
			return NULL;

#ifdef MEDIA_HTML_SUPPORT
		if (type == MEDIA_PROTOTYPE)
		{
			TRAPD(status, DOM_HTMLMediaElement::ConstructHTMLMediaElementObjectL(prototype, this));

			if (OpStatus::IsError(status))
				return NULL;
		}
		else if (type == TRACK_PROTOTYPE)
		{
			TRAPD(status, DOM_HTMLTrackElement::ConstructHTMLTrackElementObjectL(prototype, this));

			if (OpStatus::IsError(status))
				return NULL;
		}
#endif // MEDIA_HTML_SUPPORT

		if (OpStatus::IsError(KeepAliveWithRuntime(prototype)))
			return NULL;

		return htmlelement_prototypes[type] = prototype;
	}
}

#ifdef SVG_DOM

ES_Object *
DOM_Runtime::GetSVGObjectPrototype(SVGObjectPrototype type)
{
	if (svgobject_prototypes[type])
		return svgobject_prototypes[type];
	else
	{
		ES_Object *prototype;
		const char *class_name = GetSVGObjectPrototypeClass(type);

		if (OpStatus::IsError(DOM_Prototype::Make(prototype, GetObjectPrototype(), class_name, class_name, NULL, NULL, this)))
			return NULL;

		if (OpStatus::IsError(DOM_SVGObject_Prototype::Construct(prototype, type, this)))
			return NULL;

		if (OpStatus::IsError(KeepAliveWithRuntime(prototype)))
			return NULL;

		return svgobject_prototypes[type] = prototype;
	}
}

ES_Object *
DOM_Runtime::GetSVGElementPrototype(SVGElementPrototype type, DOM_SVGElementInterface ifs)
{
	if (svgelement_prototypes[type])
		return svgelement_prototypes[type];
	else
	{
		ES_Object *prototype;
		ES_Object *element_prototype = GetPrototype(SVGELEMENT_PROTOTYPE);
		const char *class_name = GetSVGElementPrototypeClass(type);
		if (!element_prototype || OpStatus::IsError(DOM_Prototype::Make(prototype, element_prototype, class_name, class_name, NULL, NULL, this)))
			return NULL;

		if (OpStatus::IsError(DOM_SVGElement_Prototype::Construct(prototype, type, ifs, this)))
			return NULL;

		if (OpStatus::IsError(KeepAliveWithRuntime(prototype)))
			return NULL;

		return svgelement_prototypes[type] = prototype;
	}
}

#endif // SVG_DOM

Link *
DOM_Runtime::AddAccessedProxyEnvironment(DOM_ProxyEnvironmentImpl *environment, Link *group)
{
	AccessedProxyEnvironment *ape = OP_NEW(AccessedProxyEnvironment, (environment, group));
	if (ape)
		ape->Into(&accessed_proxy_environments);
	return ape;
}

class DOM_FindLongestConstructorName
	: public OpHashTableForEachListener
{
public:
	DOM_FindLongestConstructorName()
		: longest(0)
	{
	}

	virtual void HandleKeyData(const void *key0, void *)
	{
		const char *key = static_cast<const char *>(key0);
		unsigned key_length = op_strlen(key);

		if (key_length > longest)
			longest = key_length;
	}

	unsigned longest;
};

/* static */ void
DOM_Runtime::InitializeConstructorsTableL(OpString8HashTable<DOM_ConstructorInformation> *table, unsigned &longest_name)
{
#define ADD_CONSTRUCTOR(name, id) table->AddL(#name, OP_NEW_L(DOM_ConstructorInformation, (#name, CTN_BASIC, id##_PROTOTYPE)))

	ADD_CONSTRUCTOR(Node, NODE);
	ADD_CONSTRUCTOR(Element, ELEMENT);
	ADD_CONSTRUCTOR(Document, DOCUMENT);
	ADD_CONSTRUCTOR(DocumentFragment, DOCUMENTFRAGMENT);
	ADD_CONSTRUCTOR(Attr, ATTR);
	ADD_CONSTRUCTOR(Text, TEXT);
	ADD_CONSTRUCTOR(Comment, COMMENT);
	ADD_CONSTRUCTOR(CDATASection, CDATASECTION);
	ADD_CONSTRUCTOR(DocumentType, DOCUMENTTYPE);
	ADD_CONSTRUCTOR(Notation, NOTATION);
	ADD_CONSTRUCTOR(Entity, ENTITY);
	ADD_CONSTRUCTOR(EntityReference, ENTITYREFERENCE);
	ADD_CONSTRUCTOR(ProcessingInstruction, PROCESSINGINSTRUCTION);
	ADD_CONSTRUCTOR(NodeList, NODELIST);
	ADD_CONSTRUCTOR(NamedNodeMap, NAMEDNODEMAP);
	ADD_CONSTRUCTOR(DOMStringList, DOMSTRINGLIST);
	ADD_CONSTRUCTOR(DOMStringMap, DOMSTRINGMAP);
	ADD_CONSTRUCTOR(DOMTokenList, DOMTOKENLIST);
	ADD_CONSTRUCTOR(DOMSettableTokenList, DOMSETTABLETOKENLIST);
	ADD_CONSTRUCTOR(XMLDocument, XMLDOCUMENT);

	ADD_CONSTRUCTOR(HTMLCollection, HTMLCOLLECTION);
	ADD_CONSTRUCTOR(HTMLOptionsCollection, HTMLOPTIONSCOLLECTION);
	ADD_CONSTRUCTOR(HTMLDocument, HTMLDOCUMENT);
	ADD_CONSTRUCTOR(HTMLElement, HTMLELEMENT);

	ADD_CONSTRUCTOR(ClientRect, CLIENTRECT);
	ADD_CONSTRUCTOR(ClientRectList, CLIENTRECTLIST);

	ADD_CONSTRUCTOR(UIEvent, UIEVENT);
	ADD_CONSTRUCTOR(MouseEvent, MOUSEEVENT);
	ADD_CONSTRUCTOR(ErrorEvent, ERROREVENT);
	ADD_CONSTRUCTOR(DOMException, DOMEXCEPTION);
#ifdef DOM2_MUTATION_EVENTS
	ADD_CONSTRUCTOR(MutationEvent, MUTATIONEVENT);
#endif // DOM2_MUTATION_EVENTS
#ifdef CSS_ANIMATIONS
	ADD_CONSTRUCTOR(AnimationEvent, ANIMATIONEVENT);
#endif // CSS_ANIMATIONS
	ADD_CONSTRUCTOR(HashChangeEvent, HASHCHANGEEVENT);
#ifdef TOUCH_EVENTS_SUPPORT
# ifdef PI_UIINFO_TOUCH_EVENTS
	if (g_op_ui_info->IsTouchEventSupportWanted())
# endif // PI_UIINFO_TOUCH_EVENTS
	{
		ADD_CONSTRUCTOR(Touch, TOUCH);
		ADD_CONSTRUCTOR(TouchList, TOUCHLIST);
		ADD_CONSTRUCTOR(TouchEvent, TOUCHEVENT);
	}
#endif // TOUCH_EVENTS_SUPPORT

	ADD_CONSTRUCTOR(CSSStyleDeclaration, CSSSTYLEDECLARATION);

	// W3C File API
	ADD_CONSTRUCTOR(File, FILE);
	ADD_CONSTRUCTOR(FileList, FILELIST);
	ADD_CONSTRUCTOR(FileReader, FILEREADER);
	ADD_CONSTRUCTOR(FileError, FILEERROR);

	ADD_CONSTRUCTOR(CSSStyleSheet, CSSSTYLESHEET);
	ADD_CONSTRUCTOR(CSSRuleList, CSSRULELIST);
	ADD_CONSTRUCTOR(CSSMediaRule, CSSMEDIARULE);
	ADD_CONSTRUCTOR(CSSSupportsRule, CSSSUPPORTSRULE);
	ADD_CONSTRUCTOR(StyleSheetList, STYLESHEETLIST);
	ADD_CONSTRUCTOR(MediaList, MEDIALIST);

#ifdef DOM_WINDOW_PERFORMANCE_OBJECT_SUPPORT
	ADD_CONSTRUCTOR(Performance, PERFORMANCE);
#endif // DOM_WINDOW_PERFORMANCE_OBJECT_SUPPORT

#ifdef DOM_GADGET_FILE_API_SUPPORT
	// Used to be named File but that conflicts with the W3C File API spec.
	ADD_CONSTRUCTOR(GadgetFile, GADGETFILE);
	ADD_CONSTRUCTOR(FileStream, FILESTREAM);
#endif // DOM_GADGET_FILE_API_SUPPORT

#ifdef DOM2_RANGE
	ADD_CONSTRUCTOR(Range, RANGE);
#endif // DOM2_RANGE

#ifdef DOM3_LOAD
	ADD_CONSTRUCTOR(LSException, LSEXCEPTION);
	ADD_CONSTRUCTOR(LSParser, LSPARSER);
#endif // DOM3_LOAD

#ifdef DOM3_XPATH
	ADD_CONSTRUCTOR(XPathExpression, XPATHEXPRESSION);
	ADD_CONSTRUCTOR(XPathNSResolver, XPATHNSRESOLVER);
	ADD_CONSTRUCTOR(XPathNamespace, XPATHNAMESPACE);
	ADD_CONSTRUCTOR(XPathResult, XPATHRESULT);
	ADD_CONSTRUCTOR(XPathException, XPATHEXCEPTION);
#endif // DOM3_XPATH

#ifdef MEDIA_HTML_SUPPORT
	ADD_CONSTRUCTOR(MediaError, MEDIAERROR);
	ADD_CONSTRUCTOR(TimeRanges, TIMERANGES);
	ADD_CONSTRUCTOR(TextTrack, TEXTTRACK);
	ADD_CONSTRUCTOR(TextTrackList, TEXTTRACKLIST);
	// TextTrackCue is exposed in JS_Window::InitializeL
	ADD_CONSTRUCTOR(TextTrackCueList, TEXTTRACKCUELIST);
#endif // MEDIA_HTML_SUPPORT

#ifdef SVG_DOM
	ADD_CONSTRUCTOR(SVGDocument, SVGDOCUMENT);
	ADD_CONSTRUCTOR(SVGElement, SVGELEMENT);
#endif // SVG_DOM

#ifdef CANVAS_SUPPORT
	ADD_CONSTRUCTOR(CanvasRenderingContext2D, CANVASCONTEXT2D);
	ADD_CONSTRUCTOR(CanvasGradient, CANVASGRADIENT);
	ADD_CONSTRUCTOR(CanvasPattern, CANVASPATTERN);
	ADD_CONSTRUCTOR(ImageData, CANVASIMAGEDATA);
	ADD_CONSTRUCTOR(TextMetrics, TEXTMETRICS);
#endif // CANVAS_SUPPORT

#ifdef CANVAS3D_SUPPORT
	ADD_CONSTRUCTOR(WebGLRenderingContext, CANVASCONTEXTWEBGL);
	ADD_CONSTRUCTOR(WebGLObject, WEBGLOBJECT);
	ADD_CONSTRUCTOR(WebGLBuffer, WEBGLBUFFER);
	ADD_CONSTRUCTOR(WebGLFramebuffer, WEBGLFRAMEBUFFER);
	ADD_CONSTRUCTOR(WebGLProgram, WEBGLPROGRAM);
	ADD_CONSTRUCTOR(WebGLRenderbuffer, WEBGLRENDERBUFFER);
	ADD_CONSTRUCTOR(WebGLShader, WEBGLSHADER);
	ADD_CONSTRUCTOR(WebGLTexture, WEBGLTEXTURE);
	ADD_CONSTRUCTOR(WebGLUniformLocation, WEBGLUNIFORMLOCATION);
	ADD_CONSTRUCTOR(WebGLActiveInfo, WEBGLACTIVEINFO);
#endif // CANVAS3D_SUPPORT

#ifdef DATABASE_STORAGE_SUPPORT
	ADD_CONSTRUCTOR(Database, DATABASE);
	ADD_CONSTRUCTOR(SQLTransaction, SQLTRANSACTION);
	ADD_CONSTRUCTOR(SQLResultSet, SQLRESULTSET);
	ADD_CONSTRUCTOR(SQLResultSetRowList, SQLRESULTSETROWLIST);
	ADD_CONSTRUCTOR(SQLError, SQLERROR);
#endif //DATABASE_STORAGE_SUPPORT

#ifdef CLIENTSIDE_STORAGE_SUPPORT
	ADD_CONSTRUCTOR(Storage, STORAGE);
	ADD_CONSTRUCTOR(StorageEvent, STORAGEEVENT);
#endif // CLIENTSIDE_STORAGE_SUPPORT

#ifdef DOM_SELECTION_SUPPORT
	ADD_CONSTRUCTOR(Selection, WINDOWSELECTION);
#endif // DOM_SELECTION_SUPPORT

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
	ADD_CONSTRUCTOR(MessagePort, CROSSDOCUMENT_MESSAGEPORT);
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#ifdef EXTENSION_SUPPORT
	ADD_CONSTRUCTOR(UIItemEvent, EXTENSION_UIITEM_EVENT);
#endif // EXTENSION_SUPPORT

	ADD_CONSTRUCTOR(PopStateEvent, POPSTATEEVENT);

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	ADD_CONSTRUCTOR(DeviceOrientationEvent, DEVICEORIENTATION_EVENT);
	ADD_CONSTRUCTOR(DeviceMotionEvent, DEVICEMOTION_EVENT);
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT

#if defined DRAG_SUPPORT || defined USE_OP_CLIPBOARD
	ADD_CONSTRUCTOR(DataTransfer, DATATRANSFER);
	ADD_CONSTRUCTOR(DataTransferItem, DATATRANSFERITEM);
	ADD_CONSTRUCTOR(DataTransferItems, DATATRANSFERITEMS);
#endif // DRAG_SUPPORT || USE_OP_CLIPBOARD

#ifdef DRAG_SUPPORT
	ADD_CONSTRUCTOR(DragEvent, DRAGEVENT);
#endif // DRAG_SUPPORT

#ifdef USE_OP_CLIPBOARD
	ADD_CONSTRUCTOR(ClipboardEvent, CLIPBOARDEVENT);
#endif // USE_OP_CLIPBOARD

#undef ADD_CONSTRUCTOR

	DOM_HTMLElement::InitializeConstructorsTableL(table);

#ifdef SVG_DOM
	DOM_SVGElement::InitializeConstructorsTableL(table);
	DOM_SVGObject::InitializeConstructorsTableL(table);
#endif // SVG_DOM

	DOM_FindLongestConstructorName visitor;
	table->ForEach(&visitor);
	longest_name = visitor.longest;

	OP_ASSERT(longest_name < 64);
}

const char *
DOM_Runtime::GetPrototypeClass(Prototype prototype)
{
	return g_DOM_prototypeClassNames[prototype];
}

const char *
DOM_Runtime::GetHTMLPrototypeClass(HTMLElementPrototype prototype)
{
	return g_DOM_htmlPrototypeClassNames[prototype];
}

#ifdef SVG_DOM

const char *
DOM_Runtime::GetSVGObjectPrototypeClass(SVGObjectPrototype prototype)
{
	return g_DOM_svgObjectPrototypeClassNames[prototype];
}

const char *
DOM_Runtime::GetSVGElementPrototypeClass(SVGElementPrototype prototype)
{
	return g_DOM_svgElementPrototypeClassNames[prototype];
}

#endif // SVG_DOM

const char *
DOM_Runtime::GetConstructorName(Prototype prototype)
{
	return g_DOM_constructorNames[prototype];
}

ES_Object *
DOM_Runtime::CreateConstructorL(DOM_Object *target, const char *name, ConstructorTableNamespace ns, unsigned id)
{
	if (ns == CTN_BASIC)
	{
#ifdef DOM_GADGET_FILE_API_SUPPORT
		if (id == GADGETFILE_PROTOTYPE || id == FILESTREAM_PROTOTYPE)
			if (!(GetFramesDocument() && DOM_IO::AllowFileAPI(GetFramesDocument())))
				return NULL;
#endif // DOM_GADGET_FILE_API_SUPPORT

		DOM_Object *object = target->PutConstructorL(name, static_cast<Prototype>(id), TRUE);

		if (id == NODE_PROTOTYPE)
			DOM_Node::ConstructNodeObjectL(*object, this);
#ifdef DOM2_RANGE
		else if (id == RANGE_PROTOTYPE)
			DOM_Range::ConstructRangeObjectL(*object, this);
#endif // DOM2_RANGE
#ifdef DOM2_MUTATION_EVENTS
		else if (id == MUTATIONEVENT_PROTOTYPE)
			DOM_MutationEvent::ConstructMutationEventObjectL(*object, this);
#endif // DOM2_MUTATION_EVENTS
		else if (id == DOMEXCEPTION_PROTOTYPE)
			DOM_DOMException::ConstructDOMExceptionObjectL(*object, this);
		else if (id == FILEERROR_PROTOTYPE)
			DOM_FileError::ConstructFileErrorObjectL(*object, this);
#ifdef DOM_WEBWORKERS_SUPPORT
		else if (id == FILEEXCEPTION_PROTOTYPE)
			DOM_FileException::ConstructFileExceptionObjectL(*object, this);
#endif // DOM_WEBWORKERS_SUPPORT
#ifdef DOM3_LOAD
		else if (id == LSPARSER_PROTOTYPE)
			DOM_LSParser::ConstructLSParserObjectL(*object, this);
		else if (id == LSEXCEPTION_PROTOTYPE)
			DOM_LSException::ConstructLSExceptionObjectL(*object, this);
#endif // DOM3_LOAD
#ifdef DOM3_XPATH
		else if (id == XPATHNAMESPACE_PROTOTYPE)
			DOM_XPathNamespace::ConstructXPathNamespaceObjectL(*object, this);
		else if (id == XPATHRESULT_PROTOTYPE)
			DOM_XPathResult::ConstructXPathResultObjectL(*object, this);
		else if (id == XPATHEXCEPTION_PROTOTYPE)
			DOM_XPathException::ConstructXPathExceptionObjectL(*object, this);
#endif // DOM3_XPATH
#ifdef MEDIA_HTML_SUPPORT
		else if (id == MEDIAERROR_PROTOTYPE)
			DOM_MediaError::ConstructMediaErrorObjectL(*object, this);
#endif // MEDIA_HTML_SUPPORT
#ifdef CANVAS3D_SUPPORT
		else if (id == CANVASCONTEXTWEBGL_PROTOTYPE)
			DOMCanvasContextWebGL::ConstructCanvasContextWebGLObjectL(*object, this);
#endif //CANVAS3D_SUPPORT
#ifdef DATABASE_STORAGE_SUPPORT
		else if (id == SQLERROR_PROTOTYPE)
			DOM_SQLError::ConstructSQLErrorObjectL(*object, this);
#endif // DATABASE_STORAGE_SUPPORT

		return *object;
	}
#ifdef SVG_SUPPORT
	else if (ns == CTN_SVGELEMENT)
		return DOM_SVGElement::CreateConstructorL(this, target, id);
	else if (ns == CTN_SVGOBJECT)
		return DOM_SVGObject::CreateConstructorL(this, target, name, id);
#endif // SVG_SUPPORT
	else
		return DOM_HTMLElement::CreateConstructorL(this, target, id);
}

void DOM_Runtime::RecordConstructor(Prototype prototype, DOM_Object* constructor)
{
	OP_ASSERT(constructor);
	OP_ASSERT(!constructors[prototype]);

	if (OpStatus::IsSuccess(KeepAliveWithRuntime(constructor)))
		constructors[prototype] = constructor;
}

OP_STATUS
DOM_Runtime::CreateConstructor(ES_Value *value, DOM_Object *target, const char *name, unsigned ns, unsigned id)
{
	ES_Object *object = NULL;

	TRAPD(status, object = CreateConstructorL(target, name, static_cast<ConstructorTableNamespace>(ns), id));
	RETURN_IF_ERROR(status);

	if (object)
	{
		DOM_Object::DOMSetObject(value, object);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

OP_STATUS
DOM_Runtime::GetDisplayURL(OpString &display_url)
{
	URL url;
	if (origin->IsUniqueOrigin())
	{
		if (FramesDocument *doc = GetFramesDocument())
			url = doc->GetURL();
	}
	else
		url = origin->security_context;

	return url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, 0, display_url, URL::KNoRedirect);
}

#ifdef ECMASCRIPT_DEBUGGER

/* virtual */ const char *
DOM_Runtime::GetDescription() const
{
	switch (type)
	{
	default:
		OP_ASSERT(!"Please add a new case");
	case TYPE_DOCUMENT:
		return "document";
#ifdef DOM_WEBWORKERS_SUPPORT
	case TYPE_DEDICATED_WEBWORKER:
		return "webworker";
	case TYPE_SHARED_WEBWORKER:
		return "shared-webworker";
#endif // DOM_WEBWORKERS_SUPPORT
#ifdef EXTENSION_SUPPORT
	case TYPE_EXTENSION_JS:
		return "extensionjs";
#endif // EXTENSION_SUPPORT
	}
}

/* virtual */ OP_STATUS
DOM_Runtime::GetWindows(OpVector<Window> &windows)
{
	switch (type)
	{
	default:
		OP_ASSERT(!"Please add a new case");
	case TYPE_DOCUMENT:
		if (GetFramesDocument())
			return windows.Add(GetFramesDocument()->GetWindow());
		break;
#ifdef DOM_WEBWORKERS_SUPPORT
	case TYPE_DEDICATED_WEBWORKER:
	case TYPE_SHARED_WEBWORKER:
		{
			DOM_WebWorkerController *wwc = (environment ? environment->GetWorkerController() : NULL);
			DOM_WebWorker *worker = (wwc ? wwc->GetWorkerObject() : NULL);
			if (worker)
				return worker->GetWindows(windows);
		}
		break;
#endif // DOM_WEBWORKERS_SUPPORT
#ifdef EXTENSION_SUPPORT
	case TYPE_EXTENSION_JS:
		if (environment && environment->GetFramesDocument())
			return windows.Add(environment->GetFramesDocument()->GetWindow());
		break;
#endif // EXTENSION_SUPPORT
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_Runtime::GetURLDisplayName(OpString &str)
{
	URL url;

	switch (type)
	{
	default:
		OP_ASSERT(!"Please add a new case");
	case TYPE_DOCUMENT:
		if (GetFramesDocument())
			url = GetFramesDocument()->GetURL();
		break;
#ifdef DOM_WEBWORKERS_SUPPORT
	case TYPE_DEDICATED_WEBWORKER:
	case TYPE_SHARED_WEBWORKER:
		{
			DOM_WebWorkerController *wwc = (environment ? environment->GetWorkerController() : NULL);
			DOM_WebWorker *worker = (wwc ? wwc->GetWorkerObject() : NULL);
			DOM_WebWorkerDomain *domain = (worker ? worker->GetWorkerDomain() : NULL);
			if (domain)
				url = domain->GetOriginURL();
		}
		break;
#endif // DOM_WEBWORKERS_SUPPORT
#ifdef EXTENSION_SUPPORT
	case TYPE_EXTENSION_JS:
		if (environment && environment->GetFramesDocument())
			url = environment->GetFramesDocument()->GetURL();
		break;
#endif // EXTENSION_SUPPORT
	}

	str.Empty();
	RETURN_IF_MEMORY_ERROR(url.GetAttribute(URL::KUniName_Username_Password_Hidden, str));
	return OpStatus::OK;
}

# ifdef EXTENSION_SUPPORT
/* virtual */ OP_STATUS
DOM_Runtime::GetExtensionName(OpString &extension_name)
{
	if (type == TYPE_EXTENSION_JS)
		if (EcmaScript_Object *global_object = ES_Runtime::GetHostObject(GetGlobalObject()))
		{
			DOM_ExtensionScope *global_scope = static_cast<DOM_ExtensionScope *>(global_object);
			if (OpGadget *extension = global_scope->GetExtensionGadget())
				return extension->GetGadgetName(extension_name);
		}

	return OpStatus::ERR;
}
# endif // EXTENSION_SUPPORT

#endif // ECMASCRIPT_DEBUGGER

OP_STATUS
DOM_Runtime::PreparePrototype(ES_Object *object, Prototype type)
{
	TRAPD(status, PreparePrototypeL(object, type));
	return status;
}

void DOM_Runtime::RecordHTMLElementConstructor(HTMLElementPrototype prototype, DOM_Object* constructor)
{
	OP_ASSERT(constructor);
	OP_ASSERT(!htmlelement_constructors[prototype]);

	if (OpStatus::IsSuccess(KeepAliveWithRuntime(constructor)))
		htmlelement_constructors[prototype] = constructor;
}

OP_STATUS
DOM_Runtime::GetSerializedOrigin(TempBuffer &buffer, unsigned flags)
{
	URL origin_url = GetOriginURL();
	return DOM_Utils::GetSerializedOrigin(origin_url, buffer, flags);
}

