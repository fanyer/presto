/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _DOM_DOCUMENT_
#define _DOM_DOCUMENT_

#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/doctype.h"
#include "modules/dom/src/domcore/implem.h"
#include "modules/dom/src/domcore/element.h"

#include "modules/ecmascript/ecmascript.h"

#include "modules/logdoc/namespace.h"
#include "modules/logdoc/html.h"

#include "modules/url/url2.h"

class DOM_Document
	: public DOM_Node
{
protected:
	friend class DOM_EnvironmentImpl;

	DOM_Document(DOM_DOMImplementation *implementation, BOOL is_xml);
	~DOM_Document();

	URL url;
	HTML_Element *placeholder;
	DOM_Element *root;

	BOOL is_xml;
	XMLDocumentInformation *xml_document_info;

	DOM_DOMImplementation *implementation;
	/**< Shared between all documents in an environment. */

public:
	static OP_STATUS Make(DOM_Document *&document, DOM_DOMImplementation *implementation, BOOL create_placeholder = FALSE, BOOL is_xml = FALSE);

	/** Initialize the "Document" prototype. */
	static void ConstructDocumentObjectL(ES_Object *object, DOM_Runtime *runtime);

	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_DOCUMENT || DOM_Node::IsA(type); }

	virtual void GCTraceSpecial(BOOL via_tree);

	URL GetURL();
	void SetURL(URL newurl) { url = newurl; }

	ES_GetState GetDocumentURI(ES_Value *value);

	HTML_Element *GetPlaceholderElement() { return placeholder; }
	OP_STATUS ResetPlaceholderElement(HTML_Element *new_placeholder = NULL);
	void ClearPlaceholderElement();

	void SetDocRootElement(HTML_Element* root);
	/**< Set the HE_DOC_ROOT for a document that didn't have one before. */

	DOM_Element *GetRootElement() { return root; }
	virtual OP_STATUS SetRootElement(DOM_Element *new_root);

	OP_STATUS GetDocumentType(DOM_Node *&doctype);

	DOM_DOMImplementation *GetDOMImplementation() { return implementation; }
	LogicalDocument *GetLogicalDocument();

	int GetDefaultNS();
	BOOL IsMainDocument();

#ifdef DOM_FULLSCREEN_MODE
	OP_STATUS RequestFullscreen(DOM_Element *element, DOM_Runtime *origining_runtime);
#endif // DOM_FULLSCREEN_MODE

	static OP_STATUS GetProxy(DOM_Object *&document, ES_Runtime *origining_runtime);

	BOOL IsXML() { return is_xml; }

	const XMLDocumentInformation *GetXMLDocumentInfo() { return xml_document_info; }
	OP_STATUS SetXMLDocumentInfo(const XMLDocumentInformation *document_info);
	OP_STATUS UpdateXMLDocumentInfo();

	DOM_Object::DOMException CheckName(const uni_char *name, BOOL is_qname, unsigned length = UINT_MAX);

	/** This method gets string corresponding Document.readyState as specified in 
	 *  http://www.whatwg.org/specs/web-apps/current-work/multipage/dom.html#current-document-readiness
	 *
	 * In addition to what is written in the spec it will result in "uninitialized"
	 * if document argument is NULL.
	 *
	 * @param value[out] If not NULL then the result string will be assigned to it.
	 * @param document FramesDocument which 'readiness' is being queried.
	 */
	static void GetDocumentReadiness(ES_Value* value, FramesDocument* document);

	DOM_DECLARE_FUNCTION(createDocumentFragment);
	DOM_DECLARE_FUNCTION(createEvent);
	DOM_DECLARE_FUNCTION(importNode);
	DOM_DECLARE_FUNCTION(adoptNode);
	DOM_DECLARE_FUNCTION(elementFromPoint);
	DOM_DECLARE_FUNCTION(createProcessingInstruction);

#ifndef HAS_NOTEXTSELECTION
	DOM_DECLARE_FUNCTION(getSelection);
#endif // HAS_NOTEXTSELECTION

#ifdef DOM_FULLSCREEN_MODE
	DOM_DECLARE_FUNCTION(exitFullscreen);
#endif // DOM_FULLSCREEN_MODE

#ifdef DOM2_RANGE
	DOM_DECLARE_FUNCTION(createRange);
#endif // DOM2_RANGE

	enum {
		FUNCTIONS_BASIC = 6,
#ifndef HAS_NOTEXTSELECTION
		FUNCTIONS_getSelection,
#endif // HAS_NOTEXTSELECTION
#ifdef DOM_FULLSCREEN_MODE
		FUNCTIONS_exitFullscreen,
#endif // DOM_FULLSCREEN_MODE
#ifdef DOM2_RANGE
		FUNCTIONS_createRange,
#endif // DOM2_RANGE
#ifdef DOM3_XPATH
		FUNCTIONS_createNSResolver,
#endif // DOM3_XPATH
#ifdef DOM_XPATH_REFERENCE_IMPLEMENTATION
		FUNCTIONS_evaluate0,
#endif // DOM_XPATH_REFERENCE_IMPLEMENTATION
		FUNCTIONS_ARRAY_SIZE
	};

	DOM_DECLARE_FUNCTION_WITH_DATA(getElementById);
	DOM_DECLARE_FUNCTION_WITH_DATA(createNode); // createElement{,NS} and createAttribute{,NS}
	DOM_DECLARE_FUNCTION_WITH_DATA(createCharacterData); // create{TextNode,Comment,CDATASection}
	DOM_DECLARE_FUNCTION_WITH_DATA(getElementsByClassName); // Also used by DOM_Element.
	// Also: getElementsByTagName{,NS}

#ifdef DOM2_TRAVERSAL
	DOM_DECLARE_FUNCTION_WITH_DATA(createTraversalObject); // create{NodeIterator,TreeWalker}
#endif // DOM2_TRAVERSAL
#ifdef DOCUMENT_EDIT_SUPPORT
	DOM_DECLARE_FUNCTION_WITH_DATA(documentEdit); // execCommand, queryCommand{Enabled,State,Supported,Value}
#endif // DOCUMENT_EDIT_SUPPORT

	enum {
		FUNCTIONS_WITH_DATA_BASIC = 11,
#ifdef DOM2_TRAVERSAL
		FUNCTIONS_createNodeIterator,
		FUNCTIONS_createTreeWalker,
#endif // DOM2_RANGE
#ifdef DOCUMENT_EDIT_SUPPORT
		FUNCTIONS_execCommand,
		FUNCTIONS_queryCommandEnabled,
		FUNCTIONS_queryCommandState,
		FUNCTIONS_queryCommandSupported,
		FUNCTIONS_queryCommandValue,
#endif // DOCUMENT_EDIT_SUPPORT
#ifdef DOM3_XPATH
		FUNCTIONS_createExpression,
		FUNCTIONS_evaluate,
#endif // DOM3_XPATH
#ifdef DOM_SELECTORS_API
		FUNCTIONS_querySelector,
		FUNCTIONS_querySelectorAll,
#endif // DOM_SELECTORS_API
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};
private:
	void MoveAllToThisDocument(HTML_Element *start, DOM_EnvironmentImpl *from);
};

#endif /* _DOM_DOCUMENT_ */
