/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_DOMRUNTIME_H
#define DOM_DOMRUNTIME_H

#include "modules/ecmascript/ecmascript.h"
#include "modules/dom/domutils.h"
#include "modules/dom/src/dominternaltypes.h"
#include "modules/doc/documentorigin.h"

#ifdef SVG_DOM
# include "modules/dom/src/domsvg/domsvgenum.h"
#endif // SVG_DOM

class ES_Object;
class EcmaScript_Object;
class FramesDocument;
class DOM_EnvironmentImpl;
class DOM_ProxyEnvironmentImpl;
class DOM_Object;

#ifdef CLIENTSIDE_STORAGE_SUPPORT
class DOM_Storage_OperationCallback;
#endif //CLIENTSIDE_STORAGE_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
class DOM_WebWorker;
#endif // DOM_WEBWORKERS_SUPPORT

#include "modules/url/url2.h"

class DOM_ConstructorInformation
{
public:
	DOM_ConstructorInformation(const char *name, unsigned ns, unsigned id)
		: name(name),
		  ns(ns),
		  id(id)
	{
	}

	const char *name;
	unsigned ns:8;
	unsigned id:24;
};

class DOM_Runtime : public ES_Runtime
{
private:
	DOM_EnvironmentImpl *environment;

	DocumentOrigin* origin;
	BOOL owns_environment;

	DOM_Runtime *cached_target_runtime;
	/**< Target runtime in last successful security check where this
	     runtime was the source runtime. */

	DOM_Runtime *cached_source_runtime;
	/**< Source runtime in last successful security check where this
	     runtime was the target runtime. */

	void ClearTargetSecurityCheckCache()
	{
		if (cached_target_runtime)
		{
			cached_target_runtime->cached_source_runtime = NULL;
			cached_target_runtime = NULL;
		}
	}

	void ClearSourceSecurityCheckCache()
	{
		if (cached_source_runtime)
		{
			cached_source_runtime->cached_target_runtime = NULL;
			cached_source_runtime = NULL;
		}
	}

public:
	static void CacheSecurityCheck(DOM_Runtime *target_runtime, DOM_Runtime *source_runtime)
	{
		target_runtime->ClearSourceSecurityCheckCache();
		source_runtime->ClearTargetSecurityCheckCache();

		target_runtime->cached_source_runtime = source_runtime;
		source_runtime->cached_target_runtime = target_runtime;
	}

	static BOOL QuickSecurityCheck(DOM_Runtime *target_runtime, DOM_Runtime *source_runtime)
	{
		return target_runtime == source_runtime || target_runtime->cached_source_runtime == source_runtime;
	}

#include "modules/dom/src/domruntime.h.inc"

	/** Important: this enumeration needs to be kept in sync with:

	      g_DOM_htmlClassNames in domhtml/htmlelem.cpp
		  g_DOM_htmlPrototypeClassNames in in domhtml/htmlelem.cpp
	      g_DOM_htmlProperties in domhtml/htmlproperties.cpp

	    This means the elements must be in the same order in all four
	    places.  You cannot add or remove any elements without
	    updating all four. */
	enum HTMLElementPrototype
	{
		UNKNOWN_ELEMENT_PROTOTYPE,
		HTML_PROTOTYPE,
		HEAD_PROTOTYPE,
		LINK_PROTOTYPE,
		TITLE_PROTOTYPE,
		META_PROTOTYPE,
		BASE_PROTOTYPE,
		ISINDEX_PROTOTYPE,
		STYLE_PROTOTYPE,
		BODY_PROTOTYPE,
		FORM_PROTOTYPE,
		SELECT_PROTOTYPE,
		OPTGROUP_PROTOTYPE,
		OPTION_PROTOTYPE,
		INPUT_PROTOTYPE,
		TEXTAREA_PROTOTYPE,
		BUTTON_PROTOTYPE,
		LABEL_PROTOTYPE,
		FIELDSET_PROTOTYPE,
		LEGEND_PROTOTYPE,
		OUTPUT_PROTOTYPE,
		DATALIST_PROTOTYPE,
		PROGRESS_PROTOTYPE,
		METER_PROTOTYPE,
		KEYGEN_PROTOTYPE,
		ULIST_PROTOTYPE,
		OLIST_PROTOTYPE,
		DLIST_PROTOTYPE,
		DIRECTORY_PROTOTYPE,
		MENU_PROTOTYPE,
		LI_PROTOTYPE,
		DIV_PROTOTYPE,
		PARAGRAPH_PROTOTYPE,
		HEADING_PROTOTYPE,
		QUOTE_PROTOTYPE,
		PRE_PROTOTYPE,
		BR_PROTOTYPE,
		FONT_PROTOTYPE,
		HR_PROTOTYPE,
		MOD_PROTOTYPE,
#ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
		XML_PROTOTYPE,
#endif // #ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
		ANCHOR_PROTOTYPE,
		IMAGE_PROTOTYPE,
		OBJECT_PROTOTYPE,
		PARAM_PROTOTYPE,
		APPLET_PROTOTYPE,
		EMBED_PROTOTYPE,
		MAP_PROTOTYPE,
		AREA_PROTOTYPE,
		SCRIPT_PROTOTYPE,
		TABLE_PROTOTYPE,
		TABLECAPTION_PROTOTYPE,
		TABLECOL_PROTOTYPE,
		TABLESECTION_PROTOTYPE,
		TABLEROW_PROTOTYPE,
		TABLECELL_PROTOTYPE,
		FRAMESET_PROTOTYPE,
		FRAME_PROTOTYPE,
		IFRAME_PROTOTYPE,
#ifdef MEDIA_HTML_SUPPORT
		MEDIA_PROTOTYPE,
		AUDIO_PROTOTYPE,
		VIDEO_PROTOTYPE,
		SOURCE_PROTOTYPE,
		TRACK_PROTOTYPE,
#endif //MEDIA_HTML_SUPPORT
#ifdef CANVAS_SUPPORT
		CANVAS_PROTOTYPE,
#endif // CANVAS_SUPPORT
		MARQUEE_PROTOTYPE,
		TIME_PROTOTYPE,

		HTMLELEMENT_PROTOTYPES_COUNT
	};

#ifdef SVG_DOM
	/** Important: this enumeration needs to be kept in sync with:

	      g_DOM_svgObjectPrototypeClassNames in domsvg/domsvgobject.cpp

	    This means the elements must be in the same order in both places.
	    You cannot add or remove any elements without updating both. */
	enum SVGObjectPrototype
	{
		SVGNUMBER_PROTOTYPE,
		SVGLENGTH_PROTOTYPE,
		SVGPOINT_PROTOTYPE,
		SVGANGLE_PROTOTYPE,
		SVGTRANSFORM_PROTOTYPE,
		SVGPATHSEG_PROTOTYPE,
		SVGMATRIX_PROTOTYPE,
		SVGPAINT_PROTOTYPE,
		SVGASPECTRATIO_PROTOTYPE,
		SVGCSSPRIMITIVEVALUE_PROTOTYPE,
		SVGRECT_PROTOTYPE,
		SVGCSSRGBCOLOR_PROTOTYPE,
		SVGPATH_PROTOTYPE,
		SVGRGBCOLOR_PROTOTYPE,

		SVGOBJECT_PROTOTYPES_COUNT
	};

	/** Important: this enumeration needs to be kept in sync with:

	      g_DOM_svgElementPrototypeClassNames in domsvg/domsvgelement.cpp

	    This means the elements must be in the same order in both places.
	    You cannot add or remove any elements without updating both. */
	enum SVGElementPrototype
	{
		SVG_PROTOTYPE,
		SVG_CIRCLE_PROTOTYPE,
		SVG_ELLIPSE_PROTOTYPE,
		SVG_LINE_PROTOTYPE,
		SVG_PATH_PROTOTYPE,
		SVG_POLYLINE_PROTOTYPE,
		SVG_POLYGON_PROTOTYPE,
		SVG_RECT_PROTOTYPE,
		SVG_SVG_PROTOTYPE,
		SVG_TEXT_PROTOTYPE,
		SVG_TEXTCONTENT_PROTOTYPE,
		SVG_TSPAN_PROTOTYPE,
		SVG_TREF_PROTOTYPE,
		SVG_ANIMATE_PROTOTYPE,
		SVG_G_PROTOTYPE,
		SVG_A_PROTOTYPE,
		SVG_SCRIPT_PROTOTYPE,
		SVG_MPATH_PROTOTYPE,
		SVG_FONT_PROTOTYPE,
		SVG_GLYPH_PROTOTYPE,
		SVG_MISSINGGLYPH_PROTOTYPE,
		SVG_FOREIGN_OBJECT_PROTOTYPE,
		SVG_DEFS_PROTOTYPE,
		SVG_DESC_PROTOTYPE,
		SVG_TITLE_PROTOTYPE,
		SVG_SYMBOL_PROTOTYPE,
		SVG_USE_PROTOTYPE,
		SVG_IMAGE_PROTOTYPE,
		SVG_SWITCH_PROTOTYPE,
		SVG_LINEARGRADIENT_PROTOTYPE,
		SVG_RADIALGRADIENT_PROTOTYPE,
		SVG_STOP_PROTOTYPE,
#ifdef SVG_FULL_11
		SVG_PATTERN_PROTOTYPE,
		SVG_TEXTPATH_PROTOTYPE,
		SVG_MARKER_PROTOTYPE,
		SVG_VIEW_PROTOTYPE,
		SVG_MASK_PROTOTYPE,
		SVG_FEFLOOD_PROTOTYPE,
		SVG_FEBLEND_PROTOTYPE,
		SVG_FECOLORMATRIX_PROTOTYPE,
		SVG_FECOMPONENTTRANSFER_PROTOTYPE,
		SVG_FECOMPOSITE_PROTOTYPE,
		SVG_FECONVOLVEMATRIX_PROTOTYPE,
		SVG_FEDIFFUSELIGHTNING_PROTOTYPE,
		SVG_FEDISPLACEMENTMAP_PROTOTYPE,
		SVG_FEIMAGE_PROTOTYPE,
		SVG_FEMERGE_PROTOTYPE,
		SVG_FEMORPHOLOGY_PROTOTYPE,
		SVG_FEOFFSET_PROTOTYPE,
		SVG_FESPECULARLIGHTNING_PROTOTYPE,
		SVG_FETILE_PROTOTYPE,
		SVG_FETURBULENCE_PROTOTYPE,
		SVG_STYLE_PROTOTYPE,
		SVG_CLIPPATH_PROTOTYPE,
		SVG_FEFUNCR_PROTOTYPE,
		SVG_FEFUNCG_PROTOTYPE,
		SVG_FEFUNCB_PROTOTYPE,
		SVG_FEFUNCA_PROTOTYPE,
		SVG_FEDISTANTLIGHT_PROTOTYPE,
		SVG_FESPOTLIGHT_PROTOTYPE,
		SVG_FEPOINTLIGHT_PROTOTYPE,
		SVG_FEMERGENODE_PROTOTYPE,
		SVG_FEGAUSSIANBLUR_PROTOTYPE,
		SVG_FILTER_PROTOTYPE,
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
		SVG_AUDIO_PROTOTYPE,
		SVG_VIDEO_PROTOTYPE,
		SVG_ANIMATION_PROTOTYPE,
		SVG_TEXTAREA_PROTOTYPE,
#endif // SVG_TINY_12

		SVGELEMENT_PROTOTYPES_COUNT
	};
#endif // SVG_DOM

	enum Type
	{
		TYPE_DOCUMENT,
		/**< The main runtime of a document. (A "normal" runtime). */

#ifdef DOM_WEBWORKERS_SUPPORT

		TYPE_DEDICATED_WEBWORKER,
		/**< A dedicated web worker. */

		TYPE_SHARED_WEBWORKER,
		/**< A shared web worker. */

#endif // DOM_WEBWORKERS_SUPPORT

#ifdef EXTENSION_SUPPORT

		TYPE_EXTENSION_JS
		/**< A runtime that is running ExtensionJS with shared access to
		     a DOM environment. */

#endif // EXTENSION_SUPPORT

	}; // Type

public:
	DOM_Runtime();
	virtual ~DOM_Runtime();

	virtual void GCTrace();

	URL GetOriginURL() { return origin->security_context; }
	/**< Returns the Origin URL that together with the effective domain and other information
	     in the Origin object decides the security context for a runtime. */

	DocumentOrigin *GetMutableOrigin() { return origin; }
	/**< Returns the DocumentOrigin object that decides the security context for a runtime. */

	OP_STATUS GetSerializedOrigin(TempBuffer &buffer, unsigned flags = DOM_Utils::SERIALIZE_FALLBACK_AS_NULL);
	/**< Get a serialized representation of the runtime's (origin) URL.
	     See DOM_Utils::GetSerializedOrigin() documentation for details
	     on serialisation.

	     @param buffer The result buffer.
	     @param flags Flags, see OriginSerializationFlags. The default value
	            ensures specification compliance.
	     @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS GetDisplayURL(OpString& display_url);
	/**< Returns the URL most useful in public messages for this runtime. Typically the origin, but
	     in case of anonymous/sandboxed runtimes it might be something different.

	     @param[out] display_url Set to the url or left empty if there is no reasonable url.
	     @returns OpStatus::OK normally, OpStatus::ERR_NO_MEMORY on OOM. */

	const uni_char *GetDomain();

	void GetDomain(const uni_char **domain, URLType *type, int *port);
	OP_STATUS SetDomainChecked(const uni_char *domain);
	/**< Check if 'domain' is a valid domain (proper suffix of current domain)
	     and set it if it is.  Returns OpStatus::ERR if the domain was not
	     valid and OpStatus::ERR_NO_MEMORY on OOM. */
	BOOL HasOverriddenDomain();
	/**< Returns TRUE if the current domain is not from the document's URL,
	     but is set for instance by assigning document.domain.  (In fact, TRUE
	     if the domain has been changed by a call to SetDomainChecked.) */

	OP_STATUS Construct(Type type, DOM_EnvironmentImpl *environment, const char* classname, DocumentOrigin* doc_origin, BOOL owns_environment = TRUE, ES_Runtime *parent_runtime = NULL);
#ifdef DOM_WEBWORKERS_SUPPORT
	OP_STATUS Construct(Type type, DOM_EnvironmentImpl *environment, URL base_worker_url);
#endif // DOM_WEBWORKERS_SUPPORT

	void Detach();
	void Reset();

	DOM_EnvironmentImpl *GetEnvironment() { return environment; }
	BOOL HasSharedEnvironment() { return !owns_environment; }
	/**< Returns TRUE if this runtime runs in the context of another environment.
	     At present, only so for isolated userJS/extension runtimes. */

	ES_Object *GetPrototype(Prototype prototype);
	void RecordConstructor(Prototype prototype, DOM_Object* constructor);
	ES_Object *GetHTMLElementPrototype(HTMLElementPrototype prototype);
	void RecordHTMLElementConstructor(HTMLElementPrototype prototype, DOM_Object* constructor);
#ifdef SVG_DOM
	ES_Object* GetSVGObjectPrototype(SVGObjectPrototype prototype);
	ES_Object *GetSVGElementPrototype(SVGElementPrototype prototype, DOM_SVGElementInterface ifs);
#endif // SVG_DOM

#ifdef SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
	void SetRelaxedLoadSaveSecurity(BOOL value) { relaxed_load_save_security = value; }
	BOOL HasRelaxedLoadSaveSecurity() { return relaxed_load_save_security; }
#endif // SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK

	Link *AddAccessedProxyEnvironment(DOM_ProxyEnvironmentImpl *environment, Link *group);

	enum ConstructorTableNamespace
	{
		CTN_BASIC,
		CTN_HTMLELEMENT,
		CTN_SVGELEMENT,
		CTN_SVGOBJECT
	};

	static void InitializeConstructorsTableL(OpString8HashTable<DOM_ConstructorInformation> *table, unsigned &longest_name);

	const char *GetConstructorName(Prototype prototype);
	ES_Object *CreateConstructorL(DOM_Object *target, const char *name, ConstructorTableNamespace ns, unsigned id);
	OP_STATUS CreateConstructor(ES_Value *value, DOM_Object *target, const char *name, unsigned ns, unsigned id);

#ifdef ECMASCRIPT_DEBUGGER
	// Overriding ES_Runtime
	virtual const char *GetDescription() const;
	virtual OP_STATUS GetWindows(OpVector<Window> &windows);
	virtual OP_STATUS GetURLDisplayName(OpString &str);
# ifdef EXTENSION_SUPPORT
	virtual OP_STATUS GetExtensionName(OpString &extension_name);
# endif // EXTENSION_SUPPORT
#endif // ECMASCRIPT_DEBUGGER

private:
	OP_STATUS PreparePrototype(ES_Object *object, Prototype type);
	const char *GetPrototypeClass(Prototype prototype);
	const char *GetHTMLPrototypeClass(HTMLElementPrototype prototype);
#ifdef SVG_DOM
	const char *GetSVGObjectPrototypeClass(SVGObjectPrototype prototype);
	const char *GetSVGElementPrototypeClass(SVGElementPrototype prototype);
#endif // SVG_DOM

	ES_Object *SetPrototype(DOM_Object *&prototype, ES_Object *prototype_prototype, const char *object_class);
	/**< If object_class is NULL, 'Object' will be used. */
	/**< Traps and calls PreparePrototypeL(). */
	void PreparePrototypeL(ES_Object *object, Prototype type);
	/**< Autogenerated from prototypes.txt.  Lives in domprototypes.cpp. */

	ES_Object **prototypes;
	DOM_Object **constructors;
#ifdef SVG_DOM
	ES_Object **svgobject_prototypes;
	ES_Object **svgelement_prototypes;
#endif // SVG_DOM
	ES_Object **htmlelement_prototypes;
	DOM_Object **htmlelement_constructors;

#ifdef SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
	BOOL relaxed_load_save_security;
#endif // SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK

	class AccessedProxyEnvironment
		: public Link
	{
	public:
		AccessedProxyEnvironment(DOM_ProxyEnvironmentImpl *environment, Link *group)
			: environment(environment),
			  group(group)
		{
		}

		DOM_ProxyEnvironmentImpl *environment;
		Link *group;
	};

	Head accessed_proxy_environments;

	Type type;
};

#endif // DOM_DOMRUNTIME_H
