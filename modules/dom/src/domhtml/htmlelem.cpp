/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domhtml/htmlelem.h"

#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/docfrag.h"
#include "modules/dom/src/domcore/text.h"
#include "modules/dom/src/domcore/domstringmap.h"
#include "modules/dom/src/domcss/cssstyledeclaration.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventthread.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/dom/src/domhtml/htmldoc.h"
#include "modules/dom/src/js/location.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/opatom.h"
#include "modules/dom/src/webforms2/webforms2dom.h"

#ifdef DOCUMENT_EDIT_SUPPORT
#include "modules/documentedit/OpDocumentEdit.h"
#endif

#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/util/tempbuf.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/html5parser.h"
#include "modules/forms/form.h"
#include "modules/forms/formmanager.h"
#include "modules/forms/formsenum.h" // for the attributes constants
#include "modules/forms/formvalue.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/formvalueradiocheck.h"
#include "modules/forms/webforms2support.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/layout/layout_workplace.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/xmlutils/xmlserializer.h"

#ifdef DOM_NO_COMPLEX_GLOBALS
# define DOM_HTML_CLASSNAMES_START() void DOM_htmlClassNames_Init(DOM_GlobalData *global_data) { const char **classnames = global_data->htmlClassNames;
# define DOM_HTML_CLASSNAMES_ITEM(name) *classnames = "HTML" name "Element"; ++classnames;
# define DOM_HTML_CLASSNAMES_ITEM_LAST(name) *classnames = "HTML" name "Element";
# define DOM_HTML_CLASSNAMES_END() }
#else // DOM_NO_COMPLEX_GLOBALS
# define DOM_HTML_CLASSNAMES_START() const char *const g_DOM_htmlClassNames[] = {
# define DOM_HTML_CLASSNAMES_ITEM(name) "HTML" name "Element",
# define DOM_HTML_CLASSNAMES_ITEM_LAST(name) "HTML" name "Element"
# define DOM_HTML_CLASSNAMES_END() };
#endif // DOM_NO_COMPLEX_GLOBALS


/* Important: this array must be kept in sync with the enumeration
   DOM_Runtime::HTMLElementPrototype.  See its definition for what
   else it needs to be in sync with. */
DOM_HTML_CLASSNAMES_START()
	DOM_HTML_CLASSNAMES_ITEM("Unknown")
	DOM_HTML_CLASSNAMES_ITEM("Html")
	DOM_HTML_CLASSNAMES_ITEM("Head")
	DOM_HTML_CLASSNAMES_ITEM("Link")
	DOM_HTML_CLASSNAMES_ITEM("Title")
	DOM_HTML_CLASSNAMES_ITEM("Meta")
	DOM_HTML_CLASSNAMES_ITEM("Base")
	DOM_HTML_CLASSNAMES_ITEM("IsIndex")
	DOM_HTML_CLASSNAMES_ITEM("Style")
	DOM_HTML_CLASSNAMES_ITEM("Body")
	DOM_HTML_CLASSNAMES_ITEM("Form")
	DOM_HTML_CLASSNAMES_ITEM("Select")
	DOM_HTML_CLASSNAMES_ITEM("OptGroup")
	DOM_HTML_CLASSNAMES_ITEM("Option")
	DOM_HTML_CLASSNAMES_ITEM("Input")
	DOM_HTML_CLASSNAMES_ITEM("TextArea")
	DOM_HTML_CLASSNAMES_ITEM("Button")
	DOM_HTML_CLASSNAMES_ITEM("Label")
	DOM_HTML_CLASSNAMES_ITEM("FieldSet")
	DOM_HTML_CLASSNAMES_ITEM("Legend")
	DOM_HTML_CLASSNAMES_ITEM("Output")
	DOM_HTML_CLASSNAMES_ITEM("DataList")
	DOM_HTML_CLASSNAMES_ITEM("Progress")
	DOM_HTML_CLASSNAMES_ITEM("Meter")
	DOM_HTML_CLASSNAMES_ITEM("Keygen")
	DOM_HTML_CLASSNAMES_ITEM("UList")
	DOM_HTML_CLASSNAMES_ITEM("OList")
	DOM_HTML_CLASSNAMES_ITEM("DList")
	DOM_HTML_CLASSNAMES_ITEM("Directory")
	DOM_HTML_CLASSNAMES_ITEM("Menu")
	DOM_HTML_CLASSNAMES_ITEM("LI")
	DOM_HTML_CLASSNAMES_ITEM("Div")
	DOM_HTML_CLASSNAMES_ITEM("Paragraph")
	DOM_HTML_CLASSNAMES_ITEM("Heading")
	DOM_HTML_CLASSNAMES_ITEM("Quote")
	DOM_HTML_CLASSNAMES_ITEM("Pre")
	DOM_HTML_CLASSNAMES_ITEM("BR")
	DOM_HTML_CLASSNAMES_ITEM("Font")
	DOM_HTML_CLASSNAMES_ITEM("HR")
	DOM_HTML_CLASSNAMES_ITEM("Mod")
#ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
	DOM_HTML_CLASSNAMES_ITEM("Xml")
#endif // #ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
	DOM_HTML_CLASSNAMES_ITEM("Anchor")
	DOM_HTML_CLASSNAMES_ITEM("Image")
	DOM_HTML_CLASSNAMES_ITEM("Object")
	DOM_HTML_CLASSNAMES_ITEM("Param")
	DOM_HTML_CLASSNAMES_ITEM("Applet")
	DOM_HTML_CLASSNAMES_ITEM("Embed")
	DOM_HTML_CLASSNAMES_ITEM("Map")
	DOM_HTML_CLASSNAMES_ITEM("Area")
	DOM_HTML_CLASSNAMES_ITEM("Script")
	DOM_HTML_CLASSNAMES_ITEM("Table")
	DOM_HTML_CLASSNAMES_ITEM("TableCaption")
	DOM_HTML_CLASSNAMES_ITEM("TableCol")
	DOM_HTML_CLASSNAMES_ITEM("TableSection")
	DOM_HTML_CLASSNAMES_ITEM("TableRow")
	DOM_HTML_CLASSNAMES_ITEM("TableCell")
	DOM_HTML_CLASSNAMES_ITEM("FrameSet")
	DOM_HTML_CLASSNAMES_ITEM("Frame")
	DOM_HTML_CLASSNAMES_ITEM("IFrame")
#ifdef MEDIA_HTML_SUPPORT
	DOM_HTML_CLASSNAMES_ITEM("Media")
	DOM_HTML_CLASSNAMES_ITEM("Audio")
	DOM_HTML_CLASSNAMES_ITEM("Video")
	DOM_HTML_CLASSNAMES_ITEM("Source")
	DOM_HTML_CLASSNAMES_ITEM("Track")
#endif // MEDIA_HTML_SUPPORT
#ifdef CANVAS_SUPPORT
	DOM_HTML_CLASSNAMES_ITEM("Canvas")
#endif // CANVAS_SUPPORT
	DOM_HTML_CLASSNAMES_ITEM("Marquee")
	DOM_HTML_CLASSNAMES_ITEM_LAST("Time")
DOM_HTML_CLASSNAMES_END()

#undef DOM_HTML_CLASSNAMES_START
#undef DOM_HTML_CLASSNAMES_ITEM
#undef DOM_HTML_CLASSNAMES_ITEM_LAST
#undef DOM_HTML_CLASSNAMES_END

#ifdef DOM_NO_COMPLEX_GLOBALS
# define DOM_HTML_PROTOTYPE_CLASSNAMES_START() void DOM_htmlPrototypeClassNames_Init(DOM_GlobalData *global_data) { const char **classnames = global_data->htmlPrototypeClassNames;
# define DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM(name) *classnames = "HTML" name "ElementPrototype"; ++classnames;
# define DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM_LAST(name) *classnames = "HTML" name "ElementPrototype";
# define DOM_HTML_PROTOTYPE_CLASSNAMES_END() }
#else // DOM_NO_COMPLEX_GLOBALS
# define DOM_HTML_PROTOTYPE_CLASSNAMES_START() const char *const g_DOM_htmlPrototypeClassNames[] = {
# define DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM(name) "HTML" name "ElementPrototype",
# define DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM_LAST(name) "HTML" name "ElementPrototype"
# define DOM_HTML_PROTOTYPE_CLASSNAMES_END() };
#endif // DOM_NO_COMPLEX_GLOBALS

/* Important: this array must be kept in sync with the enumeration
   DOM_Runtime::HTMLElementPrototype.  See its definition for what
   else it needs to be in sync with. */
DOM_HTML_PROTOTYPE_CLASSNAMES_START()
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Unknown")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Html")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Head")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Link")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Title")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Meta")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Base")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("IsIndex")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Style")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Body")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Form")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Select")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("OptGroup")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Option")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Input")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("TextArea")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Button")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Label")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("FieldSet")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Legend")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Output")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("DataList")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Progress")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Meter")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Keygen")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("UList")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("OList")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("DList")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Directory")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Menu")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("LI")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Div")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Paragraph")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Heading")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Quote")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Pre")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("BR")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Font")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("HR")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Mod")
#ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Xml")
#endif // #ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Anchor")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Image")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Object")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Param")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Applet")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Embed")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Map")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Area")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Script")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Table")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("TableCaption")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("TableCol")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("TableSection")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("TableRow")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("TableCell")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("FrameSet")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Frame")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("IFrame")
#ifdef MEDIA_HTML_SUPPORT
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Media")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Audio")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Video")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Source")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Track")
#endif // MEDIA_HTML_SUPPORT
#ifdef CANVAS_SUPPORT
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Canvas")
#endif // CANVAS_SUPPORT
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM("Marquee")
	DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM_LAST("Time")
DOM_HTML_PROTOTYPE_CLASSNAMES_END()

#undef DOM_HTML_PROTOTYPE_CLASSNAMES_START
#undef DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM
#undef DOM_HTML_PROTOTYPE_CLASSNAMES_ITEM_LAST
#undef DOM_HTML_PROTOTYPE_CLASSNAMES_END

/* The enumerators in this enumeration need not be ordered in any
   particular way. */
enum DOM_HTMLElementClass
{
	CLASS_HTMLELEMENT,
	CLASS_HTMLANCHORELEMENT,
	CLASS_HTMLTABLEROWELEMENT,
	CLASS_HTMLTABLECELLELEMENT,
	CLASS_HTMLIMAGEELEMENT,
	CLASS_HTMLMAPELEMENT,
	CLASS_HTMLJSLINKELEMENT,
	CLASS_HTMLFORMELEMENT,
	CLASS_HTMLBODYELEMENT,
	CLASS_HTMLHTMLELEMENT,
	CLASS_HTMLTABLEELEMENT,
	CLASS_HTMLINPUTELEMENT,
	CLASS_HTMLTITLEELEMENT,
	CLASS_HTMLFRAMEELEMENT,
	CLASS_HTMLTABLESECTIONELEMENT,
	CLASS_HTMLOBJECTELEMENT,
#ifdef MEDIA_HTML_SUPPORT
	CLASS_HTMLAUDIOELEMENT,
	CLASS_HTMLVIDEOELEMENT,
	CLASS_HTMLTRACKELEMENT,
#endif // MEDIA_HTML_SUPPORT
#ifdef CANVAS_SUPPORT
	CLASS_HTMLCANVASELEMENT,
#endif
	CLASS_HTMLPLUGINELEMENT,
	CLASS_HTMLSELECTELEMENT,
	CLASS_HTMLOPTGROUPELEMENT,
	CLASS_HTMLOPTIONELEMENT,
	CLASS_HTMLLABELELEMENT,
	CLASS_HTMLOUTPUTELEMENT,
	CLASS_HTMLFIELDSETELEMENT,
	CLASS_HTMLDATALISTELEMENT,
	CLASS_HTMLPROGRESSELEMENT,
	CLASS_HTMLMETERELEMENT,
	CLASS_HTMLKEYGENELEMENT,
	CLASS_HTMLSCRIPTELEMENT,
	CLASS_HTMLBUTTONELEMENT,
	CLASS_HTMLTEXTAREAELEMENT,
	CLASS_HTMLFORMSELEMENT,
	CLASS_HTMLSTYLESHEETELEMENT,
#ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
	CLASS_HTMLXMLELEMENT,
#endif // #ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
	CLASS_HTMLTIMEELEMENT,

	CLASS_LAST
};


/* The elements in this array must be in the same order as the HTE_*
   enumerators are declared in logdoc/src/html5/elementtypes.h.
   Missing elements are okay. */
const ConstructHTMLElementData g_DOM_construct_HTMLElement_data[] =
{
	{ HE_A, DOM_Runtime::ANCHOR_PROTOTYPE, CLASS_HTMLANCHORELEMENT },
	{ HE_APPLET, DOM_Runtime::APPLET_PROTOTYPE, CLASS_HTMLPLUGINELEMENT },
	{ HE_AREA, DOM_Runtime::AREA_PROTOTYPE, CLASS_HTMLJSLINKELEMENT },
#ifdef MEDIA_HTML_SUPPORT
	{ HE_AUDIO, DOM_Runtime::AUDIO_PROTOTYPE, CLASS_HTMLAUDIOELEMENT },
#endif // MEDIA_HTML_SUPPORT
	{ HE_BASE, DOM_Runtime::BASE_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_BLOCKQUOTE, DOM_Runtime::QUOTE_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_BODY, DOM_Runtime::BODY_PROTOTYPE, CLASS_HTMLBODYELEMENT },
	{ HE_BR, DOM_Runtime::BR_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_BUTTON, DOM_Runtime::BUTTON_PROTOTYPE, CLASS_HTMLBUTTONELEMENT },
#ifdef CANVAS_SUPPORT
	{ HE_CANVAS, DOM_Runtime::CANVAS_PROTOTYPE, CLASS_HTMLCANVASELEMENT },
#endif // CANVAS_SUPPORT
	{ HE_CAPTION, DOM_Runtime::TABLECAPTION_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_COL, DOM_Runtime::TABLECOL_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_COLGROUP, DOM_Runtime::TABLECOL_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_DATALIST, DOM_Runtime::DATALIST_PROTOTYPE, CLASS_HTMLDATALISTELEMENT },
	{ HE_DEL, DOM_Runtime::MOD_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_DIR, DOM_Runtime::DIRECTORY_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_DIV, DOM_Runtime::DIV_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_DL, DOM_Runtime::DLIST_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_EMBED, DOM_Runtime::EMBED_PROTOTYPE, CLASS_HTMLPLUGINELEMENT },
	{ HE_FIELDSET, DOM_Runtime::FIELDSET_PROTOTYPE, CLASS_HTMLFIELDSETELEMENT },
	{ HE_FONT, DOM_Runtime::FONT_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_FORM, DOM_Runtime::FORM_PROTOTYPE, CLASS_HTMLFORMELEMENT },
	{ HE_FRAME, DOM_Runtime::FRAME_PROTOTYPE, CLASS_HTMLFRAMEELEMENT },
	{ HE_FRAMESET, DOM_Runtime::FRAMESET_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_H1, DOM_Runtime::HEADING_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_H2, DOM_Runtime::HEADING_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_H3, DOM_Runtime::HEADING_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_H4, DOM_Runtime::HEADING_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_H5, DOM_Runtime::HEADING_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_H6, DOM_Runtime::HEADING_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_HEAD, DOM_Runtime::HEAD_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_HR, DOM_Runtime::HR_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_HTML, DOM_Runtime::HTML_PROTOTYPE, CLASS_HTMLHTMLELEMENT },
	{ HE_IFRAME, DOM_Runtime::IFRAME_PROTOTYPE, CLASS_HTMLFRAMEELEMENT },
	{ HE_IMG, DOM_Runtime::IMAGE_PROTOTYPE, CLASS_HTMLIMAGEELEMENT },
	{ HE_INPUT, DOM_Runtime::INPUT_PROTOTYPE, CLASS_HTMLINPUTELEMENT },
	{ HE_INS, DOM_Runtime::MOD_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_ISINDEX, DOM_Runtime::ISINDEX_PROTOTYPE, CLASS_HTMLFORMSELEMENT },
	{ HE_KEYGEN, DOM_Runtime::KEYGEN_PROTOTYPE, CLASS_HTMLKEYGENELEMENT },
	{ HE_LABEL, DOM_Runtime::LABEL_PROTOTYPE, CLASS_HTMLLABELELEMENT },
	{ HE_LEGEND, DOM_Runtime::LEGEND_PROTOTYPE, CLASS_HTMLFORMSELEMENT },
	{ HE_LI, DOM_Runtime::LI_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_LINK, DOM_Runtime::LINK_PROTOTYPE, CLASS_HTMLSTYLESHEETELEMENT },
	{ HE_MAP, DOM_Runtime::MAP_PROTOTYPE, CLASS_HTMLMAPELEMENT },
	{ HE_MARQUEE, DOM_Runtime::MARQUEE_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_MENU, DOM_Runtime::MENU_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_META, DOM_Runtime::META_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_METER, DOM_Runtime::METER_PROTOTYPE, CLASS_HTMLMETERELEMENT },
	{ HE_OBJECT, DOM_Runtime::OBJECT_PROTOTYPE, CLASS_HTMLOBJECTELEMENT },
	{ HE_OL, DOM_Runtime::OLIST_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_OPTGROUP, DOM_Runtime::OPTGROUP_PROTOTYPE, CLASS_HTMLOPTGROUPELEMENT },
	{ HE_OPTION, DOM_Runtime::OPTION_PROTOTYPE, CLASS_HTMLOPTIONELEMENT },
	{ HE_OUTPUT, DOM_Runtime::OUTPUT_PROTOTYPE, CLASS_HTMLOUTPUTELEMENT },
	{ HE_P, DOM_Runtime::PARAGRAPH_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_PARAM, DOM_Runtime::PARAM_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_PRE, DOM_Runtime::PRE_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_PROGRESS, DOM_Runtime::PROGRESS_PROTOTYPE, CLASS_HTMLPROGRESSELEMENT },
	{ HE_Q, DOM_Runtime::QUOTE_PROTOTYPE, CLASS_HTMLELEMENT },
	{ HE_SCRIPT, DOM_Runtime::SCRIPT_PROTOTYPE, CLASS_HTMLSCRIPTELEMENT },
	{ HE_SELECT, DOM_Runtime::SELECT_PROTOTYPE, CLASS_HTMLSELECTELEMENT },
#ifdef MEDIA_HTML_SUPPORT
	{ HE_SOURCE, DOM_Runtime::SOURCE_PROTOTYPE, CLASS_HTMLELEMENT },
#endif // MEDIA_HTML_SUPPORT
	{ HE_STYLE, DOM_Runtime::STYLE_PROTOTYPE, CLASS_HTMLSTYLESHEETELEMENT },
	{ HE_TABLE, DOM_Runtime::TABLE_PROTOTYPE, CLASS_HTMLTABLEELEMENT },
	{ HE_TBODY, DOM_Runtime::TABLESECTION_PROTOTYPE, CLASS_HTMLTABLESECTIONELEMENT },
	{ HE_TD, DOM_Runtime::TABLECELL_PROTOTYPE, CLASS_HTMLTABLECELLELEMENT },
	{ HE_TEXTAREA, DOM_Runtime::TEXTAREA_PROTOTYPE, CLASS_HTMLTEXTAREAELEMENT },
	{ HE_TFOOT, DOM_Runtime::TABLESECTION_PROTOTYPE, CLASS_HTMLTABLESECTIONELEMENT },
	{ HE_TH, DOM_Runtime::TABLECELL_PROTOTYPE, CLASS_HTMLTABLECELLELEMENT },
	{ HE_THEAD, DOM_Runtime::TABLESECTION_PROTOTYPE, CLASS_HTMLTABLESECTIONELEMENT },
	{ HE_TIME, DOM_Runtime::TIME_PROTOTYPE, CLASS_HTMLTIMEELEMENT },
	{ HE_TITLE, DOM_Runtime::TITLE_PROTOTYPE, CLASS_HTMLTITLEELEMENT },
	{ HE_TR, DOM_Runtime::TABLEROW_PROTOTYPE, CLASS_HTMLTABLEROWELEMENT },
#ifdef MEDIA_HTML_SUPPORT
	{ Markup::HTE_TRACK, DOM_Runtime::TRACK_PROTOTYPE, CLASS_HTMLTRACKELEMENT },
#endif // MEDIA_HTML_SUPPORT
	{ HE_UL, DOM_Runtime::ULIST_PROTOTYPE, CLASS_HTMLELEMENT },
#ifdef MEDIA_HTML_SUPPORT
	{ HE_VIDEO, DOM_Runtime::VIDEO_PROTOTYPE, CLASS_HTMLVIDEOELEMENT },
#endif // MEDIA_HTML_SUPPORT
#ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
	{ HE_XML, DOM_Runtime::XML_PROTOTYPE, CLASS_HTMLXMLELEMENT },
#endif // #ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
	{ HE_UNKNOWN, DOM_Runtime::UNKNOWN_ELEMENT_PROTOTYPE, CLASS_HTMLELEMENT },
	{ 0, 0, 0 }
};

/* static */ OP_STATUS
DOM_HTMLElement::Make(DOM_HTMLElement *&element, HTML_Element *html_element, DOM_EnvironmentImpl *environment)
{
	element = NULL;

	DOM_Runtime::HTMLElementPrototype htmlelement_prototype_type = DOM_Runtime::HTMLELEMENT_PROTOTYPES_COUNT;
	HTML_ElementType element_type = html_element->Type();
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	BOOL has_volatile_properties = FALSE;

	if ((unsigned) element_type >= g_DOM_construct_HTMLElement_data[0].element_type)
	{
		unsigned hi = sizeof g_DOM_construct_HTMLElement_data / sizeof g_DOM_construct_HTMLElement_data[0] - 2;
		unsigned lo = 0;
		unsigned index = 0;

		while (hi >= lo)
		{
			index = (hi + lo) / 2;
			if ((HTML_ElementType) g_DOM_construct_HTMLElement_data[index].element_type == element_type)
				break;
			if ((HTML_ElementType) g_DOM_construct_HTMLElement_data[index].element_type < element_type)
				lo = ++index;
			else
				hi = index - 1;
		}

		if ((HTML_ElementType) g_DOM_construct_HTMLElement_data[index].element_type == element_type)
		{
			htmlelement_prototype_type = (DOM_Runtime::HTMLElementPrototype) g_DOM_construct_HTMLElement_data[index].prototype;

			ES_Object *prototype = runtime->GetHTMLElementPrototype(htmlelement_prototype_type);
			RETURN_OOM_IF_NULL(prototype);

			const char *classname = g_DOM_htmlClassNames[htmlelement_prototype_type];

			switch (g_DOM_construct_HTMLElement_data[index].element_class)
			{
			case CLASS_HTMLELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLANCHORELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLAnchorElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLTABLEROWELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLTableRowElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLTABLECELLELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLTableCellElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLIMAGEELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLImageElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLMAPELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLMapElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLJSLINKELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLJSLinkElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLFORMELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLFormElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLBODYELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLBodyElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLHTMLELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLHtmlElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLTABLEELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLTableElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLINPUTELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLInputElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLTITLEELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLTitleElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLFRAMEELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLFrameElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLTABLESECTIONELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLTableSectionElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLOBJECTELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLObjectElement, (), prototype, classname, runtime);
				has_volatile_properties = TRUE;
				break;

#ifdef CANVAS_SUPPORT
			case CLASS_HTMLCANVASELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLCanvasElement, (), prototype, classname, runtime);
				break;
#endif // CANVAS_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
			case CLASS_HTMLAUDIOELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLAudioElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLVIDEOELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLVideoElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLTRACKELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLTrackElement, (), prototype, classname, runtime);
				break;
#endif // MEDIA_HTML_SUPPORT

			case CLASS_HTMLPLUGINELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLPluginElement, (), prototype, classname, runtime);
				has_volatile_properties = TRUE;
				break;

			case CLASS_HTMLSELECTELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLSelectElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLOPTGROUPELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLOptGroupElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLOPTIONELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLOptionElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLLABELELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLLabelElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLOUTPUTELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLOutputElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLFIELDSETELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLFieldsetElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLDATALISTELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLDataListElement, (), prototype, classname, runtime);
				break;
			case CLASS_HTMLKEYGENELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLKeygenElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLPROGRESSELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLProgressElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLMETERELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLMeterElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLSCRIPTELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLScriptElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLBUTTONELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLButtonElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLTEXTAREAELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLTextAreaElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLFORMSELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLFormsElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLSTYLESHEETELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLStylesheetElement, (), prototype, classname, runtime);
				break;

			case CLASS_HTMLTIMEELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLTimeElement, (), prototype, classname, runtime);
				break;

#ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
			case CLASS_HTMLXMLELEMENT:
				DOM_ALLOCATE(element, DOM_HTMLXmlElement, (), prototype, classname, runtime);
				break;
#endif // #ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
			}
		}
	}

	if (!element)
		DOM_ALLOCATE(element, DOM_HTMLElement, (), runtime->GetPrototype(DOM_Runtime::HTMLELEMENT_PROTOTYPE), "HTMLElement", runtime);

	if (element)
	{
		if (htmlelement_prototype_type != DOM_Runtime::HTMLELEMENT_PROTOTYPES_COUNT)
			element->SetProperties(&g_DOM_htmlProperties[htmlelement_prototype_type]);

		if (has_volatile_properties)
			element->SetHasVolatilePropertySet();

#ifdef MEDIA_HTML_SUPPORT
		if (html_element->GetNsType() == NS_HTML)
		{
			switch (element_type)
			{
			case HE_AUDIO:
			case HE_VIDEO:
				environment->AddMediaElement(static_cast<DOM_HTMLMediaElement*>(element));
				break;
			}
		}
#endif // MEDIA_HTML_SUPPORT
	}

	return OpStatus::OK;
}

/* static */ void
DOM_HTMLElement::InitializeConstructorsTableL(OpString8HashTable<DOM_ConstructorInformation> *table)
{
#define ADD_CONSTRUCTOR(name, id) table->AddL(name, OP_NEW_L(DOM_ConstructorInformation, (name, DOM_Runtime::CTN_HTMLELEMENT, id)))

	for (unsigned index = 0; index != DOM_Runtime::HTMLELEMENT_PROTOTYPES_COUNT; ++index)
		ADD_CONSTRUCTOR(g_DOM_htmlClassNames[index], index);
#undef ADD_CONSTRUCTOR
}

/* static */ ES_Object *
DOM_HTMLElement::CreateConstructorL(DOM_Runtime *runtime, DOM_Object *target, unsigned id)
{
	DOM_Object *object = target->PutConstructorL(g_DOM_htmlClassNames[id], (DOM_Runtime::HTMLElementPrototype) id);
	runtime->RecordHTMLElementConstructor((DOM_Runtime::HTMLElementPrototype) id, object);
#ifdef MEDIA_HTML_SUPPORT
	if (id == DOM_Runtime::MEDIA_PROTOTYPE)
		DOM_HTMLMediaElement::ConstructHTMLMediaElementObjectL(*object, runtime);
	else if (id == DOM_Runtime::TRACK_PROTOTYPE)
		DOM_HTMLTrackElement::ConstructHTMLTrackElementObjectL(*object, runtime);
#endif // MEDIA_HTML_SUPPORT
	return *object;
}

/* virtual */ ES_GetState
DOM_HTMLElement::GetName(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	return DOM_Element::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_HTMLElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_unselectable:
	case OP_ATOM_className:
	case OP_ATOM_dir:
	case OP_ATOM_id:
	case OP_ATOM_lang:
	case OP_ATOM_title:
	case OP_ATOM_accessKey:
	case OP_ATOM_itemType:
	case OP_ATOM_itemId:
		return GetStringAttribute(property_name, value);

	case OP_ATOM_hidden:
	case OP_ATOM_itemScope:
		return GetBooleanAttribute(property_name, value);

	case OP_ATOM_itemValue:
		if (!this_element->HasAttr(ATTR_ITEMPROP))
		{
			DOMSetNull(value);
			return GET_SUCCESS;
		}
		else if (this_element->GetBoolAttr(ATTR_ITEMSCOPE))
		{
			DOMSetObject(value, this);
			return GET_SUCCESS;
		}
		else
			return GetName(GetItemValueProperty(this_element), value, origining_runtime);

	case OP_ATOM_properties:
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_propertiesMicrodata);
			if (result != GET_FAILED)
				return result;

			// "properties [...] must return an HTMLPropertiesCollection rooted at the Document node".
			DOM_HTMLPropertiesCollection *collection;
			GET_FAILED_IF_ERROR(DOM_HTMLPropertiesCollection::Make(collection, GetEnvironment(), this));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_propertiesMicrodata, collection));
			DOMSetObject(value, collection);
		}
		return GET_SUCCESS;

#ifdef CURRENT_STYLE_SUPPORT
	case OP_ATOM_currentStyle:
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_currentStyle);
			if (result != GET_FAILED)
				return result;

			DOM_CSSStyleDeclaration *style;

			GET_FAILED_IF_ERROR(DOM_CSSStyleDeclaration::Make(style, this, DOM_CSSStyleDeclaration::DOM_ST_CURRENT));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_currentStyle, style));

			DOMSetObject(value, style);
        }
		return GET_SUCCESS;
#endif // CURRENT_STYLE_SUPPORT

    case OP_ATOM_style:
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_style);
			if (result != GET_FAILED)
				return result;

			DOM_CSSStyleDeclaration *style;

			GET_FAILED_IF_ERROR(DOM_CSSStyleDeclaration::Make(style, this, DOM_CSSStyleDeclaration::DOM_ST_INLINE));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_style, style));

			DOMSetObject(value, style);
        }
		return GET_SUCCESS;

    case OP_ATOM_innerText:
    case OP_ATOM_innerHTML:
    case OP_ATOM_outerText:
    case OP_ATOM_outerHTML:
        if (value)
		{
            TempBuffer *buffer = GetEmptyTempBuf();

			TempBuffer::ExpansionPolicy epolicy = buffer->SetExpansionPolicy(TempBuffer::AGGRESSIVE);
			TempBuffer::CachedLengthPolicy lpolicy = buffer->SetCachedLengthPolicy(TempBuffer::TRUSTED);

			BOOL text_only = property_name == OP_ATOM_innerText || property_name == OP_ATOM_outerText;
			BOOL include_this = property_name == OP_ATOM_outerHTML || property_name == OP_ATOM_outerText;

			BOOL xml_output = this_element->GetNsIdx() != NS_IDX_HTML;
			OP_STATUS status;
			BOOL exception = FALSE;
#ifdef XMLUTILS_XMLSERIALIZER_SUPPORT
			if (xml_output && !text_only)
			{
				XMLSerializer *serializer;
				URL url;
				GET_FAILED_IF_ERROR(XMLSerializer::MakeToStringSerializer(serializer, buffer));
				if (include_this)
					status = serializer->Serialize(NULL, url, this_element, this_element, NULL);
				else
				{
					HTML_Element *elm = this_element->FirstChildActual();
					status = OpStatus::OK;
					while (elm && OpStatus::IsSuccess(status))
					{
						status = serializer->Serialize(NULL, url, elm, elm, NULL);
						elm = elm->SucActual();
					}
				}

				if (OpStatus::IsError(status) && !OpStatus::IsMemoryError(status))
				{
					if (serializer->GetError() != XMLSerializer::ERROR_NONE)
						exception = TRUE;
				}

				OP_DELETE(serializer);
			}
			else
#endif // XMLUTILS_XMLSERIALIZER_SUPPORT
				status = this_element->AppendEntireContentAsString(buffer, text_only, include_this, xml_output);

			buffer->SetExpansionPolicy(epolicy);
			buffer->SetCachedLengthPolicy(lpolicy);

			if (exception)
				return DOM_GETNAME_DOMEXCEPTION(SYNTAX_ERR);

			GET_FAILED_IF_ERROR(status);

			DOMSetString(value, buffer);
        }
        return GET_SUCCESS;

#ifdef DOCUMENT_EDIT_SUPPORT
	case OP_ATOM_isContentEditable:
		if (value)
		{
			DOMSetBoolean(value, this_element->IsContentEditable(TRUE));
		}
		return GET_SUCCESS;

	case OP_ATOM_contentEditable:
		if (value)
		{
			const uni_char* val = this_element->DOMGetAttribute(GetEnvironment(), ATTR_CONTENTEDITABLE, NULL, NS_IDX_DEFAULT, FALSE, FALSE);
			const uni_char* result = UNI_L("inherit");
			if (val)
			{
				if (uni_stricmp(val, "true") == 0 || !*val)
					result = UNI_L("true");
				else if (uni_stricmp(val, "false") == 0)
					result = UNI_L("false");
			}

			DOMSetString(value, result);
		}
		return GET_SUCCESS;
#endif

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	case OP_ATOM_spellcheck:
		if (value)
		{
			BOOL result = this_element->SpellcheckEnabledByAttr() > 0;

			DOMSetBoolean(value, result);
		}
		return GET_SUCCESS;
#endif
	case OP_ATOM_parentElement:
		if (value)
		{
			DOM_Node *parent;
			GET_FAILED_IF_ERROR(GetParentNode(parent));
			if (parent && parent->GetNodeType() != ELEMENT_NODE)
				parent = NULL;
			DOMSetObject(value, parent);
		}
		return GET_SUCCESS;

	case OP_ATOM_dataset:
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_dataset);
			if (result != GET_FAILED)
				return result;

			DOM_DOMStringMap *dataset;
			GET_FAILED_IF_ERROR(DOM_DOMStringMap::Make(dataset, this));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_dataset, dataset));
			DOMSetObject(value, dataset);
		}
		return GET_SUCCESS;

	case OP_ATOM_tabIndex:
		// https://homes.oslo.opera.com/annevk/specs/action-based-navigation/
		// The tabIndex attribute must be the value of the tabindex
		// content attribute on getting. If the attribute is not set
		// (or has an invalid value) then the DOM attribute must be
		// the default value for that attribute. 0 for elements that
		// are part of the tab order and -1 for elements that are not.
		if (value)
		{
			DOMSetNumber(value, this_element->GetTabIndex(GetFramesDocument()));
		}
		return GET_SUCCESS;

	case OP_ATOM_classList:
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_classList);
			if (result != GET_FAILED)
				return result;

			DOM_DOMTokenList *classlist_token;
			GET_FAILED_IF_ERROR(DOM_DOMTokenList::Make(classlist_token, this));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_classList, classlist_token));
			DOMSetObject(value, classlist_token);
		}
		return GET_SUCCESS;
	case OP_ATOM_itemRef:
	case OP_ATOM_itemProp:
		if (value)
		{
			DOM_PrivateName private_prop = (property_name == OP_ATOM_itemProp) ? DOM_PRIVATE_itemProp : DOM_PRIVATE_itemRef;
			HTML_AttrType attr_type      = (property_name == OP_ATOM_itemProp) ? ATTR_ITEMPROP : ATTR_ITEMREF;
			ES_GetState result = DOMSetPrivate(value, private_prop);
			if (result != GET_FAILED)
				return result;

			DOM_DOMSettableTokenList *itemString_token;
			GET_FAILED_IF_ERROR(DOM_DOMSettableTokenList::Make(itemString_token, this, attr_type));
			GET_FAILED_IF_ERROR(PutPrivate(private_prop, itemString_token));
			DOMSetObject(value, itemString_token);
		}
		return GET_SUCCESS;

#ifdef DRAG_SUPPORT
	case OP_ATOM_dropzone:
		DOMSetString(value, this_element->GetDropzone());
		return GET_SUCCESS;

	case OP_ATOM_draggable:
		DOMSetBoolean(value, this_element->IsDraggable(GetFramesDocument()));
		return GET_SUCCESS;
#endif // DRAG_SUPPORT

	default:
		if (properties)
		{
			// Look up in the property->array lists. They are sorted so we skip past
			// the values with lower numbers. The sorting saves us half a list search
			// for no added complexity.
			unsigned index;

			if (properties->string)
			{
				for (index = 0; HTML_STRING_PROPERTY_ATOM(properties->string[index]) < property_name; ++index)
				{
					// Loop past all properties with lower numbers
				}
				if (HTML_STRING_PROPERTY_ATOM(properties->string[index]) == property_name)
					return GetStringAttribute(property_name, value);
			}

			if (properties->numeric)
			{
				for (index = 0; HTML_NUMERIC_PROPERTY_ATOM(properties->numeric[index]) < property_name; ++index)
				{
					// Loop past all properties with lower numbers
					HTML_NUMERIC_PROPERTY_SKIP_DEFAULT(index, properties->numeric[index]);
				}
				if (HTML_NUMERIC_PROPERTY_ATOM(properties->numeric[index]) == property_name)
					return GetNumericAttribute(property_name, &properties->numeric[index], value);
			}

			if (properties->boolean)
			{
				for (index = 0; properties->boolean[index] < property_name; ++index)
				{
					// Loop past all properties with lower numbers
				}
				if (properties->boolean[index] == property_name)
					return GetBooleanAttribute(property_name, value);
			}
		}

		return DOM_Element::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLElement::PutName(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	return DOM_Element::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_unselectable:
	case OP_ATOM_className:
	case OP_ATOM_dir:
	case OP_ATOM_id:
	case OP_ATOM_lang:
	case OP_ATOM_style:
	case OP_ATOM_title:
	case OP_ATOM_itemType:
	case OP_ATOM_itemId:
	case OP_ATOM_accessKey:
		return SetStringAttribute(property_name, FALSE, value, origining_runtime);

	case OP_ATOM_hidden:
	case OP_ATOM_itemScope:
		return SetBooleanAttribute(property_name, value, origining_runtime);

	case OP_ATOM_itemValue:
		if (!this_element->HasAttr(ATTR_ITEMPROP) || this_element->GetBoolAttr(ATTR_ITEMSCOPE))
			return DOM_PUTNAME_DOMEXCEPTION(INVALID_ACCESS_ERR);
		else
			return PutName(GetItemValueProperty(this_element), value, origining_runtime);


	case OP_ATOM_dataset:
	case OP_ATOM_properties:
		return PUT_SUCCESS;

#ifdef DOCUMENT_EDIT_SUPPORT
	case OP_ATOM_contentEditable:
		{
			if (value->type != VALUE_STRING)
				return PUT_NEEDS_STRING;

			const uni_char* attr_val = NULL;

			if (uni_stricmp(value->value.string, "true") == 0)
				attr_val = UNI_L("true");
			else if (uni_stricmp(value->value.string, "false") == 0)
				attr_val = UNI_L("false");
			else if (uni_stricmp(value->value.string, "inherit") != 0)
				return DOM_PUTNAME_DOMEXCEPTION(SYNTAX_ERR);

			if (attr_val)
				PUT_FAILED_IF_ERROR(this_element->DOMSetAttribute(GetEnvironment(), ATTR_CONTENTEDITABLE, NULL, NS_IDX_DEFAULT, attr_val, uni_strlen(attr_val), FALSE));
			else
				// FIXME: Add a DOMRemoveAttribute that accepts ATTR_CONTENTEDITABLE rather than a string
				PUT_FAILED_IF_ERROR(this_element->DOMRemoveAttribute(GetEnvironment(), UNI_L("contenteditable"), NS_IDX_HTML, TRUE));

			return PUT_SUCCESS;
		}
#endif // DOCUMENT_EDIT_SUPPORT

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	case OP_ATOM_spellcheck:
	{
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;

		const uni_char* attr_val = value->value.boolean ? UNI_L("true") : UNI_L("false");

		PUT_FAILED_IF_ERROR(this_element->DOMSetAttribute(GetEnvironment(), ATTR_SPELLCHECK, NULL, NS_IDX_DEFAULT, attr_val, uni_strlen(attr_val), FALSE));

		return PUT_SUCCESS;
	}
#endif // INTERNAL_SPELLCHECK_SUPPORT
	case OP_ATOM_tabIndex:
		return SetNumericAttribute(OP_ATOM_tabIndex, DOM_HTMLElement::GetTabIndexTypeDescriptor(), value, origining_runtime);

	case OP_ATOM_innerHTML:
	case OP_ATOM_innerText:
	case OP_ATOM_outerHTML:
	case OP_ATOM_outerText:
		return ParseAndReplace(property_name, value, (DOM_Runtime *) origining_runtime);

	case OP_ATOM_classList:
		return PUT_SUCCESS;

	case OP_ATOM_itemRef:
	case OP_ATOM_itemProp:
		{
			// [PutForwards=value] readonly attribute DOMSettableTokenList itemRef;
			// [PutForwards=value] readonly attribute DOMSettableTokenList itemProp;
			if (value->type != VALUE_STRING)
				return PUT_NEEDS_STRING;

			ES_Value propStringToken_value;
			ES_GetState result = GetName(property_name, &propStringToken_value, origining_runtime);
			if (result != GET_SUCCESS)
			{
				OP_ASSERT(result == GET_NO_MEMORY || !"New return type added to GetName(OP_ATOM_itemRef or itemProp)?");
				return PUT_NO_MEMORY;
			}
			OP_ASSERT(propStringToken_value.type == VALUE_OBJECT);
			DOM_DOMSettableTokenList* dom_stl = DOM_HOSTOBJECT(propStringToken_value.value.object, DOM_DOMSettableTokenList);
			return dom_stl->PutName(OP_ATOM_value, value, origining_runtime);
		}
#ifdef DRAG_SUPPORT
	case OP_ATOM_dropzone:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		return SetStringAttribute(property_name, FALSE, value, origining_runtime);

	case OP_ATOM_draggable:
	{
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;
		ES_Value draggable_attr;
		DOMSetString(&draggable_attr, value->value.boolean ? UNI_L("true") : UNI_L("false"));
		return SetStringAttribute(property_name, FALSE, &draggable_attr, origining_runtime);
	}
#endif // DRAG_SUPPORT

	default:
		if (properties)
		{
			unsigned index;

			if (properties->string)
				for (index = 0; HTML_STRING_PROPERTY_ATOM(properties->string[index]) <= property_name; ++index)
					if (HTML_STRING_PROPERTY_ATOM(properties->string[index]) == property_name)
						return SetStringAttribute(property_name, HTML_STRING_PROPERTY_ATOM_NULL_AS_EMPTY(properties->string[index]), value, origining_runtime);

			if (properties->numeric)
				for (index = 0; HTML_NUMERIC_PROPERTY_ATOM(properties->numeric[index]) <= property_name; ++index)
				{
					if (HTML_NUMERIC_PROPERTY_ATOM(properties->numeric[index]) == property_name)
						return SetNumericAttribute(property_name, &properties->numeric[index], value, origining_runtime);
					HTML_NUMERIC_PROPERTY_SKIP_DEFAULT(index, properties->numeric[index]);
				}

			if (properties->boolean)
				for (index = 0; properties->boolean[index] <= property_name; ++index)
					if (properties->boolean[index] == property_name)
						return SetBooleanAttribute(property_name, value, origining_runtime);
		}
	}

	return DOM_Element::PutName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLElement::PutNameRestart(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	return DOM_Node::PutNameRestart(property_name, property_code, value, origining_runtime, restart_object);
}

/* virtual */ ES_PutState
DOM_HTMLElement::PutNameRestart(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object)
{
	switch (property_name)
	{
    case OP_ATOM_innerHTML:
    case OP_ATOM_innerText:
    case OP_ATOM_outerHTML:
    case OP_ATOM_outerText:
        return ParseAndReplace(property_name, value, (DOM_Runtime *) origining_runtime, NOWHERE, DOM_HOSTOBJECT(restart_object, DOM_Object));

	case OP_ATOM_text:
		return SetTextContent(value, (DOM_Runtime *) origining_runtime, restart_object);

	case OP_ATOM_itemValue:
		return PutNameRestart(GetItemValueProperty(this_element), value, origining_runtime, restart_object);
	}

	return DOM_Node::PutNameRestart(property_name, value, origining_runtime, restart_object);
}

/* virtual */ ES_GetState
DOM_HTMLElement::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_Element::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;
	DOM_Event::FetchNamedHTMLElmEventPropertiesL(enumerator, this_element);
	return GET_SUCCESS;
}

/* virtual */ void
DOM_HTMLElement::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_Element::DOMChangeRuntime(new_runtime);

	/* These only affect different more specific classes, but we might as well
	   handle them all here.  The operation is not expensive when they are not
	   set, and this function is rarely called anyhow. */
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_labels_collection);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_forms);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_elements);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_templateElements);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_images);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_audioRecording);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_plugin_scriptable);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_options);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_areas);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_rows);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_cells);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_tbodies);
}

/* static */ void
DOM_HTMLElement_Prototype::ConstructL(ES_Object *prototype, DOM_Runtime::HTMLElementPrototype type, DOM_Runtime *runtime)
{
	switch (type)
	{
	case DOM_Runtime::FORM_PROTOTYPE:
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormElement::reset, "reset", NULL);
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormElement::submit, "submit", NULL);
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormElement::item, "item", "n-");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormElement::namedItem, "namedItem", "s-");

		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormElement::checkValidity, "checkValidity", NULL);
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormElement::resetFromData, "resetFromData", NULL);
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormElement::dispatchFormInputOrFormChange, DOM_HTMLFormElement::DISPATCH_FORM_INPUT, "dispatchFormInput", NULL);
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormElement::dispatchFormInputOrFormChange, DOM_HTMLFormElement::DISPATCH_FORM_CHANGE, "dispatchFormChange", NULL);
		break;

	case DOM_Runtime::INPUT_PROTOTYPE:
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLInputElement::stepUpDown, DOM_HTMLInputElement::STEP_UP, "stepUp", "n-");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLInputElement::stepUpDown, DOM_HTMLInputElement::STEP_DOWN, "stepDown", "n-");
		// Falltrough
	case DOM_Runtime::SELECT_PROTOTYPE:
	case DOM_Runtime::TEXTAREA_PROTOTYPE:
	case DOM_Runtime::ANCHOR_PROTOTYPE:
	case DOM_Runtime::BUTTON_PROTOTYPE:
	case DOM_Runtime::KEYGEN_PROTOTYPE:
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLElement::focus, "focus", NULL);
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLElement::blur, "blur", NULL);

		if (type == DOM_Runtime::SELECT_PROTOTYPE)
		{
			DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLSelectElement::addOrRemove, 0, "add", NULL);
			DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLSelectElement::addOrRemove, 1, "remove", "n-");
			DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLSelectElement::item, "item", "n-");
			DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLSelectElement::namedItem, "namedItem", "s-");
		}

		// Everything but "anchor"
		if (type == DOM_Runtime::INPUT_PROTOTYPE || type == DOM_Runtime::TEXTAREA_PROTOTYPE ||
			type == DOM_Runtime::SELECT_PROTOTYPE || type == DOM_Runtime::BUTTON_PROTOTYPE ||
			type == DOM_Runtime::KEYGEN_PROTOTYPE)
		{
			DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormsElement::setCustomValidity, "setCustomValidity", "s-");
			DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormsElement::checkValidity, "checkValidity", NULL);
		}

		if (type == DOM_Runtime::INPUT_PROTOTYPE || type == DOM_Runtime::TEXTAREA_PROTOTYPE)
		{
#ifdef DOM_SELECTION_SUPPORT
			DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormsElement::select, "select", NULL);
			DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormsElement::setSelectionRange, "setSelectionRange", "nns-");
#else
			DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLElement::focus, "select", NULL); // At least trigger the one side effect of select() we can trigger
#endif // DOM_SELECTION_SUPPORT
		}

		break;

	case DOM_Runtime::OUTPUT_PROTOTYPE:
	case DOM_Runtime::FIELDSET_PROTOTYPE:
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormsElement::setCustomValidity, "setCustomValidity", "s-");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormsElement::checkValidity, "checkValidity", NULL);
		break;

	case DOM_Runtime::TABLE_PROTOTYPE:
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLTableElement::createTableItem, 0, "createCaption");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLTableElement::createTableItem, 1, "createTHead");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLTableElement::createTableItem, 2, "createTFoot");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLTableElement::deleteTableItem, 0, "deleteCaption");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLTableElement::deleteTableItem, 1, "deleteTHead");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLTableElement::deleteTableItem, 2, "deleteTFoot");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLTableElement::insertRow, "insertRow", "n-");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLTableElement::deleteRow, "deleteRow", "n-");
		break;

	case DOM_Runtime::TABLESECTION_PROTOTYPE:
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLTableSectionElement::insertRow, "insertRow", "n-");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLTableSectionElement::deleteRow, "deleteRow", "n-");
		break;

	case DOM_Runtime::TABLEROW_PROTOTYPE:
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLTableRowElement::insertCell, "insertCell", "n-");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLTableRowElement::deleteCell, "deleteCell", "n-");
		break;

	case DOM_Runtime::OBJECT_PROTOTYPE:
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormsElement::setCustomValidity, "setCustomValidity", "s-");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLFormsElement::checkValidity, "checkValidity", NULL);
#ifdef SVG_DOM
		// Fall through.
	case DOM_Runtime::EMBED_PROTOTYPE:
	case DOM_Runtime::IFRAME_PROTOTYPE:
	case DOM_Runtime::FRAME_PROTOTYPE:
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLPluginElement::getSVGDocument, "getSVGDocument", "");
#endif // SVG_DOM
		break;

#ifdef MEDIA_HTML_SUPPORT
	case DOM_Runtime::MEDIA_PROTOTYPE:
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLMediaElement::addTextTrack, "addTextTrack", "sss-");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLMediaElement::canPlayType, "canPlayType", "s");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLMediaElement::load_play_pause, 0, "load");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLMediaElement::load_play_pause, 1, "play");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLMediaElement::load_play_pause, 2, "pause");
		break;
#endif // MEDIA_HTML_SUPPORT

#ifdef CANVAS_SUPPORT
	case DOM_Runtime::CANVAS_PROTOTYPE:
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLCanvasElement::getContext, "getContext", "s-");
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLCanvasElement::toDataURL, "toDataURL", "s-");
		break;
#endif
	case DOM_Runtime::MARQUEE_PROTOTYPE:
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLMarqueeElement::startOrStop, 0, "start", NULL);
		DOM_Object::AddFunctionL(prototype, runtime, DOM_HTMLMarqueeElement::startOrStop, 1, "stop", NULL);
		break;
	}
}

/* static */ OP_STATUS
DOM_HTMLElement_Prototype::Construct(ES_Object *prototype, DOM_Runtime::HTMLElementPrototype type, DOM_Runtime *runtime)
{
	TRAPD(status, ConstructL(prototype, type, runtime));
	return status;
}

/* virtual */ int
DOM_HTMLOptionElement_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();
	DOM_HTMLElement *option;

	CALL_FAILED_IF_ERROR(environment->ConstructDocumentNode());
	CALL_FAILED_IF_ERROR(DOM_HTMLElement::CreateElement(option, static_cast<DOM_Document *>(environment->GetDocument()), UNI_L("option")));

	if (argc > 0 && argv[0].type == VALUE_STRING)
	{
		DOM_Text *text;

		CALL_FAILED_IF_ERROR(DOM_Text::Make(text, option, argv[0].value.string));
		CALL_FAILED_IF_ERROR(option->InsertChild(text, NULL, static_cast<DOM_Runtime *>(origining_runtime)));
	}

	HTML_Element *option_he = option->GetThisElement();

	if (argc > 1 && argv[1].type == VALUE_STRING)
		CALL_FAILED_IF_ERROR(option_he->DOMSetAttribute(environment, ATTR_VALUE, NULL, NS_IDX_DEFAULT, argv[1].value.string, argv[1].GetStringLength(), FALSE));

	BOOL selected = argc > 3 && argv[3].type == VALUE_BOOLEAN && argv[3].value.boolean;
	BOOL default_selected = argc > 2 && argv[2].type == VALUE_BOOLEAN && argv[2].value.boolean;

	/* If the defaultSelected argument is present and true, the new object
	   must have a selected attribute set with no value. */
	if (default_selected)
		CALL_FAILED_IF_ERROR(option_he->DOMSetBooleanAttribute(environment, ATTR_SELECTED, NULL, NS_IDX_DEFAULT, TRUE));

	if (default_selected != selected)
		/* If the selected argument is present and true, the new object must
		   have its selectedness set to true; otherwise the fourth argument
		   is absent or false, the selectedness must be set to false, even
		   if the defaultSelected argument is present and true. */

		FormValueList::DOMSetFreeOptionSelected(option_he, selected);

	DOMSetObject(return_value, option);
	return ES_VALUE;
}

/* virtual */ int
DOM_HTMLImageElement_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();
	DOM_HTMLElement *image;

	CALL_FAILED_IF_ERROR(environment->ConstructDocumentNode());

	CALL_FAILED_IF_ERROR(DOM_HTMLElement::CreateElement(image, static_cast<DOM_Node *>(environment->GetDocument()), UNI_L("img")));

	HTML_Element *element = image->GetThisElement();

	if (argc > 0 && argv[0].type == VALUE_NUMBER)
	{
		unsigned width = TruncateDoubleToUInt(argv[0].value.number);
		int iwidth = width > DOM_HTMLElement::NUMERIC_ATTRIBUTE_MAX ? 0 : static_cast<int>(width);
		CALL_FAILED_IF_ERROR(element->DOMSetNumericAttribute(environment, ATTR_WIDTH, NULL, NS_IDX_DEFAULT, iwidth));
	}

	if (argc > 1 && argv[1].type == VALUE_NUMBER)
	{
		unsigned height = TruncateDoubleToUInt(argv[1].value.number);
		int iheight = height > DOM_HTMLElement::NUMERIC_ATTRIBUTE_MAX ? 0 : static_cast<int>(height);
		CALL_FAILED_IF_ERROR(element->DOMSetNumericAttribute(environment, ATTR_HEIGHT, NULL, NS_IDX_DEFAULT, iheight));
	}

	DOMSetObject(return_value, image);
	return ES_VALUE;
}

/**
 * The HTMLElement.prototype.toString() method will be used on HTMLElements, but
 * also on the prototype object itself and since that isn't even an host object
 * we have to make a special low level function object for it, that can handle
 * more generic data than normal functions.
 */
class DOM_HTMLElement_toString : public DOM_Object
{
	virtual int Call(ES_Object* this_object0, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
	{
		DOM_Object *this_object = DOM_HOSTOBJECT(this_object0, DOM_Object);
		DOM_HTMLElement* element;
		DOM_THIS_OBJECT_EXISTING_OPTIONAL(element, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);
		if (element)
		{
			HTML_Element* this_element = element->GetThisElement();
			if (this_element->GetNsType() == NS_HTML)
			{
				HTML_ElementType type = this_element->Type();

				if (type == HE_A || type == HE_AREA)
				{
					DOMSetString(return_value, this_element->DOMGetAttribute(element->GetEnvironment(), ATTR_HREF, NULL, NS_IDX_DEFAULT, FALSE, TRUE));
					return ES_VALUE;
				}
			}
		}

		return DOM_toString(this_object0, GetEmptyTempBuf(), return_value);
	}
};

/* static */ void
DOM_HTMLElement::ConstructHTMLElementPrototypeL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Object* toString;
	DOMSetFunctionRuntimeL(toString = OP_NEW_L(DOM_HTMLElement_toString, ()), runtime);
	ES_Value val;
	DOMSetObject(&val, toString);
	PutL(object, "toString", val, runtime);
}

/* Also used in domcore/domdoc.cpp. */
OP_STATUS
SendEvent(DOM_EnvironmentImpl *environment, DOM_EventType event, HTML_Element *target, ES_Thread* thread)
{
	if (FramesDocument *frames_doc = environment->GetFramesDocument())
	{
		ShiftKeyState modifiers = SHIFTKEY_NONE;

		if (thread)
		{
			ES_ThreadInfo info = thread->GetOriginInfo();

			if (info.type == ES_THREAD_EVENT)
				modifiers = info.data.event.modifiers;
		}

		return frames_doc->HandleEvent(event, NULL, target, modifiers, 0, thread);
	}
	return OpStatus::OK;
}

/* static */ int
DOM_HTMLElement::blur(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);
	DOM_EnvironmentImpl *environment = element->GetEnvironment();

	if (FramesDocument *frames_doc = environment->GetFramesDocument())
		if (HTML_Document* html_doc = frames_doc->GetHtmlDocument())
		{
			HTML_Element *this_element = element->GetThisElement(), *current_element = html_doc->GetFocusedElement();
			current_element = current_element ? current_element : html_doc->GetNavigationElement();

			if (this_element && current_element == this_element)
			{
				/* Hack: don't allow a script to unfocus elements when
				   they are focused. */

				ES_Thread *thread = GetCurrentThread(origining_runtime);
				while (thread)
				{
					if (thread->Type() == ES_THREAD_EVENT)
					{
						DOM_Event *event = ((DOM_EventThread *) thread)->GetEvent();
						if (event->GetKnownType() == ONFOCUS && event->GetTarget() == element)
							return ES_FAILED;
					}
					thread = thread->GetInterruptedThread();
				}

				CALL_FAILED_IF_ERROR(frames_doc->Reflow(FALSE));

				html_doc->ClearFocusAndHighlight(FALSE);

				ES_Thread* interrupt_thread = environment->GetDOMRuntime() == origining_runtime ?
					GetCurrentThread(origining_runtime) : NULL;

				CALL_FAILED_IF_ERROR(SendEvent(environment, ONBLUR, this_element, interrupt_thread));
			}
		}

	return ES_FAILED;
}

/* static */ int
DOM_HTMLElement::focus(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);
	DOM_EnvironmentImpl *environment = element->GetEnvironment();

	// Called from scripts and internally from the select() method

	// Note that it can be suspended and restarted.

	if (FramesDocument *frames_doc = environment->GetFramesDocument())
		if (HTML_Document* html_doc = frames_doc->GetHtmlDocument())
		{
			HTML_Element *this_element = element->GetThisElement();
			HTML_Element *prev_focused = html_doc->GetFocusedElement();

			if (!frames_doc->GetDocRoot() || !frames_doc->GetDocRoot()->IsAncestorOf(this_element))
				return ES_FAILED;

			if (prev_focused != this_element)
			{
				// Some elements need layout boxes to be focused (specifically form elements)
				// so we need to Reflow to create some. Though Reflow() will not work
				// if we are in the WaitForStyles() state so in that case we need to
				// wait until we're out of that state first which we do with
				// DOM_DelayedLayoutListener.
				if (ShouldBlockWaitingForStyle(frames_doc, origining_runtime))
				{
					DOM_DelayedLayoutListener *listener = OP_NEW(DOM_DelayedLayoutListener, (frames_doc, GetCurrentThread(origining_runtime)));
					if (!listener)
						return ES_NO_MEMORY;
					return ES_SUSPEND|ES_RESTART;
				}
				CALL_FAILED_IF_ERROR(frames_doc->Reflow(FALSE));

				HTML_Element* elm_to_blur = prev_focused ? prev_focused : html_doc->GetNavigationElement(); // Not sure this is true.

				if (this_element->IsFocusable(frames_doc))
				{
					// Sending in HTML_Document::FOCUS_ORIGIN_DOM will prevent DOM events from being
					// sent so that we can send them with the right interrupt_thread
					BOOL highlight = html_doc->GetVisualDevice()->GetDrawFocusRect();

					if ((this_element->Type() == HE_INPUT || this_element->Type() == HE_TEXTAREA)
						&& !(prev_focused && (prev_focused->IsMatchingType(HE_INPUT, NS_HTML)
											|| prev_focused->IsMatchingType(HE_TEXTAREA, NS_HTML)))
						&& (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AllowAutofocusFormElement, element->GetHostName())
							|| (html_doc->GetFramesDocument()->GetNavigatedInHistory()
								&& DOM_Utils::IsInlineScriptOrWindowOnLoad(GetCurrentThread(origining_runtime)))))
					{
						InputType input_type = this_element->GetInputType();
						if (this_element->Type() == HE_TEXTAREA)
							input_type = INPUT_TEXT;

						switch (input_type)
						{
							case INPUT_TEXT:
							case INPUT_PASSWORD:
							case INPUT_URI:
							case INPUT_EMAIL:
							{
								ES_Thread *thread = GetCurrentThread(origining_runtime)->GetRunningRootThread();
								if (thread->Type() == ES_THREAD_EVENT)
								{
									DOM_Event* e = static_cast<DOM_EventThread*>(thread)->GetEvent();
									DOM_EventType event_type = e->GetKnownType();
									if (e->GetSynthetic()
											|| !(event_type == ONCLICK || event_type == ONMOUSEDOWN ||
												event_type == ONMOUSEUP  || event_type == ONKEYPRESS  ||
												event_type == ONKEYDOWN  || event_type == ONKEYUP     ||
												event_type == ONACTIVATE || event_type == ONRESET     ||
												event_type == ONSUBMIT   || event_type == ONFOCUS))
									{
										// Block the event thread, since it's caused by a script
										// the user didn't initiate.
										return ES_FAILED;
									}
								}
							}
						}
					}

					BOOL focused = html_doc->FocusElement(this_element, HTML_Document::FOCUS_ORIGIN_DOM, TRUE, TRUE, highlight);

					// FIXME: Should fix so that html_doc->FocusElement can send these events correctly
					HTML_Element* new_focused = html_doc->GetFocusedElement();
					if (!new_focused)
						new_focused = html_doc->GetNavigationElement(); // To match the code above

					ES_Thread* interrupt_thread = environment->GetDOMRuntime() == origining_runtime ?
						GetCurrentThread(origining_runtime) : NULL;

					if (elm_to_blur && elm_to_blur != new_focused)
						CALL_FAILED_IF_ERROR(SendEvent(environment, ONBLUR, elm_to_blur, interrupt_thread));

					if (focused)
						CALL_FAILED_IF_ERROR(SendEvent(environment, ONFOCUS, this_element, interrupt_thread));
				}
			}
		}

	return ES_FAILED;
}

#ifdef DOM_SELECTION_SUPPORT
/* static */ int
DOM_HTMLFormsElement::select(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);
	HTML_Element *this_element = element->GetThisElement();

	/* Only valid for <input>s and <textarea>s. */
	if (!this_element->IsMatchingType(Markup::HTE_INPUT, NS_HTML) && !this_element->IsMatchingType(Markup::HTE_TEXTAREA, NS_HTML))
		return ES_FAILED;

	// Note that focus() below may suspend/restart which would make this function
	// suspend/restart as well but then we just need to restart focus as well.
	if (argc < 0)
		return focus(this_object, NULL, -1, return_value, origining_runtime);

	// Select all text and focus the textfield/textarea
	DOM_EnvironmentImpl *environment = element->GetEnvironment();

	CALL_FAILED_IF_ERROR(this_element->DOMSelectContents(environment));
	ES_Thread* interrupt_thread = environment->GetDOMRuntime() == origining_runtime ?
				GetCurrentThread(origining_runtime) : NULL;
	CALL_FAILED_IF_ERROR(SendEvent(environment, ONSELECT, this_element, interrupt_thread));
	int ret_val = focus(this_object, NULL, 0, return_value, origining_runtime);
	return ret_val;
}

OP_STATUS
DOM_HTMLFormsElement::SetSelectionRange(ES_Runtime* origining_runtime, int start, int end, SELECTION_DIRECTION direction)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();

	if (start > end)
		start = end;

	if (start == end)
		this_element->DOMSetCaretPosition(environment, start);
	else
		this_element->DOMSetSelection(environment, start, end, direction);

	ES_Thread* interrupt_thread = environment->GetDOMRuntime() == origining_runtime ?
		GetCurrentThread(origining_runtime) : NULL;
	return SendEvent(environment, ONSELECT, this_element, interrupt_thread);
}

/* static */ int
DOM_HTMLFormsElement::setSelectionRange(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_ELEMENT, DOM_HTMLFormsElement);
	DOM_CHECK_ARGUMENTS("NN");

	HTML_Element *this_element = element->GetThisElement();

	/* Only valid for <input>s and <textarea>s. */
	if (!this_element->IsMatchingType(Markup::HTE_INPUT, NS_HTML) && !this_element->IsMatchingType(Markup::HTE_TEXTAREA, NS_HTML))
		return ES_FAILED;

	if (this_element->IsMatchingType(HE_INPUT, NS_HTML) && !InputElementHasSelectionAttributes(this_element))
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	int start = TruncateDoubleToInt(argv[0].value.number);
	int end = TruncateDoubleToInt(argv[1].value.number);

	SELECTION_DIRECTION direction = SELECTION_DIRECTION_NONE;
	if (argc > 2 && argv[2].type == VALUE_STRING)
	{
		if (uni_str_eq(argv[2].value.string, "forward"))
			direction = SELECTION_DIRECTION_FORWARD;
		else if (uni_str_eq(argv[2].value.string, "backward"))
			direction = SELECTION_DIRECTION_BACKWARD;
	}

	CALL_FAILED_IF_ERROR(element->SetSelectionRange(origining_runtime, start, end, direction));

	return ES_FAILED;
}
#endif // DOM_SELECTION_SUPPORT

/* static */ int
DOM_HTMLFormsElement::setCustomValidity(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);
	DOM_CHECK_ARGUMENTS("s");

	HTML_Element *form_control = element->GetThisElement();
	if (form_control->Type() == HE_FIELDSET || form_control->Type() == HE_BUTTON || form_control->Type() == HE_OUTPUT)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	uni_char* error_str = NULL;
	BOOL valid = *argv[0].value.string != '\0';
	if (valid)
	{
		 error_str = UniSetNewStr(argv[0].value.string);
		 if (!error_str)
			 return ES_NO_MEMORY;
	}

	form_control->SetSpecialAttr(FORMS_ATTR_USER_SET_INVALID_FLAG, ITEM_TYPE_STRING, (void*)error_str, TRUE, SpecialNs::NS_FORMS); // Create if missing
	// Can have changed pseudo classes so that we need to reapply styles
	DOM_EnvironmentImpl *environment = element->GetEnvironment();
	if (FramesDocument *frames_doc = environment->GetFramesDocument())
		if (LogicalDocument* logdoc = frames_doc->GetLogicalDocument())
			logdoc->GetLayoutWorkplace()->ApplyPropertyChanges(form_control, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);

	return ES_FAILED;
}

/* static */ int
DOM_HTMLFormsElement::checkValidity(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);

	if (FramesDocument *frames_doc = element->GetEnvironment()->GetFramesDocument())
	{
		HTML_Element *form_control = element->GetThisElement();
		FormValidator validator(frames_doc);
		ValidationResult val_res = validator.ValidateSingleControl(form_control);
		BOOL success;
		if (val_res.IsOk())
			success = TRUE;
		else
		{
			success = FALSE;
			element->GetFramesDocument()->SetNextOnInvalidCauseAlert(FALSE);
			ES_Thread *thread = GetCurrentThread(origining_runtime);
			validator.SendOnInvalidEvent(form_control, thread);
		}

		DOMSetBoolean(return_value, success);
		return ES_VALUE;
	}
	else
		return ES_FAILED;
}

/* static */ int
DOM_HTMLFormElement::checkValidity(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);
	if (FramesDocument* frames_doc = this_object->GetFramesDocument())
	{
		HTML_Element *form = element->GetThisElement();
		FormValidator validator(frames_doc);
		ES_Thread *thread = GetCurrentThread(origining_runtime);
		BOOL ret_val = validator.ValidateForm(form, FALSE, thread).IsOk();
		DOMSetBoolean(return_value, ret_val);
		return ES_VALUE;
	}
	else
		return ES_FAILED;
}

/* static */ int
DOM_HTMLFormElement::resetFromData(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
//	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);
//	HTML_Element* form = element->GetThisElement();

	// WF2-FIXME: Implement
	return ES_FAILED;
}

/* static */ int
DOM_HTMLFormElement::dispatchFormInputOrFormChange(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_FORMELEMENT, DOM_HTMLFormElement);

	if (FramesDocument* frames_doc = element->GetFramesDocument())
	{
		ES_Thread* thread = GetCurrentThread(origining_runtime);

		// Iterate over elements in the form
		CALL_FAILED_IF_ERROR(element->InitElementsCollection());

		FormValidator validator(frames_doc);
		DOM_Collection *elements = element->elements;

		for (int index = 0; index < elements->Length(); ++index)
		{
			HTML_Element* form_thing = elements->Item(index);
			if (data == DISPATCH_FORM_CHANGE)
				validator.SendOnFormChangeEvent(form_thing, thread);
			else
			{
				OP_ASSERT(data == DISPATCH_FORM_INPUT);
				validator.SendOnFormInputEvent(form_thing, thread);
			}
		}
	}
	return ES_FAILED;
}

/* static */ int
DOM_HTMLFormElement::submit(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);

	HTML_Element *form = element->GetThisElement();

	if (form->Type() != HE_FORM)
		return ES_FAILED;

	FramesDocument* frames_doc = element->GetFramesDocument();
	if (!frames_doc)
		return ES_FAILED;

	ES_Thread *origining_thread = GetCurrentThread(origining_runtime);
#ifdef WAND_SUPPORT
	BOOL is_from_submit_event = FALSE;
	ES_Thread *thread = origining_thread;
	while (thread)
	{
		if (thread->Type() == ES_THREAD_EVENT)
		{
			DOM_Event *event = ((DOM_EventThread *) thread)->GetEvent();
			if (event->GetKnownType() == ONSUBMIT && !event->GetSynthetic())
			{
				is_from_submit_event = TRUE;
				break;
			}
		}
		thread = thread->GetInterruptedThread();
	}
#endif // WAND_SUPPORT

	OP_STATUS status = FormManager::DOMSubmit(frames_doc, form, origining_thread
#ifdef WAND_SUPPORT
		 , is_from_submit_event
#endif// WAND_SUPPORT
		);
#if 0 // We have disabled the exception code as part of bug 306647: throwing submit() breaks sites
	if (status == OpStatus::ERR_OUT_OF_RANGE)
	{
		// WF2 spec:
		// Otherwise, if the form submission was initiated via the submit()
		// method, then instead of firing invalid events, a SYNTAX_ERR
		// exception shall be raised (and submission is aborted) if any
		// of the controls are invalid. [DOM3CORE]
		int rv = DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);
		if (rv == ES_EXCEPTION && return_value->type == VALUE_OBJECT)
		{
			// Add some extra information in the message. Not regulated by any specification, but to make it easier for users and web developers to understand why they fail to submit.
			ES_Object* exception = return_value->value.object;
			EcmaScript_Object* dom_exception = ES_Runtime::GetHostObject(exception);
			ES_Value exception_message;
			if (dom_exception->Get(UNI_L("message"), &exception_message) == OpBoolean::IS_TRUE &&
				exception_message.type == VALUE_STRING)
			{
				OpString full_message;
				CALL_FAILED_IF_ERROR(full_message.Set(exception_message.value.string));
				CALL_FAILED_IF_ERROR(full_message.Append(UNI_L(" - Form didn't validate in submit()")));
				exception_message.value.string = full_message.CStr();
				dom_exception->Put(UNI_L("message"), exception_message);
			}
		}
		return rv;
	}
#endif // 0 - bug 306647 - disable throwing submit
	CALL_FAILED_IF_ERROR(status);
	/* DOMSubmit above will usually submit the form by loading an URL, but
	   DocumentManager::OpenURL will not be called immediately, instead a
	   message is posted.  Returning ES_SUSPEND here causes the scheduler to
	   take a pause by simply breaking out of the run loop and post a message
	   to itself to continue, allowing OpenURL to be called first. */
	return ES_SUSPEND;
}

/* static */ int
DOM_HTMLFormElement::reset(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_FORMELEMENT, DOM_HTMLElement);
	DOM_EnvironmentImpl *environment = element->GetEnvironment();

	ES_Thread* interrupt_thread = environment->GetDOMRuntime() == origining_runtime ?
		GetCurrentThread(origining_runtime) : NULL;
	CALL_FAILED_IF_ERROR(SendEvent(environment, ONRESET, element->GetThisElement(), interrupt_thread));

	return ES_FAILED;
}

/* static */ int
DOM_HTMLFormElement::item(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_FORMELEMENT, DOM_HTMLFormElement);

	CALL_FAILED_IF_ERROR(element->InitElementsCollection());
	DOM_Collection *collection = element->GetElementsCollection();

	return DOM_Collection::item(collection, argv, argc, return_value, origining_runtime);
}

/* static */ int
DOM_HTMLFormElement::namedItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_FORMELEMENT, DOM_HTMLFormElement);

	CALL_FAILED_IF_ERROR(element->InitElementsCollection());
	DOM_Collection *collection = element->GetElementsCollection();

	return DOM_HTMLCollection::namedItem(collection, argv, argc, return_value, origining_runtime);
}

/* static */ int
DOM_HTMLElement::insertAdjacentElement(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	if (argc >= 0)
	{
		DOM_THIS_OBJECT(this_node, DOM_TYPE_NODE, DOM_Node);
		DOM_CHECK_ARGUMENTS("so");
		DOM_ARGUMENT_OBJECT(new_node, 1, DOM_TYPE_NODE, DOM_Node);

		DOM_Node *parent;
		DOM_Node *reference;

		const uni_char *where = argv[0].value.string;

		if (uni_stri_eq(where, "BEFOREBEGIN"))
		{
			CALL_FAILED_IF_ERROR(this_node->GetParentNode(parent));
			reference = this_node;
		}
		else if (uni_stri_eq(where, "AFTERBEGIN"))
		{
			parent = this_node;
			CALL_FAILED_IF_ERROR(this_node->GetFirstChild(reference));
		}
		else if (uni_stri_eq(where, "BEFOREEND"))
		{
			parent = this_node;
			reference = NULL;
		}
		else if (uni_stri_eq(where, "AFTEREND"))
		{
			CALL_FAILED_IF_ERROR(this_node->GetParentNode(parent));
			CALL_FAILED_IF_ERROR(this_node->GetNextSibling(reference));
		}
		else
			return ES_FAILED;

		ES_Value arguments[2];
		DOMSetObject(&arguments[0], new_node);
		DOMSetObject(&arguments[1], reference);

		return DOM_Node::insertBefore(parent, arguments, 2, return_value, origining_runtime);
	}
	else
		return DOM_Node::insertBefore(NULL, NULL, -1, return_value, origining_runtime);
}

/* static */ int
DOM_HTMLElement::insertAdjacentHTML(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);
    return element->InsertAdjacent(OP_ATOM_insertAdjacentHTML, this_object, argv, argc, return_value, origining_runtime);
}

/* static */ int
DOM_HTMLElement::insertAdjacentText(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);
    return element->InsertAdjacent(OP_ATOM_insertAdjacentText, this_object, argv, argc, return_value, origining_runtime);
}

/* static */ int
DOM_HTMLElement::click(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);
	DOM_EnvironmentImpl *environment = element->GetEnvironment();
	HTML_Element *this_element = element->this_element;

	// If click() is called during the handling of an ONCLICK
	// event from the same element or from one of its children, then
	// do nothing.  This avoids event loops in situations such as
	//
	//   <td onclick="this.children.tags('A')[0].click()"><a>Foo</a></td>
	ES_Thread *thread = GetCurrentThread(origining_runtime);
	DOM_EventType event_type = DOM_EVENT_NONE;

	while (thread)
	{
		if (thread->Type() == ES_THREAD_EVENT)
		{
			event_type = static_cast<DOM_EventThread*>(thread)->GetEventType();

			if (event_type == ONCLICK)
			{
				DOM_Object *target = static_cast<DOM_EventThread*>(thread)->GetEventTarget();

				if (target->IsA(DOM_TYPE_NODE) && element->IsAncestorOf(static_cast<DOM_Node*>(target)))
					return ES_FAILED; // click inside an onclick to the same node not allowed -> just ignore and return
			}
		}

		thread = thread->GetInterruptedThread();
	}

	// JS1.3 says that the onclick event shouldn't be sent
	// but that is not true anymore.
	CALL_FAILED_IF_ERROR(BeforeClick(element));
	CALL_FAILED_IF_ERROR(SendEvent(environment, ONCLICK, this_element, GetCurrentThread(origining_runtime)));

	return ES_FAILED;
}

static void TranslateTableItemData(int data, HTML_ElementType &type, const uni_char *&name)
{
	if (data == 0)
	{
		type = HE_CAPTION;
		name = UNI_L("caption");
	}
	else if (data == 1)
	{
		type = HE_THEAD;
		name = UNI_L("thead");
	}
	else
	{
		type = HE_TFOOT;
		name = UNI_L("tfoot");
	}
}

/* static */ int
DOM_HTMLTableElement::createTableItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
#ifdef DOM2_MUTATION_EVENTS
	if (argc >= 0)
#endif // DOM2_MUTATION_EVENTS
	{
		DOM_THIS_OBJECT(table, DOM_TYPE_HTML_TABLEELEMENT, DOM_HTMLTableElement);

		HTML_ElementType type;
		const uni_char *name;

		TranslateTableItemData(data, type, name);

		DOM_HTMLElement *item;
		CALL_FAILED_IF_ERROR(table->GetChildElement(item, type));

		if (!item)
		{
			CALL_FAILED_IF_ERROR(CreateElement(item, table, name));

			DOM_HTMLElement *reference;

			if (type == HE_CAPTION)
				PUT_FAILED_IF_ERROR(table->GetChildElement(reference, HE_THEAD));
			else
				reference = NULL;

			if (type == HE_THEAD || !reference)
				PUT_FAILED_IF_ERROR(table->GetChildElement(reference, HE_TFOOT));

			if (type == HE_TFOOT || !reference)
				PUT_FAILED_IF_ERROR(table->GetChildElement(reference, HE_TBODY));

			ES_Value arguments[2];
			DOMSetObject(&arguments[0], item);
			DOMSetObject(&arguments[1], reference);

			return DOM_Node::insertBefore(table, arguments, 2, return_value, origining_runtime);
		}

		DOMSetObject(return_value, item);
		return ES_VALUE;
	}
#ifdef DOM2_MUTATION_EVENTS
	else
		return DOM_Node::insertBefore(NULL, NULL, -1, return_value, origining_runtime);
#endif // DOM2_MUTATION_EVENTS
}

/* static */ int
DOM_HTMLTableElement::deleteTableItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
#ifdef DOM2_MUTATION_EVENTS
	if (argc >= 0)
#endif // DOM2_MUTATION_EVENTS
	{
		DOM_THIS_OBJECT(table, DOM_TYPE_HTML_TABLEELEMENT, DOM_HTMLTableElement);

		HTML_ElementType type;
		const uni_char *name;

		TranslateTableItemData(data, type, name);

		DOM_HTMLElement *item;
		CALL_FAILED_IF_ERROR(table->GetChildElement(item, type));

		if (item)
		{
			ES_Value arguments[1];
			DOMSetObject(&arguments[0], item);

			int result = DOM_Node::removeChild(table, arguments, 1, return_value, origining_runtime);
			if (result != ES_VALUE)
				return result;
		}
	}
#ifdef DOM2_MUTATION_EVENTS
	else
	{
		int result = DOM_Node::removeChild(NULL, NULL, -1, return_value, origining_runtime);
		if (result != ES_VALUE)
			return result;
	}
#endif // DOM2_MUTATION_EVENTS

	return ES_FAILED;
}

/* static */ int
DOM_HTMLTableElement::insertRow(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_HTMLTableElement *table;
	DOM_Collection *rows;
	int count, index;
	ES_Value arguments[2];

#ifdef DOM2_MUTATION_EVENTS
	if (argc >= 0)
#endif // DOM2_MUTATION_EVENTS
	{
		DOM_THIS_OBJECT_EXISTING(table, DOM_TYPE_HTML_TABLEELEMENT, DOM_HTMLTableElement);
		DOM_CHECK_ARGUMENTS("N");
		index = TruncateDoubleToInt(argv[0].value.number);
	}
#ifdef DOM2_MUTATION_EVENTS
	else
	{
		ES_Value index_value;
		OP_BOOLEAN status = origining_runtime->GetName(return_value->value.object, UNI_L("insertRowIndex"), &index_value);
		CALL_FAILED_IF_ERROR(status);

		int result = DOM_Node::insertBefore(NULL, NULL, -1, return_value, origining_runtime);

		if (status == OpBoolean::IS_TRUE)
		{
			index = TruncateDoubleToInt(index_value.value.number);

			if (result == (ES_SUSPEND | ES_RESTART))
				goto suspend;
			else if (result != ES_VALUE)
				return result;

			DOM_HTMLElement *element = DOM_VALUE2OBJECT(*return_value, DOM_HTMLElement);
			if (element->IsA(DOM_TYPE_HTML_TABLEROWELEMENT))
				return ES_VALUE;

			OP_ASSERT(element->IsA(DOM_TYPE_HTML_TABLESECTIONELEMENT));

			DOM_Node *node;
			CALL_FAILED_IF_ERROR(element->GetParentNode(node));

			OP_ASSERT(node->IsA(DOM_TYPE_HTML_TABLEELEMENT));

			this_object = table = (DOM_HTMLTableElement *) node;
		}
		else
			return result;
	}
#endif // DOM2_MUTATION_EVENTS

	CALL_FAILED_IF_ERROR(table->InitRowsCollection());
	rows = table->GetRowsCollection();

	count = rows->Length();

	if (index < -1 || index > count)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	HTML_Element *reference_element, *parent_element;

	if (count == 0)
	{
		DOM_HTMLElement *tbody;
		CALL_FAILED_IF_ERROR(table->GetChildElement(tbody, HE_TBODY));

		if (!tbody)
		{
			CALL_FAILED_IF_ERROR(CreateElement(tbody, table, UNI_L("tbody")));

			DOM_HTMLElement *tfoot;
			CALL_FAILED_IF_ERROR(table->GetChildElement(tfoot, HE_TFOOT));

			DOMSetObject(&arguments[0], tbody);
			DOMSetObject(&arguments[1], tfoot);

			int result = DOM_Node::insertBefore(table, arguments, 2, return_value, origining_runtime);

#ifdef DOM2_MUTATION_EVENTS
			if (result == (ES_SUSPEND | ES_RESTART))
				goto suspend;
#endif // DOM2_MUTATION_EVENTS

			if (result != ES_VALUE)
				return result;
		}

		reference_element = NULL;
		parent_element = tbody->GetThisElement();
	}
	else
	{
		if (index == -1 || index == count)
			reference_element = rows->Item(count - 1);
		else
			reference_element = rows->Item(index);

		parent_element = NULL;
	}

	DOM_Node *reference;
	if (reference_element)
		CALL_FAILED_IF_ERROR(table->GetEnvironment()->ConstructNode(reference, reference_element, table->owner_document));
	else
		reference = NULL;

	DOM_Node *parent;
	if (parent_element)
		CALL_FAILED_IF_ERROR(table->GetEnvironment()->ConstructNode(parent, parent_element, table->owner_document));
	else
		CALL_FAILED_IF_ERROR(reference->GetParentNode(parent));

	DOM_HTMLElement *row;
	CALL_FAILED_IF_ERROR(CreateElement(row, table, UNI_L("tr")));

	if (index == -1 || index == count)
		reference = NULL;

	DOMSetObject(&arguments[0], row);
	DOMSetObject(&arguments[1], reference);

	return DOM_Node::insertBefore(parent, arguments, 2, return_value, origining_runtime);

#ifdef DOM2_MUTATION_EVENTS
suspend:
	ES_Value index_value;
	DOMSetNumber(&index_value, index);

	CALL_FAILED_IF_ERROR(origining_runtime->PutName(return_value->value.object, UNI_L("insertRowIndex"), index_value));
	return ES_SUSPEND | ES_RESTART;
#endif // DOM2_MUTATION_EVENTS
}

/* static */ int
DOM_HTMLTableElement::deleteRow(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
#ifdef DOM2_MUTATION_EVENTS
	if (argc >= 0)
#endif // DOM2_MUTATION_EVENTS
	{
		DOM_THIS_OBJECT(table, DOM_TYPE_HTML_TABLEELEMENT, DOM_HTMLTableElement);
		DOM_CHECK_ARGUMENTS("N");

		CALL_FAILED_IF_ERROR(table->InitRowsCollection());

		DOM_Collection *rows = table->GetRowsCollection();

		int count = rows->Length();
		OP_ASSERT(argv[0].type == VALUE_NUMBER);
		int index = TruncateDoubleToInt(argv[0].value.number);

		if (index == -1)
			index = count - 1;

		if (index < 0 || index >= count)
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

		DOM_Node *row;
		CALL_FAILED_IF_ERROR(table->GetEnvironment()->ConstructNode(row, rows->Item(index), table->owner_document));

		DOM_Node *parent;
		CALL_FAILED_IF_ERROR(row->GetParentNode(parent));

		ES_Value arguments[1];
		DOMSetObject(&arguments[0], row);

		return DOM_Node::removeChild(parent, arguments, 1, return_value, origining_runtime);
	}
#ifdef DOM2_MUTATION_EVENTS
	else
		return DOM_Node::removeChild(NULL, NULL, -1, return_value, origining_runtime);
#endif // DOM2_MUTATION_EVENTS
}

/* static */ int
DOM_HTMLTableRowElement::insertCell(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
#ifdef DOM2_MUTATION_EVENTS
	if (argc >= 0)
#endif // DOM2_MUTATION_EVENTS
	{
		DOM_THIS_OBJECT(row, DOM_TYPE_HTML_TABLEROWELEMENT, DOM_HTMLTableRowElement);
		DOM_CHECK_ARGUMENTS("N");
		DOM_Node *reference = NULL;
		OP_ASSERT(argv[0].type == VALUE_NUMBER);
		int index = TruncateDoubleToInt(argv[0].value.number);
		if (index < -1)
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

		if (index >= 0)
		{
			HTML_Element* stop = row->this_element->NextSiblingActual();
			HTML_Element* it = row->this_element->NextActual();
			while (stop != it)
			{
				if (it->IsMatchingType(HE_TD, NS_HTML) || it->IsMatchingType(HE_TH, NS_HTML))
				{
					if (index == 0)
						break;
					--index;
					it = it->NextSiblingActual();
				}
				else
					it = it->NextActual();
			}
			if (stop == it)
			{
				// The index didn't exist. If it's 0 now, it was just 1 short
				// and we insert at the end with reference == NULL
				if (index > 0)
					return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
			}
			else
				CALL_FAILED_IF_ERROR(row->GetEnvironment()->ConstructNode(reference, it, row->owner_document));
		}
		DOM_HTMLElement *cell;
		CALL_FAILED_IF_ERROR(CreateElement(cell, row, UNI_L("td")));

		ES_Value arguments[2];
		DOMSetObject(&arguments[0], cell);
		DOMSetObject(&arguments[1], reference);

		return DOM_Node::insertBefore(row, arguments, 2, return_value, origining_runtime);
	}
#ifdef DOM2_MUTATION_EVENTS
	else
		return DOM_Node::insertBefore(NULL, NULL, -1, return_value, origining_runtime);
#endif // DOM2_MUTATION_EVENTS
}

/* static */ int
DOM_HTMLTableRowElement::deleteCell(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
#ifdef DOM2_MUTATION_EVENTS
	if (argc >= 0)
#endif // DOM2_MUTATION_EVENTS
	{
		DOM_THIS_OBJECT(row, DOM_TYPE_HTML_TABLEROWELEMENT, DOM_HTMLTableRowElement);
		DOM_CHECK_ARGUMENTS("N");

		CALL_FAILED_IF_ERROR(row->InitCellsCollection());

		DOM_Collection *cells = row->GetCellsCollection();

		int count = cells->Length();
		OP_ASSERT(argv[0].type == VALUE_NUMBER);
		int index = TruncateDoubleToInt(argv[0].value.number);

		if (index == -1)
			index = count - 1;

		if (index < 0 || index >= count)
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

		DOM_Node *cell;
		CALL_FAILED_IF_ERROR(row->GetEnvironment()->ConstructNode(cell, cells->Item(index), row->owner_document));

		ES_Value arguments[1];
		DOMSetObject(&arguments[0], cell);

		return DOM_Node::removeChild(row, arguments, 1, return_value, origining_runtime);
	}
#ifdef DOM2_MUTATION_EVENTS
	else
		return DOM_Node::removeChild(NULL, NULL, -1, return_value, origining_runtime);
#endif // DOM2_MUTATION_EVENTS
}

/* static */ int
DOM_HTMLTableSectionElement::insertRow(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
#ifdef DOM2_MUTATION_EVENTS
	if (argc >= 0)
#endif // DOM2_MUTATION_EVENTS
	{
		DOM_THIS_OBJECT(section, DOM_TYPE_HTML_TABLESECTIONELEMENT, DOM_HTMLTableSectionElement);
		DOM_CHECK_ARGUMENTS("N");

		CALL_FAILED_IF_ERROR(section->InitRowsCollection());

		DOM_Collection *rows = section->GetRowsCollection();

		int count = rows->Length();
		int index = TruncateDoubleToInt(argv[0].value.number);

		if (index == -1)
			index = count;

		if (index < 0 || index > count)
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

		DOM_Node *reference;

		if (index == count)
			reference = NULL;
		else
			CALL_FAILED_IF_ERROR(section->GetEnvironment()->ConstructNode(reference, rows->Item(index), section->owner_document));

		DOM_HTMLElement *row;
		CALL_FAILED_IF_ERROR(CreateElement(row, section, UNI_L("tr")));

		ES_Value arguments[2];
		DOMSetObject(&arguments[0], row);
		DOMSetObject(&arguments[1], reference);

		return DOM_Node::insertBefore(section, arguments, 2, return_value, origining_runtime);
	}
#ifdef DOM2_MUTATION_EVENTS
	else
		return DOM_Node::insertBefore(NULL, NULL, -1, return_value, origining_runtime);
#endif // DOM2_MUTATION_EVENTS
}

/* static */ int
DOM_HTMLTableSectionElement::deleteRow(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
#ifdef DOM2_MUTATION_EVENTS
	if (argc >= 0)
#endif // DOM2_MUTATION_EVENTS
	{
		DOM_THIS_OBJECT(section, DOM_TYPE_HTML_TABLESECTIONELEMENT, DOM_HTMLTableSectionElement);
		DOM_CHECK_ARGUMENTS("N");

		CALL_FAILED_IF_ERROR(section->InitRowsCollection());

		DOM_Collection *rows = section->GetRowsCollection();

		int count = rows->Length();
		int index = TruncateDoubleToInt(argv[0].value.number);

		if (index == -1)
			index = count - 1;

		if (index < 0 || index >= count)
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

		DOM_Node *row;
		CALL_FAILED_IF_ERROR(section->GetEnvironment()->ConstructNode(row, rows->Item(index), section->GetOwnerDocument()));

		ES_Value arguments[1];
		DOMSetObject(&arguments[0], row);

		return DOM_Node::removeChild(section, arguments, 1, return_value, origining_runtime);
	}
#ifdef DOM2_MUTATION_EVENTS
	else
		return DOM_Node::removeChild(NULL, NULL, -1, return_value, origining_runtime);
#endif // DOM2_MUTATION_EVENTS
}

class DOM_ParseAndReplaceState
	: public DOM_Object
{
public:
	enum Point { REMOVE_CHILD, INSERT_BEFORE } point;
	DOM_Node *parent, *insert_before, *new_node;
	ES_Value return_value;
	OpString string;
	int string_length;
	BOOL is_xml;

	DOM_ParseAndReplaceState(Point point)
		: point(point), parent(NULL), insert_before(NULL), new_node(NULL), string_length(0), is_xml(FALSE)
	{
	}

	virtual void GCTrace()
	{
		GCMark(return_value);
		GCMark(parent);
		GCMark(insert_before);
		GCMark(new_node);
	}
};

ES_PutState
DOM_HTMLElement::ParseAndReplace(OpAtom prop, ES_Value* value, DOM_Runtime *origining_runtime, int where, DOM_Object *restart_object)
{
	DOM_ParseAndReplaceState *state = NULL;
	DOM_ParseAndReplaceState::Point point = DOM_ParseAndReplaceState::REMOVE_CHILD;
	BOOL is_restarted = FALSE;
	BOOL no_side_effects = FALSE;

	DOM_EnvironmentImpl *environment = GetEnvironment();
#ifdef NS4P_COMPONENT_PLUGINS
	if (FramesDocument* doc = environment->GetFramesDocument())
		if (LayoutWorkplace* wp = doc->GetLayoutWorkplace())
			if (wp->IsTraversing() || wp->IsReflowing())
				return PUT_FAILED;
#endif // NS4P_COMPONENT_PLUGINS

	HTML_Element *parent = NULL;
	HTML_Element *insert_before = NULL;
	DOM_Node *new_node = NULL;
	ES_Value return_value;
	const uni_char *string;
	int string_length;
	BOOL is_xml = FALSE;

	if (!restart_object)
	{
		if (value->type == VALUE_NULL)
			DOMSetString(value);
		else if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;

		if (!value->value.string)
			return PUT_FAILED;

		switch (prop)
		{
		case OP_ATOM_outerHTML:
		case OP_ATOM_outerText:
			parent = this_element->ParentActual();
			insert_before = this_element->SucActual();
			if (parent && parent->Type() == HE_DOC_ROOT)
				return PUT_READ_ONLY;
			break;

		case OP_ATOM_innerHTML:
		case OP_ATOM_innerText:
			parent = this_element;
			break;

		case OP_ATOM_insertAdjacentHTML:
		case OP_ATOM_insertAdjacentText:
			switch (where)
			{
			case BEFORE_BEGIN:
				parent = this_element->ParentActual();
				insert_before = this_element;
				break;

			case AFTER_BEGIN:
				parent = this_element;
				insert_before = this_element->FirstChildActual();
				break;

			case AFTER_END:
				parent = this_element->ParentActual();
				insert_before = this_element->SucActual();
				break;

			case BEFORE_END:
				parent = this_element;
			}
		}

		string = value->value.string;
		string_length = value->GetStringLength();
		is_xml = GetOwnerDocument() ? GetOwnerDocument()->IsXML() : parent->GetNsIdx() != NS_IDX_HTML;
		if (prop == OP_ATOM_innerHTML && !is_xml && HTML5Parser::GetInsertionModeFromElementType(parent->Type(), TRUE) == HTML5TreeBuilder::IN_BODY)
		{
			const unsigned max_probe = 64;
			no_side_effects = TRUE;

			for(unsigned i = 0; i < max_probe; i++)
			{
				const uni_char ch = string[i];
				if (!ch) // string ends here and just text so far
				{
					prop = OP_ATOM_innerText;
					break;
				}

				/* stop on any sign of markup or line breaks (see below,
				* expanded as <br> for innerText). */
				if (ch == '<' || ch == '&' || ch == '\r' || ch == '\n')
					break;
			}
		}
	}
	else
	{
		state = (DOM_ParseAndReplaceState *) restart_object;
		point = state->point;
		is_restarted = TRUE;

		parent = state->parent->GetThisElement();
		insert_before = state->insert_before ? state->insert_before->GetThisElement() : NULL;
		new_node = state->new_node;
		return_value = state->return_value;
		string = state->string.CStr();
		string_length = state->string_length;
		is_xml = state->is_xml;
	}

	if (!parent || insert_before && insert_before->ParentActual() != parent)
		return PUT_FAILED;

#define IS_POINT(p) (point == DOM_ParseAndReplaceState::p)
#define SET_POINT(p) do { point = DOM_ParseAndReplaceState::p; if (state) state->point = DOM_ParseAndReplaceState::p; } while (0)
#define IS_RESTARTED (is_restarted ? (is_restarted = FALSE, TRUE) : FALSE)

	if (IS_POINT(REMOVE_CHILD))
	{
		if (prop == OP_ATOM_innerHTML || prop == OP_ATOM_innerText)
		{
			int result = RemoveAllChildren(IS_RESTARTED, &return_value, origining_runtime);

#ifdef DOM2_MUTATION_EVENTS
			if (result == (ES_SUSPEND | ES_RESTART))
				goto suspend;
#endif // DOM2_MUTATION_EVENTS

			if (result != ES_FAILED)
			{
				*value = return_value;
				return ConvertCallToPutName(result, value);
			}
		}
		else if (prop == OP_ATOM_outerHTML || prop == OP_ATOM_outerText)
		{
			int result = ES_FAILED;

			if (!IS_RESTARTED)
			{
				DOM_Node *parent_node;
				PUT_FAILED_IF_ERROR(GetParentNode(parent_node));

				ES_Value arguments[1];
				DOMSetObject(&arguments[0], this);

				result = DOM_Node::removeChild(parent_node, arguments, 1, &return_value, origining_runtime);
			}
#ifdef DOM2_MUTATION_EVENTS
			else
				result = DOM_Node::removeChild(NULL, NULL, -1, &return_value, origining_runtime);

			if (result == (ES_SUSPEND | ES_RESTART))
				goto suspend;
#endif // DOM2_MUTATION_EVENTS

			if (result != ES_VALUE)
			{
				*value = return_value;
				return ConvertCallToPutName(result, value);
			}
		}

		if (prop == OP_ATOM_innerHTML || prop == OP_ATOM_outerHTML || prop == OP_ATOM_insertAdjacentHTML)
		{
			if (!*string && no_side_effects)
				return PUT_SUCCESS;

			DOM_DocumentFragment *fragment;
			PUT_FAILED_IF_ERROR(DOM_DocumentFragment::Make(fragment, this));

			DOM_EnvironmentImpl::CurrentState state(environment, origining_runtime);
			state.SetOwnerDocument(GetOwnerDocument());

			OP_STATUS status = fragment->GetPlaceholderElement()->DOMSetInnerHTML(environment, string, string_length, parent, is_xml);

			if (OpStatus::IsError(status))
			{
				if (OpStatus::IsMemoryError(status))
					return PUT_NO_MEMORY;
				else if (is_xml)
					return DOM_PUTNAME_DOMEXCEPTION(SYNTAX_ERR);
				else
					return PUT_SUCCESS;
			}

			new_node = fragment;
		}
		else
		{
			// outerText, innerText
			// Line breaks should become <br> elements

			const uni_char* text_p = string;
			DOM_DocumentFragment *fragment = NULL;
			while(*text_p)
			{
				if (new_node && !fragment)
				{
					// Collect nodes under a fragment
					PUT_FAILED_IF_ERROR(DOM_DocumentFragment::Make(fragment, this));
				}
				else
				{
					const uni_char* text_start = text_p;
					// Scan to first line break
					while (*text_p && *text_p != '\r' && *text_p != '\n')
						text_p++;

					if (text_p == text_start) // there are linebreaks
					{
						// CR, LF, CRLF and LFCR all become a single <br> node
						DOM_HTMLElement* br_element;
						PUT_FAILED_IF_ERROR(DOM_HTMLElement::CreateElement(br_element, this, UNI_L("br")));
						uni_char first_linebreak_char = *text_p++;
						if (*text_p != first_linebreak_char && (*text_p == '\r' || *text_p == '\n'))
							text_p++;
						new_node = br_element;
					}
					else
					{
						DOM_Text *text;
						uni_char old_terminator = *text_p;
						*const_cast<uni_char*>(text_p) = '\0';
						OP_STATUS status = DOM_Text::Make(text, this, string);
						*const_cast<uni_char*>(text_p) = old_terminator;
						PUT_FAILED_IF_ERROR(status);
						new_node = text;
					}
					string = text_p;
				}

				if (fragment)
					PUT_FAILED_IF_ERROR(fragment->GetPlaceholderElement()->DOMInsertChild(environment, new_node->GetThisElement(), NULL));
			}

			if (fragment)
				new_node = fragment;
			else if (!new_node)
				return PUT_SUCCESS; // Empty string
		}

		SET_POINT(INSERT_BEFORE);
	}

	if (IS_POINT(INSERT_BEFORE))
	{
		int result;

		if (!IS_RESTARTED)
		{
			DOM_Node *parent_node;
			DOM_Node *insert_before_node;

			PUT_FAILED_IF_ERROR(environment->ConstructNode(parent_node, parent, owner_document));

			if (insert_before)
				PUT_FAILED_IF_ERROR(environment->ConstructNode(insert_before_node, insert_before, owner_document));
			else
				insert_before_node = NULL;

			ES_Value arguments[2];
			DOMSetObject(&arguments[0], new_node);
			DOMSetObject(&arguments[1], insert_before_node);

			DOM_EnvironmentImpl::CurrentState state(environment);
			state.SetSkipScriptElements();

			result = DOM_Node::insertBefore(parent_node, arguments, 2, &return_value, origining_runtime);
		}
		else
			result = DOM_Node::insertBefore(NULL, NULL, -1, &return_value, origining_runtime);

		if (result == (ES_SUSPEND | ES_RESTART))
			goto suspend;

		if (result == ES_NO_MEMORY)
			return PUT_NO_MEMORY;
		else if (result != ES_VALUE)
		{
			*value = return_value;
			return ConvertCallToPutName(result, value);
		}
	}

#undef IS_POINT
#undef SET_POINT
#undef IS_RESTARTED

	return PUT_SUCCESS;

suspend:
	if (!state)
	{
		PUT_FAILED_IF_ERROR(DOMSetObjectRuntime(state = OP_NEW(DOM_ParseAndReplaceState, (point)), environment->GetDOMRuntime()));
		PUT_FAILED_IF_ERROR(state->string.Set(value->value.string));
		state->string_length = value->GetStringLength();
		state->parent = (DOM_Node *) parent->GetESElement();
		if (insert_before)
			PUT_FAILED_IF_ERROR(environment->ConstructNode(state->insert_before, insert_before, owner_document));
		state->is_xml = is_xml;
	}

	state->return_value = return_value;

	DOMSetObject(value, state);
	return PUT_SUSPEND;
}

int DOM_HTMLElement::InsertAdjacent(OpAtom prop, DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("ss");

    int where = BEFORE_BEGIN;
	const uni_char *where_string = argv[0].value.string;

    if (uni_stricmp(UNI_L("BeforeBegin"), where_string) == 0)
        where = BEFORE_BEGIN;
    else if (uni_stricmp(UNI_L("AfterBegin"), where_string) == 0)
        where = AFTER_BEGIN;
    else if (uni_stricmp(UNI_L("BeforeEnd"), where_string) == 0)
        where = BEFORE_END;
    else if (uni_stricmp(UNI_L("AfterEnd"), where_string) == 0)
        where = AFTER_END;
    else
        return ES_FAILED;

    switch (ParseAndReplace(prop, &argv[1], origining_runtime, where))
	{
	case PUT_NO_MEMORY:
		return ES_NO_MEMORY;

	case PUT_SUSPEND:
		/* ParseAndReplace will suspend before it removes any nodes when there
		   are Mutation Events involved, but nodes should never be removed by
		   calls from here. */
		OP_ASSERT(FALSE);
		/* fall through */

	default:
		return ES_FAILED;
	}
}

#ifdef DOM2_MUTATION_EVENTS
OP_STATUS
DOM_HTMLElement::SendAttrModified(ES_Thread *interrupt_thread, OpAtom property_name, const uni_char *prevValue, const uni_char *newValue)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();

	if (environment->IsEnabled() && environment->HasEventHandlers(DOMATTRMODIFIED))
		return DOM_Element::SendAttrModified(interrupt_thread, HTM_Lex::GetAttrString(DOM_AtomToHtmlAttribute(property_name)), NS_IDX_HTML, prevValue, newValue);
	else
		return OpStatus::OK;
}
#endif // DOM2_MUTATION_EVENTS

OP_STATUS
DOM_HTMLElement::GetChildElement(DOM_HTMLElement *&element, HTML_ElementType type, unsigned index)
{
	for (HTML_Element *iter = this_element->FirstChildActual(); iter; iter = iter->NextSiblingActual())
		if (iter->Type() == type)
			if (index == 0)
			{
				DOM_Node *node;
				RETURN_IF_ERROR(GetEnvironment()->ConstructNode(node, iter, owner_document));
				element = (DOM_HTMLElement *) node;
				return OpStatus::OK;
			}
			else
				--index;

	element = NULL;
	return OpStatus::OK;
}

ES_PutState
DOM_HTMLElement::PutChildElement(OpAtom property_name, ES_Value* value, DOM_Runtime* origining_runtime, ES_Object *restart_object)
{
	int result;

	if (!restart_object)
	{
		HTML_ElementType element_type;

		if (property_name == OP_ATOM_body)
			element_type = HE_BODY;
		else if (property_name == OP_ATOM_caption)
			element_type = HE_CAPTION;
		else if (property_name == OP_ATOM_tHead)
			element_type = HE_THEAD;
		else
			element_type = HE_TFOOT;

		DOM_HTMLElement *old_child;
		PUT_FAILED_IF_ERROR(GetChildElement(old_child, element_type));

		DOM_HTMLElement *new_child = NULL;
		if (DOM_Object *object = DOM_VALUE2OBJECT(*value, DOM_Object))
			if (object->IsA(DOM_TYPE_HTML_ELEMENT))
			{
				new_child = (DOM_HTMLElement *) object;

				if (new_child->GetThisElement()->Type() != element_type)
					new_child = NULL;
			}
		if (!new_child)
			return DOM_PUTNAME_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);

		if (old_child)
		{
			ES_Value arguments[2];
			DOMSetObject(&arguments[0], new_child);
			DOMSetObject(&arguments[1], old_child);

			result = DOM_Node::replaceChild(this, arguments, 2, value, origining_runtime);
		}
		else
		{
			DOM_HTMLElement *ref_child = NULL;

			if (property_name != OP_ATOM_body)
			{
				if (property_name == OP_ATOM_caption)
					PUT_FAILED_IF_ERROR(GetChildElement(ref_child, HE_THEAD));

				if (property_name == OP_ATOM_tHead || !ref_child)
					PUT_FAILED_IF_ERROR(GetChildElement(ref_child, HE_TFOOT));

				if (property_name == OP_ATOM_tFoot || !ref_child)
					PUT_FAILED_IF_ERROR(GetChildElement(ref_child, HE_TBODY));
			}

			ES_Value arguments[2];
			DOMSetObject(&arguments[0], new_child);
			DOMSetObject(&arguments[1], ref_child);

			result = DOM_Node::insertBefore(this, arguments, 2, value, origining_runtime);
		}

		if (result == (ES_SUSPEND | ES_RESTART))
			if (old_child)
				PUT_FAILED_IF_ERROR(origining_runtime->PutName(value->value.object, UNI_L("isReplaceChild"), *value));
	}
	else
	{
		ES_Value return_value;
		OP_BOOLEAN isReplaceChild = origining_runtime->GetName(restart_object, UNI_L("isReplaceChild"), &return_value);
		PUT_FAILED_IF_ERROR(isReplaceChild);

		DOMSetObject(value, restart_object);

		if (isReplaceChild == OpBoolean::IS_TRUE)
			result = DOM_Node::replaceChild(NULL, NULL, -1, value, origining_runtime);
		else
			result = DOM_Node::insertBefore(NULL, NULL, -1, value, origining_runtime);
	}

	if (result == (ES_SUSPEND | ES_RESTART))
		return PUT_SUSPEND;

	if (result == ES_NO_MEMORY)
		return PUT_NO_MEMORY;
	else
		/* FIXME-LATER: translate other return values? */
		return PUT_SUCCESS;
}

/* static */ OP_STATUS
DOM_HTMLElement::CreateElement(DOM_HTMLElement *&element, DOM_Node *reference, const uni_char *name)
{
	DOM_Document *document;
	HTML_Element *htmlelement;
	DOM_Node *node;

	if (reference->IsA(DOM_TYPE_DOCUMENT))
		document = (DOM_Document *) reference;
	else
		document = reference->GetOwnerDocument();

	RETURN_IF_ERROR(HTML_Element::DOMCreateElement(htmlelement, document->GetEnvironment(), name, NS_IDX_HTML, FALSE));

	OP_STATUS status;
	if (OpStatus::IsError(status = document->GetEnvironment()->ConstructNode(node, htmlelement, document)))
	{
		DELETE_HTML_Element(htmlelement);
		return status;
	}

	element = (DOM_HTMLElement *) node;
	return OpStatus::OK;
}

/* static */
OP_STATUS DOM_HTMLElement::BeforeClick(DOM_Object* object)
{
	if (object->IsA(DOM_TYPE_NODE))
		if (HTML_Element *this_element = static_cast<DOM_Node*>(object)->GetThisElement())
		{
			DOM_EnvironmentImpl *environment = object->GetEnvironment();
			InputType input_type = this_element->GetInputType();
			if (input_type == INPUT_CHECKBOX || input_type == INPUT_RADIO)
			{
				BOOL is_checked = this_element->DOMGetBoolFormValue(environment);
				FormValueRadioCheck* radiocheck = FormValueRadioCheck::GetAs(this_element->GetFormValue());
				radiocheck->SaveStateBeforeOnClick(this_element);
				if (input_type == INPUT_CHECKBOX || !is_checked)
					this_element->DOMSetBoolFormValue(environment, !is_checked);
			}
		}

	return OpStatus::OK;
}

#include "modules/dochand/fdelm.h"

ES_GetState
DOM_HTMLElement::GetFrameEnvironment(ES_Value* value, FrameEnvironmentType type, DOM_Runtime *origining_runtime)
{
	DOM_ProxyEnvironment* frame_environment;
	FramesDocument* frame_frames_doc;
	GET_FAILED_IF_ERROR(this_element->DOMGetFrameProxyEnvironment(frame_environment, frame_frames_doc, GetEnvironment()));
	DOM_Object *object = NULL;

	if (frame_environment)
	{
		OP_STATUS status;

		if (type == FRAME_DOCUMENT)
			status = static_cast<DOM_ProxyEnvironmentImpl *>(frame_environment)->GetProxyDocument(object, origining_runtime);
		else
			status = static_cast<DOM_ProxyEnvironmentImpl *>(frame_environment)->GetProxyWindow(object, origining_runtime);

		GET_FAILED_IF_ERROR(status);

		if (frame_frames_doc)
			origining_runtime->GetEnvironment()->AccessedOtherEnvironment(frame_frames_doc);
	}

	DOMSetObject(value, object);
	return GET_SUCCESS;
}

/* static */ OpAtom
DOM_HTMLElement::GetItemValueProperty(HTML_Element* this_element)
{
	OP_ASSERT(this_element);
	OP_ASSERT(this_element->GetNsType() == NS_HTML);

	switch (this_element->Type())
	{
	case HE_META:
		return OP_ATOM_content;
	case HE_AUDIO:
	case HE_EMBED:
	case HE_IFRAME:
	case HE_IMG:
	case HE_SOURCE:
	case Markup::HTE_TRACK:
	case HE_VIDEO:
		return OP_ATOM_src;
	case HE_A:
	case HE_AREA:
	case HE_LINK:
		return OP_ATOM_href;
	case HE_OBJECT:
		return OP_ATOM_data;
	case HE_TIME:
		if (this_element->HasAttr(ATTR_DATETIME))
			return OP_ATOM_dateTime;

	// fall through
	default:
		return OP_ATOM_textContent;
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_HTMLElement)
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLElement, DOM_HTMLElement::blur, "blur", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLElement, DOM_HTMLElement::focus, "focus", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLElement, DOM_HTMLElement::click, "click", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLElement, DOM_HTMLElement::insertAdjacentElement, "insertAdjacentElement", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLElement, DOM_HTMLElement::insertAdjacentHTML, "insertAdjacentHTML", "ss-")
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLElement, DOM_HTMLElement::insertAdjacentText, "insertAdjacentText", "ss-")
DOM_FUNCTIONS_END(DOM_HTMLElement)

