/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h" // sets (or doesn't set) DOM_XSLT_SUPPORT

#ifdef DOM_XSLT_SUPPORT

#include "modules/dom/src/opera/domxslt.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/attr.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/domxmldocument.h"
#include "modules/dom/src/domcore/docfrag.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/dom/src/domhtml/htmldoc.h"
#include "modules/dom/src/domxpath/xpathresult.h"
#include "modules/dom/src/domxpath/xpathevaluator.h"
#include "modules/dom/src/domxpath/xpathnamespace.h"
#include "modules/dom/src/domglobaldata.h"

#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmlserializer.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/xslt/xslt.h"
#include "modules/xpath/xpath.h"
#include "modules/logdoc/optreecallback.h"
#include "modules/security_manager/include/security_manager.h"

static BOOL DOM_XSLTAllowAccess(OpSecurityManager::Operation op, FramesDocument *doc, URL url)
{
	BOOL allowed = FALSE;
	return OpStatus::IsSuccess(g_secman_instance->CheckSecurity(op, doc, url, allowed)) && allowed;
}

class DOM_XSLTParseCallback
	: public DOM_Object,
	  public ES_ThreadListener,
	  public XSLT_StylesheetParser::Callback,
	  public XMLParser::Listener
{
private:
	class XMLSerializerCallbackImpl
		: public XMLSerializer::Callback
	{
	private:
		DOM_XSLTParseCallback *parent;

	public:
		XMLSerializerCallbackImpl(DOM_XSLTParseCallback *parent)
			: parent(parent)
		{
		}

		virtual void Stopped(XMLSerializer::Callback::Status status)
		{
			parent->Stopped(status);
		}
	} serializer_callback;

protected:
	DOM_XSLTProcessor *processor;
	ES_Thread *thread;
	BOOL finished, success, oom;
#ifdef XSLT_ERRORS
	OpString error_message;
#endif // XSLT_ERRORS

public:
	DOM_XSLTParseCallback(DOM_XSLTProcessor *processor, ES_Thread *thread)
		: serializer_callback(this),
		  processor(processor),
		  thread(thread),
		  finished(FALSE),
		  success(FALSE)
	{
		thread->AddListener(this);
		thread->Block();
	}

	~DOM_XSLTParseCallback()
	{
		Cleanup();
	}

	XMLSerializer::Callback *GetSerializerCallback()
	{
		return &serializer_callback;
	}

	void Cleanup()
	{
		if (thread)
		{
			ES_ThreadListener::Remove();
			OpStatus::Ignore(thread->Unblock());
			thread = NULL;
		}
		if (processor)
		{
			processor->parse_callback = NULL;
			processor = NULL;
		}
	}

	/* From ES_ThreadListener: */
	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal)
	{
		if (signal == ES_SIGNAL_FAILED || signal == ES_SIGNAL_CANCELLED)
		{
			ES_ThreadListener::Remove();
			thread = NULL;
		}
		return OpStatus::OK;
	}

	/* From XMLSerializer::Callback, via XMLSerializerCallbackImpl: */
	void Stopped(XMLSerializer::Callback::Status status)
	{
		finished = TRUE;
		success = status == XMLSerializer::Callback::STATUS_COMPLETED;
		oom = status == XMLSerializer::Callback::STATUS_OOM;

		if (thread)
		{
			ES_ThreadListener::Remove();
			OpStatus::Ignore(thread->Unblock());
			thread = NULL;
		}
	}

	/* From XSLT_StylesheetParser::Callback: */
	virtual OP_STATUS LoadOtherStylesheet(URL stylesheet_url, XMLTokenHandler *token_handler, BOOL is_import)
	{
		if (!thread)
			return OpStatus::ERR;

		FramesDocument *doc = thread->GetScheduler()->GetFramesDocument();

		if (!DOM_XSLTAllowAccess(OpSecurityManager::XSLT_IMPORT_OR_INCLUDE, doc, stylesheet_url))
			/* FIXME: Should send a console message saying why here. */
			return OpStatus::ERR;

		XMLParser *parser;

		RETURN_IF_ERROR(XMLParser::Make(parser, NULL, doc, token_handler, stylesheet_url));

		OP_STATUS status = parser->Load(doc->GetURL(), TRUE);

		if (OpStatus::IsError(status))
		{
			OP_DELETE(parser);
			return status;
		}

		return OpStatus::OK;
	}
#ifdef XSLT_ERRORS
	virtual OP_BOOLEAN HandleMessage(XSLT_StylesheetParser::Callback::MessageType type, const uni_char *message)
	{
		if (FramesDocument* frames_doc = GetFramesDocument())
			if (frames_doc->GetWindow()->GetPrivacyMode())
				return OpBoolean::IS_TRUE;  // claim we handle message, while we actually ignore it so that there will be no traces of it anywhere else

		if (type == XSLT_StylesheetParser::Callback::MESSAGE_TYPE_ERROR)
			RETURN_IF_ERROR(error_message.Set(message));
		return OpBoolean::IS_TRUE;
	}
#endif // XSLT_ERRORS

	/* From XMLParser::Listener: */
	virtual void Continue(XMLParser *parser)
	{
	}
	virtual void Stopped(XMLParser *parser)
	{
	}
	virtual BOOL Redirected(XMLParser *parser)
	{
		URL url = parser->GetURL().GetAttribute(URL::KMovedToURL, FALSE);
		while (!url.IsEmpty())
			if (!DOM_XSLTAllowAccess(OpSecurityManager::XSLT_IMPORT_OR_INCLUDE, thread->GetScheduler()->GetFramesDocument(), url))
				return FALSE;
			else
				url = url.GetAttribute(URL::KMovedToURL, FALSE);
		return TRUE;
	}

	BOOL IsFinished()
	{
		return finished;
	}

	OP_STATUS GetStatus()
	{
		return success ? OpStatus::OK : oom ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;
	}

	DOM_XSLTProcessor *GetProcessor()
	{
		return processor;
	}

	const uni_char *GetErrorMessage()
	{
#ifdef XSLT_ERRORS
		return error_message.IsEmpty() ? NULL : error_message.CStr();
#else // XSLT_ERRORS
		return NULL;
#endif // XSLT_ERRORS
	}
};

class DOM_XSLTTransformCallback
	: public DOM_Object,
	  public ES_ThreadListener,
	  public XSLT_Stylesheet::Transformation::Callback,
	  public XMLParser::Listener
{
protected:
	DOM_XSLTProcessor *processor;
	ES_Thread *thread;
#ifdef XSLT_ERRORS
	OpString error_message;
#endif // XSLT_ERRORS

	class DocumentElm
		: public Link
	{
	public:
		DocumentElm(XMLTokenHandler *token_handler)
			: parser(NULL),
			  token_handler(token_handler)
		{
		}

		~DocumentElm()
		{
			OP_DELETE(parser);
		}

		XMLParser *parser;
		XMLTokenHandler *token_handler;
	};

	Head document_elms;

public:
	DOM_XSLTTransformCallback(DOM_XSLTProcessor *processor)
		: processor(processor),
		  thread(NULL)
	{
	}

	~DOM_XSLTTransformCallback()
	{
		Cleanup();
	}

	void Cleanup()
	{
		if (thread)
		{
			ES_ThreadListener::Remove();
			OpStatus::Ignore(thread->Unblock());
			thread = NULL;
		}
		if (processor)
		{
			processor->transform_callback = NULL;
			processor = NULL;
		}

		document_elms.Clear();
	}

	/* From ES_ThreadListener: */
	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal)
	{
		if (signal == ES_SIGNAL_FAILED || signal == ES_SIGNAL_CANCELLED)
		{
			ES_ThreadListener::Remove();
			thread = NULL;
		}
		return OpStatus::OK;
	}

	/* From XSLT_Stylesheet::Transformation::Callback: */
	virtual void ContinueTransformation(XSLT_Stylesheet::Transformation *transformation)
	{
		if (thread)
		{
			OpStatus::Ignore(thread->Unblock());
			ES_ThreadListener::Remove();
			thread = NULL;
		}
	}
	virtual OP_STATUS LoadDocument(URL document_url, XMLTokenHandler *token_handler)
	{
		FramesDocument *doc = GetFramesDocument();

		if (!doc || !DOM_XSLTAllowAccess(OpSecurityManager::XSLT_DOCUMENT, doc, document_url))
			return OpStatus::ERR;

		DocumentElm *document_elm = OP_NEW(DocumentElm, (token_handler));
		if (!document_elm || OpStatus::IsMemoryError(XMLParser::Make(document_elm->parser, this, doc, token_handler, document_url)) || OpStatus::IsMemoryError(document_elm->parser->Load(doc->GetURL())))
		{
			OP_DELETE(document_elm);
			return OpStatus::ERR_NO_MEMORY;
		}

		document_elm->Into(&document_elms);
		return OpStatus::OK;
	}
	virtual void CancelLoadDocument(XMLTokenHandler *token_handler)
	{
		for (DocumentElm *document_elm = static_cast<DocumentElm *>(document_elms.First()); document_elm; document_elm = static_cast<DocumentElm *>(document_elm->Suc()))
			if (document_elm->token_handler == token_handler)
			{
				document_elm->Out();
				OP_DELETE(document_elm);
				return;
			}
	}
#ifdef XSLT_ERRORS
	virtual OP_BOOLEAN HandleMessage(XSLT_Stylesheet::Transformation::Callback::MessageType type, const uni_char *message)
	{
		if (FramesDocument* frames_doc = GetFramesDocument())
			if (frames_doc->GetWindow()->GetPrivacyMode())
				return OpBoolean::IS_TRUE;  // claim we handle message, while we actually ignore it so that there will be no traces of it anywhere else

		if (type == XSLT_Stylesheet::Transformation::Callback::MESSAGE_TYPE_ERROR)
			RETURN_IF_ERROR(error_message.Set(message));
		return OpBoolean::IS_TRUE;
	}
#endif // XSLT_ERRORS

	/* From XMLParser::Listener: */
	virtual void Continue(XMLParser *parser)
	{
	}
	virtual void Stopped(XMLParser *parser)
	{
	}
	virtual BOOL Redirected(XMLParser *parser)
	{
		if (FramesDocument *doc = GetFramesDocument())
		{
			URL url = parser->GetURL().GetAttribute(URL::KMovedToURL, FALSE);
			while (!url.IsEmpty())
				if (!DOM_XSLTAllowAccess(OpSecurityManager::XSLT_DOCUMENT, doc, url))
					return FALSE;
				else
					url = url.GetAttribute(URL::KMovedToURL, FALSE);
			return TRUE;
		}
		else
			return FALSE;
	}

	void BlockThread(ES_Thread *thread0)
	{
		thread = thread0;
		thread->AddListener(this);
		thread->Block();
	}

	const uni_char *GetErrorMessage()
	{
#ifdef XSLT_ERRORS
		return error_message.IsEmpty() ? NULL : error_message.CStr();
#else // XSLT_ERRORS
		return NULL;
#endif // XSLT_ERRORS
	}
};

class DOM_XSLTStringDataCollector
	: public XSLT_Stylesheet::Transformation::StringDataCollector
{
public:
	virtual OP_STATUS CollectStringData(const uni_char *string, unsigned string_length);

	const uni_char *GetResult() { return resulting_text.GetStorage() ? resulting_text.GetStorage() : UNI_L(""); }
	int GetResultLength() { return resulting_text.Length(); }
	virtual OP_STATUS StringDataFinished() { return OpStatus::OK; }

private:
	TempBuffer resulting_text;
};

/* virtual */ OP_STATUS
DOM_XSLTStringDataCollector::CollectStringData(const uni_char *string, unsigned string_length)
{
	return resulting_text.Append(string, string_length);
}

class DOM_XSLTTreeCallback
	: public OpTreeCallback
{
public:
	DOM_EnvironmentImpl *environment;
	HTML_Element *root;

	DOM_XSLTTreeCallback(DOM_EnvironmentImpl *environment)
		: environment(environment),
		  root(NULL)
	{
	}

	~DOM_XSLTTreeCallback()
	{
		if (root && root->Clean(environment))
			root->Free(environment);
	}

	virtual OP_STATUS ElementFound(HTML_Element *element) { root = element; return OpStatus::OK; }
	virtual OP_STATUS ElementNotFound() { OP_ASSERT(!"Should not be called since we don't load urls"); return OpStatus::OK; }
};

class DOM_XSLTTransform_State
	: public DOM_Object
{
public:
	class Tree
	{
	public:
		Tree(XMLTreeAccessor *tree, Tree *next)
			: tree(tree),
			  next(next)
		{
		}

		~Tree()
		{
			LogicalDocument::FreeXMLTreeAccessor(tree);
			OP_DELETE(next);
		}

		XMLTreeAccessor *tree;
		Tree *next;
	};

	DOM_XSLTProcessor *processor;
	DOM_Document *owner_document;
	DOM_Node *source_node;
	XSLT_Stylesheet::Transformation *transform_in_progress; // XXX FIXME Protect objects the transform_in_progress must not outlive (stylesheet...)
	XMLTreeAccessor *input_tree;
	Tree *trees;
	DOM_XSLTStringDataCollector *string_result;
	DOM_XSLTTreeCallback *xslt_tree_result;
	LogicalDocument *logdoc;
	HTML_Element *source_element;
	HTML_Element *cloned_tree;
	XSLT_Stylesheet::OutputSpecification::Method outputmethod;

	ES_Value return_value;

	DOM_XSLTTransform_State(DOM_XSLTProcessor *processor, DOM_Document *owner_document, DOM_Node *source_node, LogicalDocument *logdoc, HTML_Element *source_element)
		: processor(processor),
		  owner_document(owner_document),
		  source_node(source_node),
		  transform_in_progress(NULL),
		  input_tree(NULL),
		  trees(NULL),
		  string_result(NULL),
		  xslt_tree_result(NULL),
		  logdoc(logdoc),
		  source_element(source_element),
		  cloned_tree(NULL)
	{
	}

	virtual void GCTrace()
	{
		GCMark(owner_document);
		GCMark(source_node);
		GCMark(return_value);
	}

	~DOM_XSLTTransform_State()
	{
		Cleanup();

		OP_DELETE(trees);
		OP_DELETE(string_result);
		OP_DELETE(xslt_tree_result);
	}

	void Cleanup()
	{
		if (transform_in_progress)
		{
			XSLT_Stylesheet::StopTransformation(transform_in_progress);
			transform_in_progress = NULL;
		}

		if (cloned_tree && cloned_tree->Clean(GetLogicalDocument()))
		{
			cloned_tree->Free(GetLogicalDocument());
			cloned_tree = NULL;
		}

		if (processor)
		{
			processor->transform_state = NULL;
			processor = NULL;
		}
	}

	XMLTreeAccessor *FindTree(HTML_Element *root)
	{
		Tree *tree = trees;
		while (tree)
			if (tree->tree->GetRoot() == LogicalDocument::GetElementAsNode(tree->tree, root))
				return tree->tree;
			else
				tree = tree->next;
		return NULL;
	}

	OP_STATUS AddTree(XMLTreeAccessor *tree)
	{
		Tree *new_tree = OP_NEW(Tree, (tree, trees));
		if (!new_tree)
		{
			LogicalDocument::FreeXMLTreeAccessor(tree);
			return OpStatus::ERR_NO_MEMORY;
		}
		trees = new_tree;
		return OpStatus::OK;
	}
};

DOM_XSLTProcessor::DOM_XSLTProcessor()
	: stylesheetparser(NULL),
	  stylesheet(NULL),
	  version(XMLVERSION_1_0),
	  is_disabled(FALSE),
	  parse_callback(NULL),
	  transform_callback(NULL),
	  transform_state(NULL),
	  serializer(NULL),
	  parameter_values(NULL)
{
}

DOM_XSLTProcessor::~DOM_XSLTProcessor()
{
	Cleanup();
	GetEnvironment()->RemoveXSLTProcessor(this);
}

static HTML_Element *
DOM_FindRootElement(HTML_Element *element)
{
	while (HTML_Element *parent = element->Parent())
		element = parent;
	return element;
}

static HTML_Element *
DOM_FindRootElement(DOM_Node *node)
{
	HTML_Element *element;
	switch (node->GetNodeType())
	{
	case ATTRIBUTE_NODE:
		if (static_cast<DOM_Attr *>(node)->GetOwnerElement())
			element = static_cast<DOM_Attr *>(node)->GetOwnerElement()->GetThisElement();
		else
			return NULL;
		break;

	case XPATH_NAMESPACE_NODE:
		if (static_cast<DOM_XPathNamespace *>(node)->GetOwnerElement())
			element = static_cast<DOM_XPathNamespace *>(node)->GetOwnerElement()->GetThisElement();
		else
			return NULL;
		break;

	case DOCUMENT_NODE:
	case DOCUMENT_FRAGMENT_NODE:
		element = node->GetPlaceholderElement();
		break;

	default:
		element = node->GetThisElement();
	}
	return DOM_FindRootElement(element);
}

static OP_STATUS
DOM_MakeXMLTreeAccessor(XMLTreeAccessor *&tree, HTML_Element *rootelement, DOM_Node *node, DOM_XSLTTransform_State *state)
{
	if (rootelement)
	{
		tree = state->FindTree(rootelement);
		if (!tree)
		{
			URL url;
			if (node->GetNodeType() == DOCUMENT_NODE)
				url = static_cast<DOM_Document *>(node)->GetURL();
			else
				url = node->GetOwnerDocument()->GetURL();
			XMLTreeAccessor::Node *rootnode;
			RETURN_IF_ERROR(state->logdoc->CreateXMLTreeAccessor(tree, rootnode, rootelement, &url));
			RETURN_IF_ERROR(state->AddTree(tree));
		}
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

static OP_STATUS
DOM_MakeXPathNodeForXSLT(XPathNode *&xpathnode, DOM_Node *domnode, DOM_XSLTTransform_State *state)
{
	XMLTreeAccessor *tree;
	RETURN_IF_ERROR(DOM_MakeXMLTreeAccessor(tree, DOM_FindRootElement(domnode), domnode, state));
	return DOM_CreateXPathNode(xpathnode, domnode, tree);
}

static OP_STATUS
DOM_MakeXPathNodeForXSLT(XPathNode *&xpathnode, HTML_Element *element, DOM_Node *domnode, DOM_XSLTTransform_State *state)
{
	XMLTreeAccessor *tree;
	RETURN_IF_ERROR(DOM_MakeXMLTreeAccessor(tree, DOM_FindRootElement(element), domnode, state));
	return XPathNode::Make(xpathnode, tree, LogicalDocument::GetElementAsNode(tree, element));
}

OP_STATUS
DOM_XSLTProcessor::SetParameters(DOM_XSLTTransform_State *state, XSLT_Stylesheet::Input &input)
{
	input.parameters_count = 0;

	ParameterValue *parameter_value = parameter_values;
	while (parameter_value)
	{
		if (parameter_value->value.type != VALUE_UNDEFINED && parameter_value->value.type != VALUE_NULL)
			++input.parameters_count;
		parameter_value = parameter_value->next;
	}

	if (!(input.parameters = OP_NEWA(XSLT_Stylesheet::Input::Parameter, input.parameters_count)))
	{
		input.parameters_count = 0;
		return OpStatus::ERR_NO_MEMORY;
	}

	XSLT_Stylesheet::Input::Parameter *parameter = input.parameters;

	parameter_value = parameter_values;
	while (parameter_value)
	{
		if (parameter_value->value.type != VALUE_UNDEFINED && parameter_value->value.type != VALUE_NULL)
		{
			RETURN_IF_ERROR(parameter->name.Set(parameter_value->name));

			switch (parameter_value->value.type)
			{
			case VALUE_NUMBER:
				RETURN_IF_ERROR(XSLT_Stylesheet::Input::Parameter::Value::MakeNumber(parameter->value, parameter_value->value.value.number));
				break;

			case VALUE_STRING:
				RETURN_IF_ERROR(XSLT_Stylesheet::Input::Parameter::Value::MakeString(parameter->value, parameter_value->value.value.string));
				break;

			case VALUE_BOOLEAN:
				RETURN_IF_ERROR(XSLT_Stylesheet::Input::Parameter::Value::MakeBoolean(parameter->value, parameter_value->value.value.boolean));
				break;

			case VALUE_OBJECT:
				DOM_HOSTOBJECT_SAFE(domnode, parameter_value->value.value.object, DOM_TYPE_NODE, DOM_Node);
				if (domnode)
				{
					XPathNode *xpathnode;
					RETURN_IF_ERROR(DOM_MakeXPathNodeForXSLT(xpathnode, domnode, state));
					OP_STATUS status = XSLT_Stylesheet::Input::Parameter::Value::MakeNode(parameter->value, xpathnode);
					XPathNode::Free(xpathnode);
					RETURN_IF_ERROR(status);
					break;
				}
				DOM_HOSTOBJECT_SAFE(domnodelist, parameter_value->value.value.object, DOM_TYPE_COLLECTION, DOM_Collection);
				if (domnodelist)
				{
					RETURN_IF_ERROR(XSLT_Stylesheet::Input::Parameter::Value::MakeNodeList(parameter->value));
					for (unsigned index = 0, length = domnodelist->Length(); index < length; ++index)
					{
						HTML_Element *element = domnodelist->Item(index);
						XPathNode *xpathnode;
						RETURN_IF_ERROR(DOM_MakeXPathNodeForXSLT(xpathnode, element, domnodelist->GetRoot(), state));
						OP_STATUS status = parameter->value->AddNodeToList(xpathnode);
						XPathNode::Free(xpathnode);
						RETURN_IF_ERROR(status);
					}
					break;
				}
				return OpStatus::ERR;
			}

			++parameter;
		}

		parameter_value = parameter_value->next;
	}

	return OpStatus::OK;
}

void
DOM_XSLTProcessor::RemoveParameter(const XMLExpandedName &name)
{
	ParameterValue **pointer = &parameter_values;
	while (*pointer)
		if ((*pointer)->name == name)
		{
			ParameterValue *next = (*pointer)->next;
			DOMFreeValue((*pointer)->value);
			OP_DELETE(*pointer);
			*pointer = next;
			break;
		}
		else
			pointer = &(*pointer)->next;
}

void
DOM_XSLTProcessor::ClearParameters()
{
	ParameterValue *parameter_value = parameter_values;
	while (parameter_value)
	{
		ParameterValue *next = parameter_value->next;
		DOMFreeValue(parameter_value->value);
		OP_DELETE(parameter_value);
		parameter_value = next;
	}
	parameter_values = NULL;
}

/* static */ OP_STATUS
DOM_XSLTProcessor::Make(DOM_XSLTProcessor *&processor, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(processor = OP_NEW(DOM_XSLTProcessor, ()), runtime, runtime->GetPrototype(DOM_Runtime::XSLTPROCESSOR_PROTOTYPE), "XSLTProcessor"));
	environment->AddXSLTProcessor(processor);
	return OpStatus::OK;
}

/* virtual */ void
DOM_XSLTProcessor::GCTrace()
{
	GCMark(parse_callback);
	GCMark(transform_callback);

	ParameterValue *parameter_value = parameter_values;
	while (parameter_value)
	{
		GCMark(parameter_value->value);
		parameter_value = parameter_value->next;
	}
}

class DOM_XSLT_StylesheetParser_Callback
	: public XSLT_StylesheetParser::Callback
{
public:
	DOM_XSLT_StylesheetParser_Callback(DOM_Runtime *origining_runtime)
		: origining_runtime(origining_runtime)
	{
	}

private:
	DOM_Runtime *origining_runtime;
};


void
DOM_XSLTProcessor::Cleanup()
{
	is_disabled = TRUE;

	XSLT_StylesheetParser::Free(stylesheetparser);
	stylesheetparser = NULL;

	if (transform_state)
		transform_state->Cleanup();

	XSLT_Stylesheet::Free(stylesheet);
	stylesheet = NULL;

	if (parse_callback)
		parse_callback->Cleanup();

	if (transform_callback)
		transform_callback->Cleanup();

	OP_DELETE(serializer);
	serializer = NULL;

	ClearParameters();
}

/* static */ int
DOM_XSLTProcessor::importStylesheet(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_XSLTProcessor *processor = NULL;
	FramesDocument *doc = NULL;
	HTML_Element *root_element = NULL, *stylesheet_element = NULL;
	URL url;
	OP_STATUS status;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(processor, DOM_TYPE_XSLTPROCESSOR, DOM_XSLTProcessor);
		DOM_CHECK_ARGUMENTS("o");
		DOM_ARGUMENT_OBJECT(stylesheet_node, 0, DOM_TYPE_NODE, DOM_Node);

		if (processor->is_disabled)
			return DOM_CALL_INTERNALEXCEPTION(RESOURCE_UNAVAILABLE_ERR);

		if (processor->parse_callback || processor->stylesheetparser)
			return DOM_CALL_INTERNALEXCEPTION(RESOURCE_BUSY_ERR);

		DOM_Element *root_node = NULL;

		if (stylesheet_node->IsA(DOM_TYPE_DOCUMENT))
			stylesheet_node = root_node = ((DOM_Document *) stylesheet_node)->GetRootElement();
		else if (stylesheet_node->IsA(DOM_TYPE_ELEMENT))
			root_node = stylesheet_node->GetOwnerDocument()->GetRootElement();

		if (!root_node)
			return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

		const XMLDocumentInformation* docinfo = root_node->GetOwnerDocument()->GetXMLDocumentInfo();
		if (docinfo)
			processor->version = docinfo->GetVersion();

		root_element = root_node->GetThisElement();

		if (stylesheet_node->IsA(DOM_TYPE_ELEMENT))
			stylesheet_element = ((DOM_Element *) stylesheet_node)->GetThisElement();

		if (processor->stylesheet || !stylesheet_element)
			return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

		doc = processor->GetFramesDocument();
		if (!doc)
			return ES_FAILED;

		LogicalDocument *logdoc = doc->GetLogicalDocument();
		if (!logdoc)
			return ES_FAILED;

		CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(processor->parse_callback = OP_NEW(DOM_XSLTParseCallback, (processor, GetCurrentThread(origining_runtime))), processor->GetRuntime()));
		CALL_FAILED_IF_ERROR(XSLT_StylesheetParser::Make(processor->stylesheetparser, processor->parse_callback));

		url = stylesheet_node->GetOwnerDocument()->GetURL();

		if (OpStatus::IsSuccess(status = XMLLanguageParser::MakeSerializer(processor->serializer, processor->stylesheetparser)) &&
		    OpStatus::IsSuccess(status = processor->serializer->Serialize(doc->GetMessageHandler(), url, root_element, stylesheet_element, processor->parse_callback->GetSerializerCallback())) &&
		    !processor->parse_callback->IsFinished())
		{
			DOMSetObject(return_value, processor->parse_callback);
			return ES_SUSPEND | ES_RESTART;
		}
	}
	else
	{
		processor = DOM_VALUE2OBJECT(*return_value, DOM_XSLTParseCallback)->GetProcessor();

		if (!processor)
			return DOM_CALL_INTERNALEXCEPTION(RESOURCE_UNAVAILABLE_ERR);

		status = OpStatus::OK;
	}

	OP_ASSERT(OpStatus::IsError(status) || processor->parse_callback->IsFinished());

	int code = ES_FAILED;

	if (!OpStatus::IsMemoryError(status))
	{
		if (OpStatus::IsSuccess(status))
			status = processor->parse_callback->GetStatus();

		if (status == OpStatus::OK)
			status = processor->stylesheetparser->GetStylesheet(processor->stylesheet);

		if (status == OpStatus::ERR)
			code = this_object->CallInternalException(DOM_Object::XSLT_PARSING_FAILED_ERR, return_value, processor->parse_callback->GetErrorMessage());
	}

	XSLT_StylesheetParser::Free(processor->stylesheetparser);
	processor->stylesheetparser = NULL;

	OP_DELETE(processor->parse_callback);
	processor->parse_callback = NULL;

	OP_DELETE(processor->serializer);
	processor->serializer = NULL;

	if (OpStatus::IsMemoryError(status))
		return ES_NO_MEMORY;
	else
		return code;
}

static BOOL
DOM_SetXMLExpandedName(XMLExpandedName &name, ES_Value *argv, XMLVersion version)
{
	const uni_char *uri, *localpart;

	if (argv[0].type == VALUE_STRING && *argv[0].value.string)
		uri = argv[0].value.string;
	else
		uri = NULL;

	OP_ASSERT(argv[1].type == VALUE_STRING);
	localpart = argv[1].value.string;
	if (!XMLUtils::IsValidNCName(version, localpart))
		return FALSE;

	name = XMLExpandedName(uri, localpart);
	return TRUE;
}

/* static */ int
DOM_XSLTProcessor::getParameter(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(processor, DOM_TYPE_XSLTPROCESSOR, DOM_XSLTProcessor);
	DOM_CHECK_ARGUMENTS("Ss");

	XMLExpandedName name;
	if (!DOM_SetXMLExpandedName(name, argv, processor->version))
		return DOM_CALL_DOMEXCEPTION(INVALID_CHARACTER_ERR);

	DOMSetNull(return_value);

	ParameterValue *parameter_value = processor->parameter_values;
	while (parameter_value)
		if (parameter_value->name == name)
		{
			*return_value = parameter_value->value;
			break;
		}
		else
			parameter_value = parameter_value->next;

	return ES_VALUE;
}

/* static */ int
DOM_XSLTProcessor::setParameter(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(processor, DOM_TYPE_XSLTPROCESSOR, DOM_XSLTProcessor);
	DOM_CHECK_ARGUMENTS("Ss-");

	XMLExpandedName name;
	if (!DOM_SetXMLExpandedName(name, argv, processor->version))
		return DOM_CALL_DOMEXCEPTION(INVALID_CHARACTER_ERR);

	processor->RemoveParameter(name);

	ParameterValue *parameter_value = OP_NEW(ParameterValue, ());
	if (!parameter_value || OpStatus::IsMemoryError(parameter_value->name.Set(name)) || OpStatus::IsMemoryError(DOMCopyValue(parameter_value->value, argv[2])))
	{
		OP_DELETE(parameter_value);
		return ES_NO_MEMORY;
	}

	parameter_value->next = processor->parameter_values;
	processor->parameter_values = parameter_value;

	return ES_FAILED;
}

/* static */ int
DOM_XSLTProcessor::removeParameter(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(processor, DOM_TYPE_XSLTPROCESSOR, DOM_XSLTProcessor);
	DOM_CHECK_ARGUMENTS("Ss");

	XMLExpandedName name;
	if (!DOM_SetXMLExpandedName(name, argv, processor->version))
		return DOM_CALL_DOMEXCEPTION(INVALID_CHARACTER_ERR);

	processor->RemoveParameter(name);

	return ES_FAILED;
}

/* static */ int
DOM_XSLTProcessor::clearParameters(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(processor, DOM_TYPE_XSLTPROCESSOR, DOM_XSLTProcessor);

	processor->ClearParameters();

	return ES_FAILED;
}

/* static */ int
DOM_XSLTProcessor::reset(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(processor, DOM_TYPE_XSLTPROCESSOR, DOM_XSLTProcessor);

	processor->Cleanup();
	processor->is_disabled = FALSE;

	return ES_FAILED;
}

static OP_STATUS
SetXSLTTransformOutputHandler(XSLT_Stylesheet::Transformation *transform_in_progress, XSLT_Stylesheet::OutputForm outputform, DOM_EnvironmentImpl *environment, DOM_XSLTStringDataCollector *&string_result, DOM_XSLTTreeCallback *&callback, BOOL is_transform_to_fragment)
{
	if (outputform == XSLT_Stylesheet::OUTPUT_DELAYED_DECISION)
		return OpStatus::OK;

	if (outputform == XSLT_Stylesheet::OUTPUT_STRINGDATA)
	{
		OP_ASSERT(!string_result);
		string_result = OP_NEW(DOM_XSLTStringDataCollector, ());
		if (!string_result)
			return OpStatus::ERR_NO_MEMORY;
		transform_in_progress->SetStringDataCollector(string_result, FALSE);
	}
	else
	{
		callback = OP_NEW(DOM_XSLTTreeCallback, (environment));
		if (!callback)
			return OpStatus::ERR_NO_MEMORY;
		XMLTokenHandler *tokenhandler;
		OP_STATUS status = OpTreeCallback::MakeTokenHandler(tokenhandler, environment->GetFramesDocument()->GetLogicalDocument(), callback, UNI_L(""));
		if (OpStatus::IsError(status))
		{
			OP_DELETE(callback);
			callback = NULL;
			return status;
		}
		transform_in_progress->SetXMLTokenHandler(tokenhandler, TRUE);
		if (is_transform_to_fragment)
			transform_in_progress->SetXMLParseMode(XMLParser::PARSEMODE_FRAGMENT);
	}

	return OpStatus::OK;
}

static XSLT_Stylesheet::OutputForm
GetOutputForm(XSLT_Stylesheet::OutputSpecification::Method outputmethod)
{
	if (outputmethod == XSLT_Stylesheet::OutputSpecification::METHOD_XML)
		return XSLT_Stylesheet::OUTPUT_XMLTOKENHANDLER;

	if (outputmethod == XSLT_Stylesheet::OutputSpecification::METHOD_UNKNOWN)
		return XSLT_Stylesheet::OUTPUT_DELAYED_DECISION;

	return XSLT_Stylesheet::OUTPUT_STRINGDATA;
}

static HTML_Element *
DOM_XSLTFindInClonedTree(HTML_Element *cloned, HTML_Element *real)
{
	unsigned index = 0;

	HTML_Element *iter = real;
	while (HTML_Element *previous = iter->PrevActual())
	{
		++index;
		iter = previous;
	}
	iter = cloned;
	while (index != 0)
	{
		--index;
		iter = iter->NextActual();
	}

	return iter;
}

static OP_STATUS
DOM_XSLTCloneDocument(DOM_XSLTProcessor *processor, HTML_Element *&new_root_element, HTML_Element *&new_source_element, HTML_Element *root_element, HTML_Element *source_element)
{
	DOM_EnvironmentImpl *environment = processor->GetEnvironment();

	RETURN_IF_ERROR(HTML_Element::DOMCloneElement(new_root_element, environment, root_element, TRUE));
	new_source_element = DOM_XSLTFindInClonedTree(new_root_element, source_element);

	return OpStatus::OK;
}

/* static */ int
DOM_XSLTProcessor::transform(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_XSLTProcessor *processor;
	DOM_XSLTTransform_State *state = NULL;
	DOM_Document *owner_document;
	DOM_Node *source_node;
	HTML_Element *source_element = NULL;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(processor, DOM_TYPE_XSLTPROCESSOR, DOM_XSLTProcessor);

		if (processor->is_disabled || !processor->stylesheet)
			return DOM_CALL_INTERNALEXCEPTION(RESOURCE_UNAVAILABLE_ERR);

		if (data == 0)
		{
			DOM_CHECK_ARGUMENTS("o");
			owner_document = NULL;
		}
		else
		{
			DOM_CHECK_ARGUMENTS("oo");
			DOM_ARGUMENT_OBJECT_EXISTING(owner_document, 1, DOM_TYPE_DOCUMENT, DOM_Document);
		}
		DOM_ARGUMENT_OBJECT_EXISTING(source_node, 0, DOM_TYPE_NODE, DOM_Node);

		if (source_node->IsA(DOM_TYPE_DOCUMENT))
			source_element = source_node->GetPlaceholderElement();
		else if (source_node->IsA(DOM_TYPE_ELEMENT))
			source_element = source_node->GetThisElement();
		else
			return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	}
	else
	{
		// Continue the transform
		state = DOM_VALUE2OBJECT(*return_value, DOM_XSLTTransform_State);
		processor = state->processor;

		if (!processor || processor->is_disabled)
			return DOM_CALL_INTERNALEXCEPTION(RESOURCE_UNAVAILABLE_ERR);

		*return_value = state->return_value;
		owner_document = state->owner_document;
		source_node = state->source_node;
		source_element = state->source_element;
	}

	DOM_EnvironmentImpl *environment = processor->GetEnvironment();

	XSLT_Stylesheet::Transformation *transform_in_progress = NULL;
	OP_STATUS status = OpStatus::OK;
	XMLTreeAccessor* input_tree = NULL;
	DOM_XSLTStringDataCollector* string_result = NULL;
	DOM_XSLTTreeCallback* xslt_tree_result = NULL;
	XSLT_Stylesheet::OutputSpecification::Method outputmethod;

	if (state)
	{
		transform_in_progress = state->transform_in_progress;
		input_tree = state->input_tree;
		string_result = state->string_result;
		xslt_tree_result = state->xslt_tree_result;

		outputmethod = state->outputmethod;
	}
	else
	{
		state = OP_NEW(DOM_XSLTTransform_State, (processor, owner_document, source_node, processor->GetLogicalDocument(), source_element));
		CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state, processor->GetRuntime()));
		processor->transform_state = state;

		if (processor->stylesheet->ShouldStripWhitespaceIn(XMLExpandedName()))
		{
			HTML_Element *root = source_element;
			while (HTML_Element *parent = root->Parent())
				root = parent;
			HTML_Element *iter = root;
			while (iter)
			{
				if (Markup::IsRealElement(iter->Type()))
					if (processor->stylesheet->ShouldStripWhitespaceIn(XMLExpandedName(iter)))
					{
						/* Probably need whitespace stripping.  It's destructive, so we
						   clone the entire tree first. */

						CALL_FAILED_IF_ERROR(DOM_XSLTCloneDocument(processor, state->cloned_tree, source_element, root, source_element));

						state->source_element = source_element;

						state->cloned_tree->XSLTStripWhitespace(processor->GetLogicalDocument(), processor->stylesheet);
						break;
					}
				iter = iter->Next();
			}
		}

		const XSLT_Stylesheet::OutputSpecification *output_specification = processor->stylesheet->GetOutputSpecification();
		outputmethod = output_specification->method;
		XSLT_Stylesheet::Input transform_input;
		XSLT_Stylesheet::OutputForm outputform = GetOutputForm(outputmethod);
		status = DOM_MakeXMLTreeAccessor(transform_input.tree, DOM_FindRootElement(source_element), source_node, state);
		transform_input.node = LogicalDocument::GetElementAsNode(transform_input.tree, source_element);
		if (OpStatus::IsSuccess(status))
			if (OpStatus::IsSuccess(status = processor->SetParameters(state, transform_input)))
			{
				input_tree = transform_input.tree;
				if (OpStatus::IsSuccess(status = processor->stylesheet->StartTransformation(transform_in_progress, transform_input, outputform)))
				{
					if (data == 1)
					{
						/* Disable automatic HTML output detection for transformToFragment: instead
						   use HTML output if the script runs in na HTML document and XML output if
						   the script runs in an XML document.  This is what Firefox does, and
						   Firefox invented XSLTProcessor.  See also bug 298171 with comments. */

						BOOL is_html = !origining_runtime->GetEnvironment()->IsXML();
						transform_in_progress->SetDefaultOutputMethod(is_html ? XSLT_Stylesheet::OutputSpecification::METHOD_HTML : XSLT_Stylesheet::OutputSpecification::METHOD_XML);
					}

					if (OpStatus::IsSuccess(status = SetXSLTTransformOutputHandler(transform_in_progress, outputform, processor->GetEnvironment(), string_result, xslt_tree_result, data == 1)))
					{
						state->string_result = string_result;
						state->xslt_tree_result = xslt_tree_result;

						status = DOMSetObjectRuntime(processor->transform_callback = OP_NEW(DOM_XSLTTransformCallback, (processor)), processor->GetRuntime());
						if (OpStatus::IsError(status))
							processor->transform_callback = NULL;
						else
							transform_in_progress->SetCallback(processor->transform_callback);
					}
				}
			}
	}

	int return_code = ES_FAILED;

	if (OpStatus::IsSuccess(status))
	{
		OP_ASSERT(transform_in_progress);

		XSLT_Stylesheet::Transformation::Status transform_status;

		transform_status = transform_in_progress->Transform();

		if (transform_status == XSLT_Stylesheet::Transformation::TRANSFORM_NEEDS_OUTPUTHANDLER)
		{
			outputmethod = transform_in_progress->GetOutputMethod();

			XSLT_Stylesheet::OutputForm outputform = GetOutputForm(outputmethod);
			transform_in_progress->SetDelayedOutputForm(outputform);

			status = SetXSLTTransformOutputHandler(transform_in_progress, outputform, processor->GetEnvironment(), string_result, xslt_tree_result, data == 1);

			if (OpStatus::IsMemoryError(status))
				transform_status = XSLT_Stylesheet::Transformation::TRANSFORM_NO_MEMORY;
			else
			{
				state->string_result = string_result;
				state->xslt_tree_result = xslt_tree_result;

				transform_status = XSLT_Stylesheet::Transformation::TRANSFORM_PAUSED;
			}
		}

		if (transform_status == XSLT_Stylesheet::Transformation::TRANSFORM_PAUSED || transform_status == XSLT_Stylesheet::Transformation::TRANSFORM_BLOCKED)
		{
			state->return_value = *return_value;
			state->input_tree = input_tree;
			state->transform_in_progress = transform_in_progress;
			state->string_result = string_result;
			state->xslt_tree_result = xslt_tree_result;
			state->outputmethod = outputmethod;

			if (transform_status == XSLT_Stylesheet::Transformation::TRANSFORM_BLOCKED)
				processor->transform_callback->BlockThread(GetCurrentThread(origining_runtime));

			DOMSetObject(return_value, state);
			return ES_SUSPEND | ES_RESTART;
		}

		if (transform_status == XSLT_Stylesheet::Transformation::TRANSFORM_FAILED)
			return_code = this_object->CallInternalException(DOM_Object::XSLT_PROCESSING_FAILED_ERR, return_value, processor->transform_callback->GetErrorMessage());
		else if (transform_status != XSLT_Stylesheet::Transformation::TRANSFORM_FINISHED)
			status = OpStatus::ERR;

		OP_STATUS status2 = XSLT_Stylesheet::StopTransformation(transform_in_progress);

		state->transform_in_progress = transform_in_progress = NULL;

		processor->transform_callback->Cleanup();
		processor->transform_state->Cleanup();

		if (OpStatus::IsSuccess(status))
			status = status2;
	}

	if (OpStatus::IsMemoryError(status))
		return ES_NO_MEMORY;
	else if (return_code != ES_FAILED)
		return return_code;
	else if (OpStatus::IsError(status))
		return DOM_CALL_INTERNALEXCEPTION(XSLT_PROCESSING_FAILED_ERR);

	HTML_Element *root_element;

	if (outputmethod == XSLT_Stylesheet::OutputSpecification::METHOD_XML)
	{
		root_element = xslt_tree_result->root;
		xslt_tree_result->root = NULL;

		if (!root_element)
			status = HTML_Element::DOMCreateNullElement(root_element, environment);
	}
	else if (OpStatus::IsSuccess(status = HTML_Element::DOMCreateNullElement(root_element, environment)))
		if (outputmethod == XSLT_Stylesheet::OutputSpecification::METHOD_HTML)
		{
			HTML_Element *actual_parent_element;
			if (data == 0)
				actual_parent_element = root_element;
			else
				status = HTML_Element::DOMCreateElement(actual_parent_element, environment, UNI_L("DIV"), NS_IDX_HTML, FALSE);

			if (OpStatus::IsSuccess(status))
			{
				status = root_element->DOMSetInnerHTML(environment, string_result->GetResult(), string_result->GetResultLength(), actual_parent_element);

				if (data == 1)
					HTML_Element::DOMFreeElement(actual_parent_element, environment);
			}
		}
		else
		{
			HTML_Element *result_element;
			if (OpStatus::IsSuccess(status = HTML_Element::DOMCreateElement(result_element, environment, UNI_L("result"), NS_IDX_DEFAULT, TRUE)))
			{
				result_element->UnderSafe(environment, root_element);

				HTML_Element *text;
			    if (OpStatus::IsSuccess(status = HTML_Element::DOMCreateTextNode(text, environment, string_result->GetResult(), FALSE, FALSE)))
					text->UnderSafe(environment, result_element);
			}
		}

	if (data == 0 && OpStatus::IsSuccess(status))
		if (outputmethod == XSLT_Stylesheet::OutputSpecification::METHOD_HTML)
		{
			DOM_HTMLDOMImplementation *html_implementation = NULL;
			DOM_DOMImplementation *implementation = source_node->GetOwnerDocument()->GetDOMImplementation();
			if (implementation->IsA(DOM_TYPE_HTML_IMPLEMENTATION))
				html_implementation = static_cast<DOM_HTMLDOMImplementation *>(implementation);
			if (html_implementation || OpStatus::IsSuccess(status = DOM_HTMLDOMImplementation::Make(html_implementation, processor->GetEnvironment())))
			{
				DOM_HTMLDocument *html_owner_document;
				status = DOM_HTMLDocument::Make(html_owner_document, html_implementation, FALSE, FALSE);
				owner_document = html_owner_document;
			}
		}
		else
			status = DOM_XMLDocument::Make(owner_document, source_node->GetOwnerDocument()->GetDOMImplementation(), FALSE);

	if (OpStatus::IsError(status))
	{
		if (root_element)
			HTML_Element::DOMFreeElement(root_element, environment);

		return ES_NO_MEMORY;
	}

	if (data == 0)
	{
		/* Can't fail unless the argument is NULL. */
		OpStatus::Ignore(owner_document->ResetPlaceholderElement(root_element));

		HTML_Element *root = root_element->FirstChildActual();
		while (root)
			if (Markup::IsRealElement(root->Type()))
			{
				DOM_Node *node;

				CALL_FAILED_IF_ERROR(environment->ConstructNode(node, root, owner_document));
				CALL_FAILED_IF_ERROR(owner_document->SetRootElement((DOM_Element *) node));

				root = NULL;
			}
			else
				root = root->SucActual();

		DOMSetObject(return_value, owner_document);
	}
	else
	{
		DOM_DocumentFragment *fragment;
		CALL_FAILED_IF_ERROR(DOM_DocumentFragment::Make(fragment, owner_document, root_element));
		DOMSetObject(return_value, fragment);
	}

	return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_XSLTProcessor)
	DOM_FUNCTIONS_FUNCTION(DOM_XSLTProcessor, DOM_XSLTProcessor::importStylesheet, "importStylesheet", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_XSLTProcessor, DOM_XSLTProcessor::getParameter, "getParameter", "Ss-")
	DOM_FUNCTIONS_FUNCTION(DOM_XSLTProcessor, DOM_XSLTProcessor::setParameter, "setParameter", "Ss-")
	DOM_FUNCTIONS_FUNCTION(DOM_XSLTProcessor, DOM_XSLTProcessor::removeParameter, "removeParameter", "Ss-")
	DOM_FUNCTIONS_FUNCTION(DOM_XSLTProcessor, DOM_XSLTProcessor::clearParameters, "clearParameters", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_XSLTProcessor, DOM_XSLTProcessor::reset, "reset", 0)
DOM_FUNCTIONS_END(DOM_XSLTProcessor)

DOM_FUNCTIONS_WITH_DATA_START(DOM_XSLTProcessor)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_XSLTProcessor, DOM_XSLTProcessor::transform, 0, "transformToDocument", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_XSLTProcessor, DOM_XSLTProcessor::transform, 1, "transformToFragment", 0)
DOM_FUNCTIONS_WITH_DATA_END(DOM_XSLTProcessor)

DOM_XSLTProcessor_Constructor::DOM_XSLTProcessor_Constructor()
	: DOM_BuiltInConstructor(DOM_Runtime::XSLTPROCESSOR_PROTOTYPE)
{
}

/* virtual */ int
DOM_XSLTProcessor_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_XSLTProcessor *xsltprocessor;
	CALL_FAILED_IF_ERROR(DOM_XSLTProcessor::Make(xsltprocessor, GetEnvironment()));

	DOMSetObject(return_value, xsltprocessor);
	return ES_VALUE;
}
#endif // DOM_XSLT_SUPPORT
