/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/attr.h"
#include "modules/dom/src/domxpath/xpathnamespace.h"

#ifdef DOM_XPATH_REFERENCE_IMPLEMENTATION

#include "modules/logdoc/htm_elm.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/adt/opvector.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlutils.h"

class DOM_XPathRef_Token
{
public:
	enum Type { TYPE_PUNCTUATOR, TYPE_OPERATOR, TYPE_AXIS, TYPE_NODETYPETEST, TYPE_NAMETEST, TYPE_PITARGETTEST, TYPE_FUNCTION, TYPE_STRING, TYPE_NUMBER };

	DOM_XPathRef_Token(Type type, const uni_char *value_start = NULL, const uni_char *value_end = NULL)
		: type(type)
	{
		if (value_start)
			OpStatus::Ignore(value.Set(value_start, value_end ? static_cast<int>(value_end - value_start) : static_cast<int>(KAll)));
	}

	BOOL operator== (Type type0) { return type == type0; }
	BOOL operator!= (Type type0) { return type != type0; }
	BOOL operator== (const char *value0) { return value.Compare(value0) == 0; }
	BOOL operator!= (const char *value0) { return value.Compare(value0) != 0; }

	Type type;
	OpString value;
};

enum DOM_XPathRef_Production { DOM_XPATHREF_P_OR, DOM_XPATHREF_P_AND, DOM_XPATHREF_P_EQUALITY,
							   DOM_XPATHREF_P_RELATIONAL, DOM_XPATHREF_P_ADDITIVE,
							   DOM_XPATHREF_P_MULTIPLICATIVE, DOM_XPATHREF_P_UNARY,
							   DOM_XPATHREF_P_PRIMARY };

class DOM_XPathRef_Expression;

static DOM_XPathRef_Expression *DOM_XPathRef_ParseExpression(const OpVector<DOM_XPathRef_Token> &tokens, unsigned &index, DOM_XPathRef_Production production = DOM_XPATHREF_P_OR);

class DOM_XPathRef_NodeSet
{
public:
	DOM_XPathRef_NodeSet()
		: ref_count(0),
		  document_order(TRUE),
		  reverse_document_order(TRUE)
	{
	}

	BOOL Add(DOM_Node *node)
	{
		if (hashed.Add(node) == OpStatus::OK)
		{
			if (indexed.GetCount() != 0)
			{
				document_order = document_order && node->document_order_index > indexed.Get(indexed.GetCount() - 1)->document_order_index;
				reverse_document_order = reverse_document_order && node->document_order_index < indexed.Get(indexed.GetCount() - 1)->document_order_index;
			}

			OpStatus::Ignore(indexed.Add(node));
			return TRUE;
		}
		else
			return FALSE;
	}

	BOOL Contains(DOM_Node *node)
	{
		return hashed.Contains(node);
	}

	DOM_Node *GetInDocumentOrder(unsigned index = 1)
	{
		if (!document_order)
			if (reverse_document_order)
				return indexed.Get(indexed.GetCount() - index);
			else
			{
				MergeSort(indexed, 0, indexed.GetCount());

				document_order = TRUE;
				reverse_document_order = FALSE;
			}

		return indexed.Get(index - 1);
	}

	DOM_Node *GetInDefaultOrder(unsigned index)
	{
		OP_ASSERT(document_order || reverse_document_order);
		return indexed.Get(index - 1);
	}

	DOM_Node *GetInAnyOrder(unsigned index)
	{
		return indexed.Get(index - 1);
	}

	unsigned GetCount()
	{
		return indexed.GetCount();
	}

	BOOL IsEmpty()
	{
		return indexed.GetCount() == 0;
	}

	static DOM_XPathRef_NodeSet *IncRef(DOM_XPathRef_NodeSet *nodeset)
	{
		if (nodeset)
			++nodeset->ref_count;
		return nodeset;
	}

	static void DecRef(DOM_XPathRef_NodeSet *nodeset)
	{
		if (nodeset && --nodeset->ref_count == 0)
			OP_DELETE(nodeset);
	}

private:
	static void MergeSort(OpVector<DOM_Node> &nodes, unsigned offset, unsigned count)
	{
		if (count < 2)
			return;
		else if (count == 2)
		{
			if (nodes.Get(offset)->document_order_index > nodes.Get(offset + 1)->document_order_index)
			{
				DOM_Node *node = nodes.Get(offset);
				nodes.Replace(offset, nodes.Get(offset + 1));
				nodes.Replace(offset + 1, node);
			}
		}
		else
		{
			OpVector<DOM_Node> lower;
			unsigned index;

			for (index = 0; index < (count + 1) / 2; ++index)
				OpStatus::Ignore(lower.Add(nodes.Get(offset + index)));

			MergeSort(lower, 0, lower.GetCount());
			MergeSort(nodes, offset + lower.GetCount(), count - lower.GetCount());

			unsigned left_index = 0, right_index = 0;

			for (index = 0; index < count;)
			{
				DOM_Node *left = lower.Get(left_index);
				DOM_Node *right = nodes.Get(offset + lower.GetCount() + right_index);

				if (left->document_order_index < right->document_order_index)
				{
					nodes.Replace(offset + index++, left);
					if (++left_index == lower.GetCount())
						break;
				}
				else
				{
					nodes.Replace(offset + index++, right);
					if (++right_index == count - lower.GetCount())
						break;
				}
			}

			while (left_index < lower.GetCount())
				nodes.Replace(offset + index++, lower.Get(left_index++));

			while (right_index < (count - lower.GetCount()))
				nodes.Replace(offset + index++, nodes.Get(offset + lower.GetCount() + right_index++));
		}
	}

	unsigned ref_count;
	BOOL document_order, reverse_document_order;
	OpVector<DOM_Node> indexed;
	OpPointerSet<DOM_Node> hashed;
};

class DOM_XPathRef_NodeSetObject
	: public DOM_Object
{
public:
	DOM_XPathRef_NodeSetObject(DOM_XPathRef_NodeSet *nodeset, DOM_Runtime *runtime)
		: nodeset(DOM_XPathRef_NodeSet::IncRef(nodeset)),
		  found(FALSE)
	{
		OpStatus::Ignore(DOMSetObjectRuntime(this, runtime, runtime->GetArrayPrototype(), "Array"));
	}

	virtual ~DOM_XPathRef_NodeSetObject()
	{
		DOM_XPathRef_NodeSet::DecRef(nodeset);
	}

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
	{
		if (property_name == OP_ATOM_length)
		{
			DOMSetNumber(value, nodeset->GetCount());
			return GET_SUCCESS;
		}
		else if (property_name == OP_ATOM_accept)
		{
			DOMSetBoolean(value, found);
			return GET_SUCCESS;
		}
		else
			return GET_FAILED;
	}

	virtual ES_GetState	GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
	{
		if (property_index >= 0 && static_cast<unsigned>(property_index) < nodeset->GetCount())
		{
			DOMSetObject(value, nodeset->GetInDocumentOrder(property_index + 1));
			return GET_SUCCESS;
		}
		else
			return GET_FAILED;
	}

	virtual ES_PutState PutName(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
	{
		if (!value || value->type != VALUE_OBJECT)
			return PUT_FAILED;
		else
		{
			found = nodeset->Contains(DOM_VALUE2OBJECT(*value, DOM_Node));
			return PUT_SUCCESS;
		}
	}

	virtual void GCTrace()
	{
		for (unsigned index = 1; index <= nodeset->GetCount(); ++index)
			GCMark(nodeset->GetInAnyOrder(index));
	}

private:
	DOM_XPathRef_NodeSet *nodeset;
	BOOL found;
};

class DOM_XPathRef_Value
{
public:
	enum Type { TYPE_INVALID, TYPE_NUMBER, TYPE_STRING, TYPE_BOOLEAN, TYPE_NODESET };

	DOM_XPathRef_Value()
		: type(TYPE_INVALID)
	{
	}

	DOM_XPathRef_Value(double number_value)
		: type(TYPE_NUMBER),
		  number_value(number_value)
	{
	}

	DOM_XPathRef_Value(const uni_char *string)
		: type(TYPE_STRING)
	{
		OpStatus::Ignore(string_value.Set(string ? string : UNI_L("")));
	}

	DOM_XPathRef_Value(const char *string)
		: type(TYPE_STRING)
	{
		OpStatus::Ignore(string_value.Set(string ? string : ""));
	}

	DOM_XPathRef_Value(BOOL boolean_value)
		: type(TYPE_BOOLEAN),
		  boolean_value(boolean_value)
	{
	}

	DOM_XPathRef_Value(DOM_XPathRef_NodeSet *nodeset_value)
		: type(TYPE_NODESET),
		  nodeset_value(DOM_XPathRef_NodeSet::IncRef(nodeset_value))
	{
	}

	DOM_XPathRef_Value(const DOM_XPathRef_Value &other)
		: type(other.type)
	{
		switch (type)
		{
		case TYPE_NUMBER:
			number_value = other.number_value;
			break;

		case TYPE_STRING:
			OpStatus::Ignore(string_value.Set(other.string_value.CStr()));
			break;

		case TYPE_BOOLEAN:
			boolean_value = other.boolean_value;
			break;

		case TYPE_NODESET:
			nodeset_value = DOM_XPathRef_NodeSet::IncRef(other.nodeset_value);
			break;
		}
	}

	~DOM_XPathRef_Value()
	{
		if (type == TYPE_NODESET)
			DOM_XPathRef_NodeSet::DecRef(nodeset_value);
	}

	DOM_XPathRef_Value &operator= (const DOM_XPathRef_Value &other)
	{
		if (this != &other && (type != other.type || type != TYPE_NODESET || nodeset_value != other.nodeset_value))
		{
			if (type == TYPE_NODESET)
				DOM_XPathRef_NodeSet::DecRef(nodeset_value);

			type = other.type;

			switch (type)
			{
			case TYPE_NUMBER:
				number_value = other.number_value;
				break;

			case TYPE_STRING:
				OpStatus::Ignore(string_value.Set(other.string_value.CStr()));
				break;

			case TYPE_BOOLEAN:
				boolean_value = other.boolean_value;
				break;

			case TYPE_NODESET:
				nodeset_value = DOM_XPathRef_NodeSet::IncRef(other.nodeset_value);
				break;
			}
		}

		return *this;
	}

	DOM_XPathRef_Value AsNumber() const;
	DOM_XPathRef_Value AsString() const;
	DOM_XPathRef_Value AsBoolean() const;

	Type type;
	double number_value;
	OpString string_value;
	BOOL boolean_value;
	DOM_XPathRef_NodeSet *nodeset_value;
};

static void
DOM_XPathRef_StringValue(TempBuffer &buffer, DOM_Node *node)
{
	HTML_Element *iter = node->GetThisElement(), *stop = iter->NextSiblingActual();

	while (iter != stop)
	{
		if (iter->IsText())
			iter->DOMGetContents(node->GetEnvironment(), &buffer);

		iter = iter->NextActual();
	}
}

static DOM_XPathRef_Value
DOM_XPathRef_StringValue(DOM_Node *node)
{
	TempBuffer result;

	if (node)
		switch (node->GetNodeType())
		{
		case ELEMENT_NODE:
		case DOCUMENT_NODE:
		case DOCUMENT_FRAGMENT_NODE:
		case TEXT_NODE:
		case CDATA_SECTION_NODE:
			DOM_XPathRef_StringValue(result, node);
			break;

		case COMMENT_NODE:
		case PROCESSING_INSTRUCTION_NODE:
			OpStatus::Ignore(node->GetThisElement()->DOMGetContents(node->GetEnvironment(), &result));
			break;

		case ATTRIBUTE_NODE:
			return DOM_XPathRef_Value(static_cast<DOM_Attr *>(node)->GetValue());
		}

	return DOM_XPathRef_Value(result.GetStorage());
}

DOM_XPathRef_Value
DOM_XPathRef_Value::AsNumber() const
{
	switch (type)
	{
	case TYPE_NUMBER:
		return *this;

	case TYPE_STRING:
		uni_char *endptr;
		return DOM_XPathRef_Value (uni_strtod(string_value.CStr(), &endptr));

	case TYPE_BOOLEAN:
		return DOM_XPathRef_Value (boolean_value ? 1.0 : 0.0);

	default:
		return AsString().AsNumber();
	}
}

DOM_XPathRef_Value
DOM_XPathRef_Value::AsString() const
{
	OpString string;
	switch (type)
	{
	case TYPE_NUMBER:
		{
			TempBuffer buf;
			buf.AppendDoubleToPrecision(number_value);
			return DOM_XPathRef_Value(buf.GetStorage());
		}

	case TYPE_STRING:
		return *this;

	case TYPE_BOOLEAN:
		return DOM_XPathRef_Value(boolean_value ? UNI_L("true") : UNI_L("false"));

	default:
		return nodeset_value->IsEmpty() ? DOM_XPathRef_Value(UNI_L("")) : DOM_XPathRef_StringValue(nodeset_value->GetInDocumentOrder());
	}
}

DOM_XPathRef_Value
DOM_XPathRef_Value::AsBoolean() const
{
	switch (type)
	{
	case TYPE_NUMBER:
		return DOM_XPathRef_Value(number_value != 0.0 ? 1 : 0);

	case TYPE_STRING:
		return DOM_XPathRef_Value(!string_value.IsEmpty() ? 1 : 0);

	case TYPE_BOOLEAN:
		return *this;

	default:
		return DOM_XPathRef_Value(!nodeset_value->IsEmpty() ? 1 : 0);
	}
}

class DOM_XPathRef_Expression
{
public:
	virtual ~DOM_XPathRef_Expression() {}
	virtual DOM_XPathRef_Value Evaluate(DOM_Node *node, double position, double size) = 0;
};

class DOM_XPathRef_FunctionCall
	: public DOM_XPathRef_Expression
{
public:
	enum Function { F_POSITION, F_LAST, F_NUMBER, F_BOOLEAN, F_STRING, F_COUNT, F_ID,
					F_LOCAL_NAME, F_NAMESPACE_URI, F_NAME, F_CONCAT, F_STARTS_WITH,
					F_CONTAINS, F_SUBSTRING_BEFORE, F_SUBSTRING_AFTER, F_SUBSTRING,
					F_STRING_LENGTH, F_NORMALIZE_SPACE, F_TRUE, F_FALSE, F_NOT,
					F_ROUND, F_FLOOR, F_CEILING, F_SUM, F_TRANSLATE, F_LANG, F_INVALID };

	static Function FunctionByName(const OpString &string)
	{
		if (string == "position")
			return F_POSITION;
		else if (string == "last")
			return F_LAST;
		else if (string == "number")
			return F_NUMBER;
		else if (string == "boolean")
			return F_BOOLEAN;
		else if (string == "string")
			return F_STRING;
		else if (string == "count")
			return F_COUNT;
		else if (string == "id")
			return F_ID;
		else if (string == "local-name")
			return F_LOCAL_NAME;
		else if (string == "namespace-uri")
			return F_NAMESPACE_URI;
		else if (string == "name")
			return F_NAME;
		else if (string == "concat")
			return F_CONCAT;
		else if (string == "starts-with")
			return F_STARTS_WITH;
		else if (string == "contains")
			return F_CONTAINS;
		else if (string == "substring-before")
			return F_SUBSTRING_BEFORE;
		else if (string == "substring-after")
			return F_SUBSTRING_AFTER;
		else if (string == "substring")
			return F_SUBSTRING;
		else if (string == "string-length")
			return F_STRING_LENGTH;
		else if (string == "normalize-space")
			return F_NORMALIZE_SPACE;
		else if (string == "true")
			return F_TRUE;
		else if (string == "false")
			return F_FALSE;
		else if (string == "not")
			return F_NOT;
		else if (string == "round")
			return F_ROUND;
		else if (string == "floor")
			return F_FLOOR;
		else if (string == "ceiling")
			return F_CEILING;
		else if (string == "sum")
			return F_SUM;
		else if (string == "translate")
			return F_TRANSLATE;
		else if (string == "lang")
			return F_LANG;
		else
		{
			OP_ASSERT(!"Invalid function name!");
			return F_INVALID;
		}
	}

	DOM_XPathRef_FunctionCall(Function function, OpAutoVector<DOM_XPathRef_Expression> *arguments)
		: function(function),
		  arguments(arguments)
	{
	}

	~DOM_XPathRef_FunctionCall()
	{
		OP_DELETE(arguments);
	}

	virtual DOM_XPathRef_Value Evaluate(DOM_Node *node, double position, double size)
	{
		DOM_XPathRef_Value *values = OP_NEWA(DOM_XPathRef_Value, arguments->GetCount());
		ANCHOR_ARRAY(DOM_XPathRef_Value, values);

		for (unsigned index = 0; index < arguments->GetCount(); ++index)
			values[index] = arguments->Get(index)->Evaluate(node, position, size);

		DOM_Node *default_node = node;

		if (arguments->GetCount() == 1 && values[0].type == DOM_XPathRef_Value::TYPE_NODESET)
			default_node = values[0].nodeset_value->GetInDocumentOrder (1);

		switch (function)
		{
		case F_POSITION:
			return DOM_XPathRef_Value(position);

		case F_LAST:
			return DOM_XPathRef_Value(size);

		case F_NUMBER:
			if (arguments->GetCount() == 1)
				return values[0].AsNumber();
			else
				return DOM_XPathRef_StringValue(node).AsNumber();

		case F_BOOLEAN:
			return values[0].AsBoolean();

		case F_STRING:
			if (arguments->GetCount() == 1)
				return values[0].AsString();
			else
				return DOM_XPathRef_StringValue(node);

		case F_COUNT:
			return DOM_XPathRef_Value(static_cast<double>(values[0].nodeset_value->GetCount()));

		case F_LOCAL_NAME:
			if (default_node)
			{
				switch (default_node->GetNodeType())
				{
				case ELEMENT_NODE:
					return DOM_XPathRef_Value(default_node->GetThisElement()->GetTagName());

				case ATTRIBUTE_NODE:
					return DOM_XPathRef_Value(static_cast<DOM_Attr *>(default_node)->GetName());

				case XPATH_NAMESPACE_NODE:
					return DOM_XPathRef_Value(static_cast<DOM_XPathNamespace *>(default_node)->GetNsPrefix());
				}
			}
			return DOM_XPathRef_Value(UNI_L(""));

		case F_NAMESPACE_URI:
			if (default_node)
			{
				switch (default_node->GetNodeType())
				{
				case ELEMENT_NODE:
					if (default_node->GetThisElement()->GetNsIdx() == NS_IDX_DEFAULT)
						return DOM_XPathRef_Value(UNI_L(""));
					else
						return DOM_XPathRef_Value(g_ns_manager->GetElementAt(default_node->GetThisElement()->GetNsIdx())->GetUri());

				case ATTRIBUTE_NODE:
					return DOM_XPathRef_Value(static_cast<DOM_Attr *>(default_node)->GetNsUri());

				case XPATH_NAMESPACE_NODE:
					return DOM_XPathRef_Value(UNI_L(""));
				}
			}
			return DOM_XPathRef_Value(UNI_L(""));

		case F_NAME:
			if (default_node)
			{
				const uni_char *local_name, *prefix;

				switch (default_node->GetNodeType())
				{
				case ELEMENT_NODE:
					local_name = default_node->GetThisElement()->GetTagName();
					prefix = g_ns_manager->GetElementAt(default_node->GetThisElement()->GetNsIdx())->GetPrefix();
					break;

				case ATTRIBUTE_NODE:
					local_name = static_cast<DOM_Attr *>(default_node)->GetName();
					prefix = static_cast<DOM_Attr *>(default_node)->GetNsPrefix();
					break;

				case XPATH_NAMESPACE_NODE:
					prefix = NULL;
					local_name = static_cast<DOM_XPathNamespace *>(default_node)->GetNsPrefix();
					break;

				default:
					return DOM_XPathRef_Value(UNI_L(""));
				}

				OpString name;

				if (prefix && *prefix)
					name.AppendFormat(UNI_L("%s:%s"), prefix, local_name);
				else
					name.Append(local_name);

				return DOM_XPathRef_Value(name.CStr());
			}
			return DOM_XPathRef_Value(UNI_L(""));

		case F_CONCAT:
		{
			OpString result;
			for (unsigned index = 0; index < arguments->GetCount(); ++index)
				result.Append(values[index].AsString().string_value.CStr());
			return DOM_XPathRef_Value(result.CStr());
		}

		case F_STARTS_WITH:
			return DOM_XPathRef_Value(values[0].AsString().string_value.Find(values[1].AsString().string_value) == 0 ? 1 : 0);

		case F_CONTAINS:
			return DOM_XPathRef_Value(values[0].AsString().string_value.Find(values[1].AsString().string_value) != KNotFound ? 1 : 0);

		case F_STRING_LENGTH:
			if (arguments->GetCount() == 1)
				return DOM_XPathRef_Value(static_cast<double>(values[0].AsString().string_value.Length()));
			else
				return DOM_XPathRef_Value(static_cast<double>(DOM_XPathRef_StringValue(node).string_value.Length()));

		case F_TRUE:
			return DOM_XPathRef_Value(TRUE);

		case F_FALSE:
			return DOM_XPathRef_Value(FALSE);

		case F_NOT:
			return DOM_XPathRef_Value(!values[0].AsBoolean().boolean_value ? 1 : 0);

		case F_SUBSTRING_BEFORE:
		case F_SUBSTRING_AFTER:
		case F_SUBSTRING:
		case F_NORMALIZE_SPACE:
		case F_TRANSLATE:
			OP_ASSERT(!"FIXME: Implement remaining functions.");
			return DOM_XPathRef_Value(UNI_L(""));

		case F_ROUND:
		case F_FLOOR:
		case F_CEILING:
		case F_SUM:
			return DOM_XPathRef_Value(0.0);

		case F_LANG:
			return DOM_XPathRef_Value(FALSE);

		case F_ID:
		default:
			return DOM_XPathRef_Value(OP_NEW(DOM_XPathRef_NodeSet, ()));
		}
	}

private:
	Function function;
	OpAutoVector<DOM_XPathRef_Expression> *arguments;
};

static void
DOM_XPathRef_InitializeDocument(DOM_Document *document)
{
	if (document->document_order_index == ~0u)
	{
		unsigned index = document->document_order_index = 0;

		DOM_EnvironmentImpl *environment = document->GetEnvironment();
		HTML_Element *iter = document->GetPlaceholderElement();

		while (iter)
		{
			if (!HTML_Element::IsRealElement(iter->Type()))
				switch (iter->Type())
				{
				case HE_TEXT:
				case HE_TEXTGROUP:
				case HE_PROCINST:
				case HE_COMMENT:
					break;

				default:
					goto skip_element;
				}

			DOM_Node *node;

			OpStatus::Ignore(environment->ConstructNode(node, iter, document));

			node->SetIsSignificant();
			node->document_order_index = ++index;

			if (node->GetNodeType() == ELEMENT_NODE)
			{
				XMLNamespaceDeclaration::Reference nsdeclaration;

				OpStatus::Ignore(XMLNamespaceDeclaration::Push(nsdeclaration, UNI_L("http://www.w3.org/XML/1998/namespace"), 36, UNI_L("xml"), 3, 0));
				OpStatus::Ignore(XMLNamespaceDeclaration::PushAllInScope(nsdeclaration, iter));

				XMLNamespaceDeclaration *default_decl = XMLNamespaceDeclaration::FindDefaultDeclaration(nsdeclaration);
				if (default_decl && default_decl->GetUri())
				{
					DOM_XPathNamespace *namespace_node;

					OpStatus::Ignore(static_cast<DOM_Element *>(node)->GetXPathNamespaceNode(namespace_node, NULL, default_decl->GetUri()));

					namespace_node->SetIsSignificant();
					namespace_node->document_order_index = ++index;
				}

				for (unsigned namespace_prefix_index = 0, namespaces_prefixes_count = XMLNamespaceDeclaration::CountDeclaredPrefixes(nsdeclaration); namespace_prefix_index < namespaces_prefixes_count; ++namespace_prefix_index)
				{
					XMLNamespaceDeclaration *decl = XMLNamespaceDeclaration::FindDeclarationByIndex(nsdeclaration, namespace_prefix_index);
					DOM_XPathNamespace *namespace_node;

					OpStatus::Ignore(static_cast<DOM_Element *>(node)->GetXPathNamespaceNode(namespace_node, decl->GetPrefix(), decl->GetUri()));

					namespace_node->SetIsSignificant();
					namespace_node->document_order_index = ++index;
				}

				ES_Value attributes_value;

				node->GetName(OP_ATOM_attributes, &attributes_value, node->GetRuntime());

				DOM_NamedNodeMap *map = DOM_VALUE2OBJECT(attributes_value, DOM_NamedNodeMap);

				ES_Value attributes_count_value;

				map->GetName(OP_ATOM_length, &attributes_count_value, map->GetRuntime());

				unsigned attributes_count = static_cast<unsigned>(attributes_count_value.value.number);

				for (unsigned attribute_index = 0; attribute_index < attributes_count; ++attribute_index)
				{
					ES_Value attribute_value;

					map->GetIndex(attribute_index, &attribute_value, map->GetRuntime());

					DOM_Attr *attribute_node = DOM_VALUE2OBJECT(attribute_value, DOM_Attr);

					if (attribute_node->GetNsUri() && uni_strcmp(attribute_node->GetNsUri(), "http://www.w3.org/2000/xmlns/") == 0)
						continue;

					attribute_node->SetIsSignificant();
					attribute_node->document_order_index = ++index;
				}
			}

			if (iter->IsText())
			{
				HTML_Element* suc = iter->SucActual();
				while (suc && suc->IsText())
				{
					iter = suc;
					suc = iter->SucActual();
				}
			}

		skip_element:
			iter = iter->NextActual();
		}
	}
}

class DOM_XPathRef_LocationPath
	: public DOM_XPathRef_Expression
{
public:
	class Step
	{
	public:
		enum Axis { A_ANCESTOR, A_ANCESTOR_OR_SELF, A_ATTRIBUTE, A_CHILD, A_DESCENDANT,
					A_DESCENDANT_OR_SELF, A_FOLLOWING, A_FOLLOWING_SIBLING, A_NAMESPACE,
					A_PARENT, A_PRECEDING, A_PRECEDING_SIBLING, A_SELF };
		enum NodeTestType { NTT_NONE, NTT_NODETYPE, NTT_NAME, NTT_PITARGET };

		static Axis AxisByName(const OpString &name)
		{
			if (name == "ancestor")
				return A_ANCESTOR;
			else if (name == "ancestor-or-self")
				return A_ANCESTOR_OR_SELF;
			else if (name == "attribute")
				return A_ATTRIBUTE;
			else if (name == "child")
				return A_CHILD;
			else if (name == "descendant")
				return A_DESCENDANT;
			else if (name == "descendant-or-self")
				return A_DESCENDANT_OR_SELF;
			else if (name == "following")
				return A_FOLLOWING;
			else if (name == "following-sibling")
				return A_FOLLOWING_SIBLING;
			else if (name == "namespace")
				return A_NAMESPACE;
			else if (name == "parent")
				return A_PARENT;
			else if (name == "preceding")
				return A_PRECEDING;
			else if (name == "preceding-sibling")
				return A_PRECEDING_SIBLING;
			else
			{
				OP_ASSERT(name == "self");
				return A_SELF;
			}
		}

		static DOM_NodeType NodeTypeByName(const OpString &name)
		{
			if (name == "node")
				return NOT_A_NODE;
			else if (name == "text")
				return TEXT_NODE;
			else if (name == "comment")
				return COMMENT_NODE;
			else
			{
				OP_ASSERT(name == "processing-instruction");
				return PROCESSING_INSTRUCTION_NODE;
			}
		}

		Axis axis;
		NodeTestType nodetesttype;
		DOM_NodeType nodetype;
		OpString namespace_uri;
		OpString local_part;
		OpString pitarget;
		OpAutoVector<DOM_XPathRef_Expression> predicates;

		BOOL MatchNodeTest(DOM_Node *node)
		{
			if (node->document_order_index == ~0u)
				return FALSE;
			else if (nodetesttype != NTT_NONE)
			{
				DOM_NodeType actual_nodetype = node->GetNodeType();

				if (actual_nodetype == CDATA_SECTION_NODE)
					actual_nodetype = TEXT_NODE;

				if (nodetype != actual_nodetype)
					return FALSE;

				if (nodetesttype == NTT_NAME)
				{
					if (!namespace_uri.IsEmpty())
						switch (actual_nodetype)
						{
						case ELEMENT_NODE:
							if (node->GetThisElement()->GetNsIdx() == NS_IDX_DEFAULT || uni_strcmp(g_ns_manager->GetElementAt(node->GetThisElement()->GetNsIdx())->GetUri(), namespace_uri.CStr()) != 0)
								return FALSE;
							break;

						case ATTRIBUTE_NODE:
							if (!static_cast<DOM_Attr *>(node)->GetNsUri())
								return FALSE;
							else if (uni_strcmp(static_cast<DOM_Attr *>(node)->GetNsUri(), namespace_uri.CStr()) != 0)
								return FALSE;
							break;

						case XPATH_NAMESPACE_NODE:
							return FALSE;
						}
					else
						switch (actual_nodetype)
						{
						case ELEMENT_NODE:
							if (node->GetThisElement()->GetNsIdx() != NS_IDX_DEFAULT)
								return FALSE;
							break;

						case ATTRIBUTE_NODE:
							if (static_cast<DOM_Attr *>(node)->GetNsUri())
								return FALSE;
							break;
						}

					if (local_part.IsEmpty())
						return TRUE;
					else
						switch (actual_nodetype)
						{
						case ELEMENT_NODE:
							return uni_strcmp(node->GetThisElement()->GetTagName(), local_part.CStr()) == 0;

						case ATTRIBUTE_NODE:
							return uni_strcmp(static_cast<DOM_Attr *>(node)->GetName(), local_part.CStr()) == 0;

						case XPATH_NAMESPACE_NODE:
							if (static_cast<DOM_XPathNamespace *>(node)->GetNsPrefix())
								return uni_strcmp(static_cast<DOM_XPathNamespace *>(node)->GetNsPrefix(), local_part.CStr()) == 0;
							else
								return FALSE;
						}
				}
				else if (nodetesttype == NTT_PITARGET)
					return uni_strcmp(node->GetThisElement()->DOMGetPITarget(node->GetEnvironment()), pitarget.CStr()) == 0;
			}
			return TRUE;
		}

		void Evaluate(DOM_Node *node, DOM_XPathRef_NodeSet *result)
		{
			DOM_XPathRef_NodeSet *nodeset = predicates.GetCount() != 0 ? OP_NEW(DOM_XPathRef_NodeSet, ()): result;

			if ((axis == A_ANCESTOR_OR_SELF || axis == A_DESCENDANT_OR_SELF || axis == A_SELF) && MatchNodeTest(node))
				nodeset->Add(node);

			DOM_Node *stop;

			switch (axis)
			{
			case A_ANCESTOR:
			case A_ANCESTOR_OR_SELF:
				switch (node->GetNodeType())
				{
				case ATTRIBUTE_NODE:
					node = static_cast<DOM_Attr *>(node)->GetOwnerElement();
					break;

				case XPATH_NAMESPACE_NODE:
					node = static_cast<DOM_XPathNamespace *>(node)->GetOwnerElement();
					break;

				default:
					OpStatus::Ignore(node->GetParentNode(node));
				}
				while (node)
				{
					if (MatchNodeTest(node))
						nodeset->Add(node);
					OpStatus::Ignore(node->GetParentNode(node));
				}
				break;

			case A_ATTRIBUTE:
				if (node->GetNodeType() == ELEMENT_NODE)
				{
					ES_Value attributes_value;

					node->GetName(OP_ATOM_attributes, &attributes_value, node->GetRuntime());

					DOM_NamedNodeMap *map = DOM_VALUE2OBJECT(attributes_value, DOM_NamedNodeMap);

					ES_Value attributes_count_value;

					map->GetName(OP_ATOM_length, &attributes_count_value, map->GetRuntime());

					unsigned attributes_count = static_cast<unsigned>(attributes_count_value.value.number);

					for (unsigned attribute_index = 0; attribute_index < attributes_count; ++attribute_index)
					{
						ES_Value attribute_value;

						map->GetIndex(attribute_index, &attribute_value, map->GetRuntime());

						DOM_Attr *attribute_node = DOM_VALUE2OBJECT(attribute_value, DOM_Attr);

						if (MatchNodeTest(attribute_node))
							nodeset->Add(attribute_node);
					}
				}
				break;

			case A_CHILD:
				OpStatus::Ignore(node->GetFirstChild(node));
				while (node)
				{
					if (MatchNodeTest(node))
						nodeset->Add(node);
					OpStatus::Ignore(node->GetNextSibling(node));
				}
				break;

			case A_DESCENDANT:
			case A_DESCENDANT_OR_SELF:
				OpStatus::Ignore(node->GetNextNode(stop, TRUE));
				OpStatus::Ignore(node->GetNextNode(node, FALSE));
				while (node != stop)
				{
					if (MatchNodeTest(node))
						nodeset->Add(node);
					OpStatus::Ignore(node->GetNextNode(node, FALSE));
				}
				break;

			case A_FOLLOWING:
				switch (node->GetNodeType())
				{
				case ATTRIBUTE_NODE:
					OpStatus::Ignore(static_cast<DOM_Attr *>(node)->GetOwnerElement()->GetNextNode(node, FALSE));
					break;

				case XPATH_NAMESPACE_NODE:
					OpStatus::Ignore(static_cast<DOM_XPathNamespace *>(node)->GetOwnerElement()->GetNextNode(node, FALSE));
					break;

				default:
					OpStatus::Ignore(node->GetNextNode(node, TRUE));
				}
				while (node)
				{
					if (MatchNodeTest(node))
						nodeset->Add(node);
					OpStatus::Ignore(node->GetNextNode(node, FALSE));
				}
				break;

			case A_FOLLOWING_SIBLING:
				OpStatus::Ignore(node->GetNextSibling(node));
				while (node)
				{
					if (MatchNodeTest(node))
						nodeset->Add(node);
					OpStatus::Ignore(node->GetNextSibling(node));
				}
				break;

			case A_NAMESPACE:
				if (node->GetNodeType() == ELEMENT_NODE)
				{
					XMLNamespaceDeclaration::Reference nsdeclaration;

					OpStatus::Ignore(XMLNamespaceDeclaration::Push(nsdeclaration, UNI_L("http://www.w3.org/XML/1998/namespace"), 36, UNI_L("xml"), 3, 0));
					OpStatus::Ignore(XMLNamespaceDeclaration::PushAllInScope(nsdeclaration, node->GetThisElement()));

					XMLNamespaceDeclaration *default_decl = XMLNamespaceDeclaration::FindDefaultDeclaration(nsdeclaration);
					if (default_decl && default_decl->GetUri())
					{
						DOM_XPathNamespace *namespace_node;

						OpStatus::Ignore(static_cast<DOM_Element *>(node)->GetXPathNamespaceNode(namespace_node, NULL, default_decl->GetUri()));

						if (MatchNodeTest(namespace_node))
							nodeset->Add(namespace_node);
					}

					for (unsigned namespace_prefix_index = 0, namespaces_prefixes_count = XMLNamespaceDeclaration::CountDeclaredPrefixes(nsdeclaration); namespace_prefix_index < namespaces_prefixes_count; ++namespace_prefix_index)
					{
						XMLNamespaceDeclaration *decl = XMLNamespaceDeclaration::FindDeclarationByIndex(nsdeclaration, namespace_prefix_index);
						DOM_XPathNamespace *namespace_node;

						OpStatus::Ignore(static_cast<DOM_Element *>(node)->GetXPathNamespaceNode(namespace_node, decl->GetPrefix(), decl->GetUri()));

						if (MatchNodeTest(namespace_node))
							nodeset->Add(namespace_node);
					}
				}
				break;

			case A_PARENT:
				switch (node->GetNodeType())
				{
				case ATTRIBUTE_NODE:
					node = static_cast<DOM_Attr *>(node)->GetOwnerElement();
					break;

				case XPATH_NAMESPACE_NODE:
					node = static_cast<DOM_XPathNamespace *>(node)->GetOwnerElement();
					break;

				default:
					OpStatus::Ignore(node->GetParentNode(node));
				}
				if (node && MatchNodeTest(node))
					nodeset->Add(node);
				break;

			case A_PRECEDING:
				switch (node->GetNodeType())
				{
				case ATTRIBUTE_NODE:
					node = static_cast<DOM_Attr *>(node)->GetOwnerElement();
					break;

				case XPATH_NAMESPACE_NODE:
					node = static_cast<DOM_XPathNamespace *>(node)->GetOwnerElement();
					break;
				}
				OpStatus::Ignore(node->GetParentNode(stop));
				OpStatus::Ignore(node->GetPreviousNode(node));
				while (node)
				{
					if (node == stop)
						OpStatus::Ignore(stop->GetParentNode(stop));
					else if (MatchNodeTest(node))
						nodeset->Add(node);
					OpStatus::Ignore(node->GetPreviousNode(node));
				}
				break;

			case A_PRECEDING_SIBLING:
				OpStatus::Ignore(node->GetPreviousSibling(node));
				while (node)
				{
					if (MatchNodeTest(node))
						nodeset->Add(node);
					OpStatus::Ignore(node->GetPreviousSibling(node));
				}
				break;
			}

			if (predicates.GetCount() != 0)
			{
				DOM_XPathRef_NodeSet *unfiltered = nodeset;

				for (unsigned predicate_index = 0; predicate_index < predicates.GetCount(); ++predicate_index)
				{
					DOM_XPathRef_NodeSet *filtered = predicate_index == predicates.GetCount() - 1 ? result : OP_NEW(DOM_XPathRef_NodeSet, ());

					for (unsigned position = 1; position <= unfiltered->GetCount(); ++position)
					{
						DOM_Node *unfiltered_node = unfiltered->GetInDefaultOrder(position);
						DOM_XPathRef_Value value(predicates.Get(predicate_index)->Evaluate(unfiltered_node, static_cast<double>(position), static_cast<double>(unfiltered->GetCount())));
						BOOL keep;

						if (value.type == DOM_XPathRef_Value::TYPE_NUMBER)
							keep = value.number_value == static_cast<double>(position);
						else
							keep = value.AsBoolean().boolean_value;

						if (keep)
							filtered->Add(unfiltered_node);
					}

					OP_DELETE(unfiltered);
					unfiltered = filtered;
				}
			}
		}
	};

	DOM_XPathRef_LocationPath(DOM_XPathRef_Expression *base, const OpVector<DOM_XPathRef_Token> &tokens, unsigned &index)
		: base(base)
	{
		if (*tokens.Get(index) == "/")
		{
			absolute = TRUE;
			++index;
		}
		else
			absolute = FALSE;

		if (index < tokens.GetCount() && *tokens.Get(index) == DOM_XPathRef_Token::TYPE_AXIS)
			do
			{
				OP_ASSERT(*tokens.Get(index) == DOM_XPathRef_Token::TYPE_AXIS);

				Step *step = OP_NEW(Step, ());

				OpStatus::Ignore(steps.Add(step));

				step->axis = Step::AxisByName(tokens.Get(index++)->value);

				if (*tokens.Get(index) == DOM_XPathRef_Token::TYPE_NODETYPETEST)
				{
					step->nodetesttype = Step::NTT_NODETYPE;
					step->nodetype = Step::NodeTypeByName(tokens.Get(index++)->value);

					if (step->nodetype == NOT_A_NODE)
						step->nodetesttype = Step::NTT_NONE;
				}
				else if (*tokens.Get(index) == DOM_XPathRef_Token::TYPE_NAMETEST)
				{
					if (tokens.Get(index)->value == "*")
						step->nodetesttype = Step::NTT_NODETYPE;
					else
					{
						step->nodetesttype = Step::NTT_NAME;

						XMLCompleteNameN qname(tokens.Get(index)->value.CStr(), tokens.Get(index)->value.Length());

						if (qname.GetPrefix())
							OpStatus::Ignore(step->namespace_uri.Set(qname.GetPrefix(), qname.GetPrefixLength()));

						if (qname.GetLocalPartLength() > 0 && (qname.GetLocalPartLength() != 1 || qname.GetLocalPart()[0] != '*'))
							OpStatus::Ignore(step->local_part.Set(qname.GetLocalPart(), qname.GetLocalPartLength()));
					}

					if (step->axis == Step::A_ATTRIBUTE)
						step->nodetype = ATTRIBUTE_NODE;
					else if (step->axis == Step::A_NAMESPACE)
						step->nodetype = XPATH_NAMESPACE_NODE;
					else
						step->nodetype = ELEMENT_NODE;

					++index;
				}
				else if (*tokens.Get(index) == DOM_XPathRef_Token::TYPE_PITARGETTEST)
				{
					step->nodetesttype = Step::NTT_PITARGET;
					step->nodetype = PROCESSING_INSTRUCTION_NODE;
					OpStatus::Ignore(step->pitarget.Set(tokens.Get(index++)->value.CStr()));
				}

				while (index < tokens.GetCount() && *tokens.Get(index) == "[")
				{
					++index;
					step->predicates.Add(DOM_XPathRef_ParseExpression(tokens, index));
					OP_ASSERT(*tokens.Get(index) == "]");
					++index;
				}
			}
			while (index < tokens.GetCount() && *tokens.Get(index) == "/" && ++index);
	}

	virtual ~DOM_XPathRef_LocationPath()
	{
		OP_DELETE(base);
	}

	virtual DOM_XPathRef_Value Evaluate(DOM_Node *node, double position, double size)
	{
		DOM_XPathRef_NodeSet *nodeset = NULL;
		DOM_XPathRef_Value base_value;

		if (base)
		{
			base_value = base->Evaluate(node, position, size);
			nodeset = base_value.nodeset_value;
		}
		else
		{
			if (absolute)
			{
				DOM_Node *parent;
				OpStatus::Ignore(node->GetParentNode(parent));
				while (parent)
				{
					node = parent;
					OpStatus::Ignore(node->GetParentNode(parent));
				}
			}
			nodeset = OP_NEW(DOM_XPathRef_NodeSet, ());
			nodeset->Add(node);
		}

		for (unsigned step_index = 0; step_index < steps.GetCount(); ++step_index)
		{
			DOM_XPathRef_NodeSet::IncRef(nodeset);

			DOM_XPathRef_NodeSet *result = OP_NEW(DOM_XPathRef_NodeSet, ());
			Step *step = steps.Get(step_index);

			for (unsigned document_position = 1; document_position <= nodeset->GetCount(); ++document_position)
				step->Evaluate(nodeset->GetInDocumentOrder(document_position), result);

			DOM_XPathRef_NodeSet::DecRef(nodeset);
			nodeset = result;
		}

		return DOM_XPathRef_Value(nodeset);
	}

private:
	DOM_XPathRef_Expression *base;
	BOOL absolute;
	OpAutoVector<Step> steps;
};

class DOM_XPathRef_Or
	: public DOM_XPathRef_Expression
{
public:
	DOM_XPathRef_Or(DOM_XPathRef_Expression *lhs, DOM_XPathRef_Expression *rhs)
		: lhs(lhs),
		  rhs(rhs)
	{
	}

	virtual ~DOM_XPathRef_Or()
	{
		OP_DELETE(lhs);
		OP_DELETE(rhs);
	}

	virtual DOM_XPathRef_Value Evaluate(DOM_Node *context, double position, double size)
	{
		if (lhs->Evaluate(context, position, size).AsBoolean().boolean_value ||
			rhs->Evaluate(context, position, size).AsBoolean().boolean_value)
			return TRUE;
		// ADS 1.2 compiler crashes if you unify these two return statements (using ? : or naked boolean expression)
		return FALSE;
	}

private:
	DOM_XPathRef_Expression *lhs, *rhs;
};

class DOM_XPathRef_And
	: public DOM_XPathRef_Expression
{
public:
	DOM_XPathRef_And(DOM_XPathRef_Expression *lhs, DOM_XPathRef_Expression *rhs)
		: lhs(lhs),
		  rhs(rhs)
	{
	}

	virtual ~DOM_XPathRef_And()
	{
		OP_DELETE(lhs);
		OP_DELETE(rhs);
	}

	virtual DOM_XPathRef_Value Evaluate(DOM_Node *context, double position, double size)
	{
		if (lhs->Evaluate(context, position, size).AsBoolean().boolean_value &&
			rhs->Evaluate(context, position, size).AsBoolean().boolean_value)
			return TRUE;
		// ADS 1.2 compiler crashes if you unify these two return statements (using ? : or naked boolean expression)
		return FALSE;
	}

private:
	DOM_XPathRef_Expression *lhs, *rhs;
};

class DOM_XPathRef_Equality
	: public DOM_XPathRef_Expression
{
public:
	DOM_XPathRef_Equality(const OpString &op, DOM_XPathRef_Expression *lhs, DOM_XPathRef_Expression *rhs)
		: equal(op == "="),
		  lhs(lhs),
		  rhs(rhs)
	{
	}

	virtual ~DOM_XPathRef_Equality()
	{
		OP_DELETE(lhs);
		OP_DELETE(rhs);
	}

	virtual DOM_XPathRef_Value Evaluate(DOM_Node *context, double position, double size)
	{
		DOM_XPathRef_Value lhs_value(lhs->Evaluate(context, position, size));
		DOM_XPathRef_Value rhs_value(rhs->Evaluate(context, position, size));

		return DOM_XPathRef_Value(!CompareInternal(lhs_value, rhs_value) == !equal ? 1 : 0);
	}

	BOOL CompareInternal(const DOM_XPathRef_Value &lhs_value, const DOM_XPathRef_Value &rhs_value)
	{
		if (lhs_value.type == DOM_XPathRef_Value::TYPE_BOOLEAN || rhs_value.type == DOM_XPathRef_Value::TYPE_BOOLEAN)
			return !lhs_value.AsBoolean().boolean_value == !rhs_value.AsBoolean().boolean_value;
		else if (lhs_value.type == DOM_XPathRef_Value::TYPE_NODESET && rhs_value.type == DOM_XPathRef_Value::TYPE_NODESET)
		{
			DOM_XPathRef_NodeSet *nodeset1 = lhs_value.nodeset_value, *nodeset2 = rhs_value.nodeset_value, *smaller, *larger;

			if (nodeset1->GetCount() < nodeset2->GetCount())
				smaller = nodeset1, larger = nodeset2;
			else
				smaller = nodeset2, larger = nodeset1;

			OpAutoStringHashTable<OpString> table;
			unsigned position;

			for (position = 1; position <= smaller->GetCount(); ++position)
			{
				OpString *string = OP_NEW(OpString, ());
				OpStatus::Ignore(string->Set(DOM_XPathRef_StringValue(smaller->GetInAnyOrder(position)).string_value));
				if (table.Add(string->CStr(), string) != OpStatus::OK)
					OP_DELETE(string);
			}

			for (position = 1; position <= larger->GetCount(); ++position)
				if (!table.Contains(DOM_XPathRef_StringValue(larger->GetInAnyOrder(position)).string_value.CStr()) == !equal)
					return equal;
			return !equal;
		}
		else if (lhs_value.type == DOM_XPathRef_Value::TYPE_NODESET)
		{
			for (unsigned position = 1; position <= lhs_value.nodeset_value->GetCount(); ++position)
				if (!CompareInternal(DOM_XPathRef_StringValue(lhs_value.nodeset_value->GetInAnyOrder(position)), rhs_value) == !equal)
					return equal;
			return !equal;
		}
		else if (rhs_value.type == DOM_XPathRef_Value::TYPE_NODESET)
		{
			for (unsigned position = 1; position <= rhs_value.nodeset_value->GetCount(); ++position)
				if (!CompareInternal(DOM_XPathRef_StringValue(rhs_value.nodeset_value->GetInAnyOrder(position)), lhs_value) == !equal)
					return equal;
			return !equal;
		}
		else if (lhs_value.type == DOM_XPathRef_Value::TYPE_NUMBER || rhs_value.type == DOM_XPathRef_Value::TYPE_NUMBER)
			return lhs_value.AsNumber().number_value == rhs_value.AsNumber().number_value;
		else if (lhs_value.type == DOM_XPathRef_Value::TYPE_STRING || rhs_value.type == DOM_XPathRef_Value::TYPE_STRING)
			return lhs_value.AsString().string_value == rhs_value.AsString().string_value;
		else
		{
			OP_ASSERT(!"Unexpected!");
			return FALSE;
		}
	}

private:
	BOOL equal;
	DOM_XPathRef_Expression *lhs, *rhs;
};

class DOM_XPathRef_Relational
	: public DOM_XPathRef_Expression
{
public:
	enum Type { REL_LT, REL_LTE, REL_GT, REL_GTE };

	DOM_XPathRef_Relational(const OpString &op, DOM_XPathRef_Expression *lhs, DOM_XPathRef_Expression *rhs)
		: lhs(lhs),
		  rhs(rhs)
	{
		if (op == "<")
			type = REL_LT;
		else if (op == "<=")
			type = REL_LTE;
		else if (op == ">")
			type = REL_GT;
		else
			type = REL_GTE;
	}

	virtual ~DOM_XPathRef_Relational()
	{
		OP_DELETE(lhs);
		OP_DELETE(rhs);
	}

	virtual DOM_XPathRef_Value Evaluate(DOM_Node *context, double position, double size)
	{
		DOM_XPathRef_Value lhs_value(lhs->Evaluate(context, position, size));
		DOM_XPathRef_Value rhs_value(rhs->Evaluate(context, position, size));

		OP_ASSERT(lhs_value.type != DOM_XPathRef_Value::TYPE_NODESET && rhs_value.type != DOM_XPathRef_Value::TYPE_NODESET);

		if (lhs_value.AsNumber().number_value < rhs_value.AsNumber().number_value)
			return DOM_XPathRef_Value(type == REL_LT || type == REL_LTE ? 1 : 0);
		else if (lhs_value.AsNumber().number_value > rhs_value.AsNumber().number_value)
			return DOM_XPathRef_Value(type == REL_GT || type == REL_GTE ? 1 : 0);
		else
			return DOM_XPathRef_Value(type == REL_LTE || type == REL_GTE ? 1 : 0);
	}

private:
	Type type;
	DOM_XPathRef_Expression *lhs, *rhs;
};

class DOM_XPathRef_Additive
	: public DOM_XPathRef_Expression
{
public:
	DOM_XPathRef_Additive(const OpString &op, DOM_XPathRef_Expression *lhs, DOM_XPathRef_Expression *rhs)
		: add(op == "+"),
		  lhs(lhs),
		  rhs(rhs)
	{
	}

	virtual ~DOM_XPathRef_Additive()
	{
		OP_DELETE(lhs);
		OP_DELETE(rhs);
	}

	virtual DOM_XPathRef_Value Evaluate(DOM_Node *context, double position, double size)
	{
		DOM_XPathRef_Value lhs_value(lhs->Evaluate(context, position, size).AsNumber());
		DOM_XPathRef_Value rhs_value(rhs->Evaluate(context, position, size).AsNumber());

		if (add)
			return DOM_XPathRef_Value(lhs_value.number_value + rhs_value.number_value);
		else
			return DOM_XPathRef_Value(lhs_value.number_value - rhs_value.number_value);
	}

private:
	BOOL add;
	DOM_XPathRef_Expression *lhs, *rhs;
};

class DOM_XPathRef_Multiplicative
	: public DOM_XPathRef_Expression
{
public:
	enum Type { OP_MUL, OP_DIV, OP_MOD };

	DOM_XPathRef_Multiplicative(const OpString &op, DOM_XPathRef_Expression *lhs, DOM_XPathRef_Expression *rhs)
		: lhs(lhs),
		  rhs(rhs)
	{
		if (op == "*")
			type = OP_MUL;
		else if (op == "div")
			type = OP_DIV;
		else
			type = OP_MOD;
	}

	virtual ~DOM_XPathRef_Multiplicative()
	{
		OP_DELETE(lhs);
		OP_DELETE(rhs);
	}

	virtual DOM_XPathRef_Value Evaluate(DOM_Node *context, double position, double size)
	{
		DOM_XPathRef_Value lhs_value(lhs->Evaluate(context, position, size).AsNumber());
		DOM_XPathRef_Value rhs_value(rhs->Evaluate(context, position, size).AsNumber());

		if (type == OP_MUL)
			return DOM_XPathRef_Value(lhs_value.number_value * rhs_value.number_value);
		else if (type == OP_DIV)
			return DOM_XPathRef_Value(lhs_value.number_value / rhs_value.number_value);
		else
			return DOM_XPathRef_Value(static_cast<double>(static_cast<long>(lhs_value.number_value) % static_cast<long>(rhs_value.number_value)));
	}

private:
	Type type;
	DOM_XPathRef_Expression *lhs, *rhs;
};

class DOM_XPathRef_Union
	: public DOM_XPathRef_Expression
{
public:
	DOM_XPathRef_Union(DOM_XPathRef_Expression *lhs, DOM_XPathRef_Expression *rhs)
		: lhs(lhs),
		  rhs(rhs)
	{
	}

	virtual ~DOM_XPathRef_Union()
	{
		OP_DELETE(lhs);
		OP_DELETE(rhs);
	}

	virtual DOM_XPathRef_Value Evaluate(DOM_Node *context, double position, double size)
	{
		DOM_XPathRef_Value lhs_value(lhs->Evaluate(context, position, size));
		DOM_XPathRef_Value rhs_value(rhs->Evaluate(context, position, size));
		DOM_XPathRef_NodeSet *result = OP_NEW(DOM_XPathRef_NodeSet, ());

		for (unsigned lhs_index = 1, lhs_count = lhs_value.nodeset_value->GetCount(); lhs_index <= lhs_count; ++lhs_index)
			result->Add(lhs_value.nodeset_value->GetInAnyOrder(lhs_index));
		for (unsigned rhs_index = 1, rhs_count = rhs_value.nodeset_value->GetCount(); rhs_index <= rhs_count; ++rhs_index)
			result->Add(rhs_value.nodeset_value->GetInAnyOrder(rhs_index));

		return DOM_XPathRef_Value(result);
	}

private:
	DOM_XPathRef_Expression *lhs, *rhs;
};

class DOM_XPathRef_Predicate
	: public DOM_XPathRef_Expression
{
public:
	DOM_XPathRef_Predicate(DOM_XPathRef_Expression *lhs, DOM_XPathRef_Expression *rhs)
		: lhs(lhs),
		  rhs(rhs)
	{
	}

	virtual ~DOM_XPathRef_Predicate()
	{
		OP_DELETE(lhs);
		OP_DELETE(rhs);
	}

	virtual DOM_XPathRef_Value Evaluate(DOM_Node *context, double position, double size)
	{
		DOM_XPathRef_Value lhs_value(lhs->Evaluate(context, position, size));
		DOM_XPathRef_NodeSet *result = OP_NEW(DOM_XPathRef_NodeSet, ());

		for (unsigned lhs_index = 1, lhs_count = lhs_value.nodeset_value->GetCount(); lhs_index <= lhs_count; ++lhs_index)
		{
			DOM_XPathRef_Value rhs_value(rhs->Evaluate(lhs_value.nodeset_value->GetInDocumentOrder(lhs_index), lhs_index, lhs_count));
			if (rhs_value.type == DOM_XPathRef_Value::TYPE_NUMBER ? rhs_value.number_value == static_cast<double>(lhs_index) : rhs_value.AsBoolean().boolean_value)
				result->Add(lhs_value.nodeset_value->GetInDocumentOrder(lhs_index));
		}

		return DOM_XPathRef_Value(result);
	}

private:
	DOM_XPathRef_Expression *lhs, *rhs;
};

class DOM_XPathRef_NegateExpression
	: public DOM_XPathRef_Expression
{
public:
	DOM_XPathRef_NegateExpression(DOM_XPathRef_Expression *expression)
		: expression(expression)
	{
	}

	virtual ~DOM_XPathRef_NegateExpression()
	{
		OP_DELETE(expression);
	}

	virtual DOM_XPathRef_Value Evaluate(DOM_Node *node, double position, double size)
	{
		return DOM_XPathRef_Value(-expression->Evaluate(node, position, size).AsNumber().number_value);
	}

private:
	DOM_XPathRef_Expression *expression;
};

class DOM_XPathRef_Number
	: public DOM_XPathRef_Expression
{
public:
	DOM_XPathRef_Number(const uni_char *value)
	{
		uni_char *endptr;
		number_value = value ? uni_strtod (value, &endptr) : op_nan (0);
	}

	virtual DOM_XPathRef_Value Evaluate(DOM_Node *node, double position, double size)
	{
		return DOM_XPathRef_Value(number_value);
	}

private:
	double number_value;
};

class DOM_XPathRef_String
	: public DOM_XPathRef_Expression
{
public:
	DOM_XPathRef_String(const uni_char *value)
	{
		OpStatus::Ignore(string_value.Set(value));
	}

	virtual DOM_XPathRef_Value Evaluate(DOM_Node *node, double position, double size)
	{
		return DOM_XPathRef_Value(string_value.CStr());
	}

private:
	OpString string_value;
};

static DOM_XPathRef_Expression *
DOM_XPathRef_ParseExpression(const OpVector<DOM_XPathRef_Token> &tokens, unsigned &index, DOM_XPathRef_Production production)
{
	DOM_XPathRef_Expression *expression = NULL;
	DOM_XPathRef_Token *token = tokens.Get(index);

	if (*token == "-")
	{
		++index;
		expression = OP_NEW(DOM_XPathRef_NegateExpression, (DOM_XPathRef_ParseExpression(tokens, index)));
	}
	else if (*token == DOM_XPathRef_Token::TYPE_FUNCTION)
	{
		OpString *name = &token->value;
		OpAutoVector<DOM_XPathRef_Expression> *arguments = OP_NEW(OpAutoVector<DOM_XPathRef_Expression>, ());

		token = tokens.Get(++index);
		OP_ASSERT(*token == "(");

		if (*tokens.Get(++index) != ")")
			do
				arguments->Add(DOM_XPathRef_ParseExpression(tokens, index));
			while (*tokens.Get(index) == "," && ++index);

		OP_ASSERT(*tokens.Get(index) == ")");

		++index;
		expression = OP_NEW(DOM_XPathRef_FunctionCall, (DOM_XPathRef_FunctionCall::FunctionByName(*name), arguments));
	}
	else if (*token == DOM_XPathRef_Token::TYPE_AXIS || *token == "/")
		expression = OP_NEW(DOM_XPathRef_LocationPath, (NULL, tokens, index));
	else if (*token == "(")
	{
		++index;
		expression = DOM_XPathRef_ParseExpression(tokens, index);
		OP_ASSERT(*tokens.Get(index) == ")");
		++index;
	}
	else if (*token == DOM_XPathRef_Token::TYPE_NUMBER)
	{
		expression = OP_NEW(DOM_XPathRef_Number, (token->value.CStr()));
		++index;
	}
	else if (*token == DOM_XPathRef_Token::TYPE_STRING)
	{
		expression = OP_NEW(DOM_XPathRef_String, (token->value.CStr()));
		++index;
	}

	while (expression && index < tokens.GetCount())
	{
		token = tokens.Get(index);

		if (*token == DOM_XPathRef_Token::TYPE_OPERATOR)
			switch (production)
			{
			case DOM_XPATHREF_P_OR:
				if (*token == "or")
				{
					++index;
					expression = OP_NEW(DOM_XPathRef_Or, (expression, DOM_XPathRef_ParseExpression(tokens, index, DOM_XPATHREF_P_AND)));
					continue;
				}

			case DOM_XPATHREF_P_AND:
				if (*token == "and")
				{
					++index;
					expression = OP_NEW(DOM_XPathRef_And, (expression, DOM_XPathRef_ParseExpression(tokens, index, DOM_XPATHREF_P_EQUALITY)));
					continue;
				}

			case DOM_XPATHREF_P_EQUALITY:
				if (*token == "=" || *token == "!=")
				{
					++index;
					expression = OP_NEW(DOM_XPathRef_Equality, (token->value, expression, DOM_XPathRef_ParseExpression(tokens, index, DOM_XPATHREF_P_RELATIONAL)));
					continue;
				}

			case DOM_XPATHREF_P_RELATIONAL:
				if (*token == "<" || *token == ">" || *token == "<=" || *token == ">=")
				{
					++index;
					expression = OP_NEW(DOM_XPathRef_Relational, (token->value, expression, DOM_XPathRef_ParseExpression(tokens, index, DOM_XPATHREF_P_ADDITIVE)));
					continue;
				}

			case DOM_XPATHREF_P_ADDITIVE:
				if (*token == "+" || *token == "-")
				{
					++index;
					expression = OP_NEW(DOM_XPathRef_Additive, (token->value, expression, DOM_XPathRef_ParseExpression(tokens, index, DOM_XPATHREF_P_MULTIPLICATIVE)));
					continue;
				}

			case DOM_XPATHREF_P_MULTIPLICATIVE:
				if (*token == "*" || *token == "div" || *token == "mod")
				{
					++index;
					expression = OP_NEW(DOM_XPathRef_Multiplicative, (token->value, expression, DOM_XPathRef_ParseExpression(tokens, index, DOM_XPATHREF_P_UNARY)));
					continue;
				}

			case DOM_XPATHREF_P_UNARY:
				if (*token == "|")
				{
					++index;
					expression = OP_NEW(DOM_XPathRef_Union, (expression, DOM_XPathRef_ParseExpression(tokens, index, DOM_XPATHREF_P_PRIMARY)));
					continue;
				}
			}

		if (*token == "[")
		{
			++index;
			expression = OP_NEW(DOM_XPathRef_Predicate, (expression, DOM_XPathRef_ParseExpression(tokens, index)));
			OP_ASSERT(*tokens.Get(index) == "]");
			++index;
			continue;
		}
		else if (*token == "/")
		{
			++index;
			expression = OP_NEW(DOM_XPathRef_LocationPath, (expression, tokens, index));
			continue;
		}

		break;
	}

	return expression;
}

DOM_XPathRef_Expression *
DOM_XPathRef_ParseExpression(const uni_char *source)
{
	OpAutoVector<DOM_XPathRef_Token> tokens;
	DOM_XPathRef_Token *last = NULL, *current = NULL;
	BOOL special = FALSE;

	while (*source)
	{
		while (*source && XMLUtils::IsSpace(*source)) ++source;

		if (*source)
		{
			const uni_char *start = source;

			if (op_isdigit(*source) || source[0] == '.' && op_isdigit(source[1]))
			{
				while (*source && op_isdigit(*source)) ++source;

				if (*source == '.')
				{
					++source;
					while (*source && op_isdigit(*source)) ++source;
				}

				current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_NUMBER, start, source));
			}
			else if (XMLUtils::IsNameFirst(XMLVERSION_1_0, *source))
			{
				do ++source; while (XMLUtils::IsName(XMLVERSION_1_0, *source) && (source[0] != ':' || source[1] != ':'));

				if (special)
					current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_OPERATOR, start, source));
				else
				{
					if (*source == '*')
						++source;

					const uni_char *end = source;

					while (*source && XMLUtils::IsSpace(*source)) ++source;

					if (source[0] == ':' && source[1] == ':')
					{
						current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_AXIS, start, end));
						source += 2;
					}
					else if (source[0] == '(')
					{
						current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_FUNCTION, start, end));

						if (*current == "node" || *current == "text" || *current == "comment")
						{
							do ++source; while (*source && XMLUtils::IsSpace(*source));
							OP_ASSERT(*source == ')');
							++source;

							OP_DELETE(current);
							current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_NODETYPETEST, start, end));
						}
						else if (*current == "processing-instruction")
						{
							do ++source; while (*source && XMLUtils::IsSpace(*source));
							OP_ASSERT(*source == ')' || *source == '\'' || *source == '"');
							if (*source == ')')
							{
								++source;

								OP_DELETE(current);
								current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_NODETYPETEST, start, end));
							}
							else
							{
								start = source;
								uni_char delim = *source;
								do ++source; while (*source && *source != delim);
								OP_ASSERT(*source == delim);

								OP_DELETE(current);
								current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_PITARGETTEST, start + 1, source));

								do ++source; while (*source && XMLUtils::IsSpace(*source));
								OP_ASSERT(*source == ')');
								++source;
							}
						}
					}
					else
						current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_NAMETEST, start, end));
				}
			}
			else if (*source == '\'' || *source == '"')
			{
				uni_char delim = *source;

				do ++source; while (*source && *source != delim);

				current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_STRING, start + 1, source));
				++source;
			}
			else if (source[0] == '*')
			{
				if (special)
					current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_OPERATOR, start, ++source));
				else
					current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_NAMETEST, start, ++source));
			}
			else if (source[0] == '@')
			{
				current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_AXIS, UNI_L("attribute")));
				++source;
			}
			else if (source[0] == '.')
			{
				if (source[1] == '.')
					tokens.Add(last = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_AXIS, UNI_L("parent"))));
				else
					tokens.Add(last = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_AXIS, UNI_L("self"))));

				current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_NODETYPETEST, UNI_L("node")));
				++source;
			}
			else if (source[0] == '/' && source[1] == '/')
			{
				tokens.Add(OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_OPERATOR, UNI_L("/"))));
				tokens.Add(OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_AXIS, UNI_L("descendant-or-self"))));
				tokens.Add(last = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_NODETYPETEST, UNI_L("node"))));
				current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_OPERATOR, UNI_L("/")));
				source += 2;
			}
			else if (*source == '(' || *source == ')' || *source == '[' || *source == ']' || *source == ',')
				current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_PUNCTUATOR, start, ++source));
			else
			{
				unsigned length;

				if ((source[0] == '!' || source[0] == '<' || source[0] == '>') && source[1] == '=')
					length = 2;
				else
					length = 1;

				current = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_OPERATOR, start, source += length));
			}
		}

		if (*current == DOM_XPathRef_Token::TYPE_NODETYPETEST || *current == DOM_XPathRef_Token::TYPE_NAMETEST || *current == DOM_XPathRef_Token::TYPE_PITARGETTEST)
			if (!last || *last != DOM_XPathRef_Token::TYPE_AXIS)
				OpStatus::Ignore(tokens.Add(last = OP_NEW(DOM_XPathRef_Token, (DOM_XPathRef_Token::TYPE_AXIS, UNI_L("child")))));

		OpStatus::Ignore(tokens.Add(current));

		special = *current != DOM_XPathRef_Token::TYPE_OPERATOR && *current != DOM_XPathRef_Token::TYPE_AXIS && (*current != DOM_XPathRef_Token::TYPE_PUNCTUATOR || *current != "(" && *current != "[");
		last = current;
	}

	if (tokens.GetCount() == 0)
		return NULL;

	unsigned index = 0;
	return DOM_XPathRef_ParseExpression(tokens, index);
}

int
DOM_XPathRef_Evaluate(DOM_Object *this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);
	DOM_CHECK_ARGUMENTS("so");
	DOM_ARGUMENT_OBJECT(context, 1, DOM_TYPE_NODE, DOM_Node);

	DOM_XPathRef_InitializeDocument(document);
	DOM_XPathRef_Expression *expression = DOM_XPathRef_ParseExpression(argv[0].value.string);
	if (!expression) // Failed to parse the string as an xpath expression, or OOM
		return ES_FAILED;
	DOM_XPathRef_Value result(expression->Evaluate(context, 1.0, 1.0));

	OP_DELETE(expression);

	if (result.type == DOM_XPathRef_Value::TYPE_NUMBER)
		DOM_Object::DOMSetNumber(return_value, result.number_value);
	else if (result.type == DOM_XPathRef_Value::TYPE_STRING)
	{
		TempBuffer *buffer = DOM_Object::GetEmptyTempBuf();
		OpStatus::Ignore(buffer->Append(result.string_value.CStr()));
		DOM_Object::DOMSetString(return_value, buffer);
	}
	else if (result.type == DOM_XPathRef_Value::TYPE_BOOLEAN)
		DOM_Object::DOMSetBoolean(return_value, result.boolean_value);
	else
		DOM_Object::DOMSetObject(return_value, OP_NEW(DOM_XPathRef_NodeSetObject, (result.nodeset_value, document->GetRuntime())));

	return ES_VALUE;
}

#endif // DOM_XPATH_REFERENCE_IMPLEMENTATION
