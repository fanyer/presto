/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLTREEACCESSOR_H
#define XMLTREEACCESSOR_H

#ifdef XMLUTILS_XMLTREEACCESSOR_SUPPORT

#include "modules/xmlutils/xmlnames.h"
#include "modules/url/url2.h"

#ifdef XMLUTILS_XMLTREEACCESSOR_FALLBACK_SUPPORT
# define MAYBE_PURE_VIRTUAL
#else // XMLUTILS_XMLTREEACCESSOR_FALLBACK_SUPPORT
# define MAYBE_PURE_VIRTUAL = 0
#endif // XMLUTILS_XMLTREEACCESSOR_FALLBACK_SUPPORT

class XMLDocumentInformation;

/** Interface for accessing a tree of nodes.  The lifetime and stability of
    the accessed tree is outside the scope of this interface definition.  No
    support is provided through the interface for signalling changes to the
    tree, to modify the tree or to invalidate the tree accessor when the
    accessed tree is modified by some other mechanism. */
class XMLTreeAccessor
{
public:
	virtual BOOL IsCaseSensitive() = 0;
	/**< Returns whether the tree accessed by this tree accessor has case
	     sensitive names. */

	typedef void Node;
	/**< Opaque node type.  A pointer to a Node is stable and valid as long as
	     the XMLTreeAccessor object and the referenced actual node are valid
	     (how long that is is outside the scope of this interface definition.)
	     Each time a reference to a specific actual node is returned, the same
	     pointer value should be returned, so pointer comparisons should be
	     valid. */

	/** Node type. */
	enum NodeType
	{
		TYPE_ROOT,
		/**< Root node.  Parent of the root element, if there is one, and any
		     sibling comments, processing instructions and character data of
		     the root element.  Has children. */
		TYPE_ELEMENT,
		/**< Ordinary element.  Has parent, siblings, children, name,
		     attributes and namespaces. */
		TYPE_TEXT,
		/**< Plain text (non-CDATA section character data.)  Has parent,
		     siblings and data. */
		TYPE_CDATA_SECTION,
		/**< CDATA section character data.  Has parent, siblings and data. */
		TYPE_COMMENT,
		/**< Comment.  Has parent, siblings and data. */
		TYPE_PROCESSING_INSTRUCTION,
		/**< Processing instruction.  Has parent, siblings, name and data. */
		TYPE_DOCTYPE
		/**< DOCTYPE declaration.  Has parent (always root node), siblings,
		     name (root element name) and data (internal subset.) */
	};

	virtual NodeType GetNodeType(Node *node) = 0;
	/**< Returns the node's type. */

	virtual Node *GetRoot() = 0;
	/**< Returns the tree accessor's root node.  The returned node's node type
	     will be TYPE_ROOT.  Never returns NULL. */

	/* Tree traversal functions (affected by set filters.) */

	virtual Node *GetRootElement() MAYBE_PURE_VIRTUAL;
	/**< Returns the tree accessor's root element node, if there is one.  The
	     returned node's node type will be TYPE_ELEMENT and its parent node
	     will be the root node. */
	virtual Node *GetParent(Node *from) = 0;
	/**< Returns the parent node of 'from'.  Without filters, never returns
	     NULL except if 'from' is the root node, in which case NULL is always
	     returned.  With filters, also returns NULL if the parent node is
	     excluded by the filters. */
	virtual Node *GetAncestor(Node *from) MAYBE_PURE_VIRTUAL;
	/**< Returns the nearest ancestor node of 'from' that is included by set
	     filters.  Without filters, this is the same as GetParent(). */
	virtual Node *GetFirstChild(Node *from) = 0;
	/**< Returns the first child node of 'from'.  If 'from' is of another type
	     than TYPE_ROOT or TYPE_ELEMENT, NULL is always returned. */
	virtual Node *GetLastChild(Node *from) = 0;
	/**< Returns the last child node of 'from'.  If 'from' is of another type
	     than TYPE_ROOT or TYPE_ELEMENT, NULL is always returned. */
	virtual Node *GetNextSibling(Node *from) = 0;
	/**< Returns the next node in document order that has the same parent node
	     as 'from'.  Always returns NULL for the root node. */
	virtual Node *GetPreviousSibling(Node *from) = 0;
	/**< Returns the previous node in document order that has the same parent
	     node as 'from'.  Always returns NULL for the root node. */
	virtual Node *GetNext(Node *from) MAYBE_PURE_VIRTUAL;
	/**< Returns the next node in document order from 'from'. */
	virtual Node *GetNextDescendant(Node *from, Node *ancestor) MAYBE_PURE_VIRTUAL;
	/**< Returns the next node in document order from 'from' that is a
	     descendant of 'ancestor'.  The node 'from' must be equal to
	     'ancestor' or be a descendant of 'ancestor'.  If it isn't, results
	     are undefined. */
	virtual Node *GetNextNonDescendant(Node *from) MAYBE_PURE_VIRTUAL;
	/**< Returns the next node in document order that is not a descendant of
	     'from'.  Always returns NULL for the root node. */
	virtual Node *GetPrevious(Node *from) MAYBE_PURE_VIRTUAL;
	/**< Returns the previous node in document order from 'from'.  Always
	     returns NULL for the root node. */
	virtual Node *GetPreviousNonAncestor(Node *from, Node *origin) MAYBE_PURE_VIRTUAL;
	/**< Returns the previous node in document order from 'from' that is not
	     an ancestor of 'origin'.  The node 'from' must be equal to 'origin'
	     or precede it in the document.  If 'origin' precedes 'from' in the
	     document, results are undefined. */

	virtual BOOL IsAncestorOf(Node *parent, Node *child) MAYBE_PURE_VIRTUAL;
	/**< Returns TRUE if 'child' has 'parent' has ancestor or if 'parent ==
	     child'.  The result is not affected by any filters set. */

	virtual BOOL IsEmptyText(Node *node) = 0;
	/**< Returns TRUE if 'node' is of type TYPE_TEXT and has zero length
	     content. */

	virtual BOOL IsWhitespaceOnly(Node *node) = 0;
	/**< Returns TRUE if 'node' is of type TYPE_TEXT and either has zero
	     length content or content that matches the S (white space) production
	     in XML 1.0 (which is the same as the one in XML 1.1.) */

	virtual BOOL Precedes(Node *node1, Node *node2) MAYBE_PURE_VIRTUAL;
	/**< Returns TRUE if 'node1' is before or the same as 'node2'.  The result
	     is not affected by any filters set. */

	/* Traversal filtering functions.  Filtering only reduces the set of nodes
	   traversed by the tree traversal functions, it never causes a function
	   to return a node it wouldn't have returned without the filters set.
	   For instance, given the tree

	     <root>
	       <parent><child/></parent>
	       <parent><child/></parent>
	     </root>

	   the call 'GetFirstChild(<root>)' will return NULL if a filter is set
	   that excludes <parent> elements from being returned.  It will not
	   return a <child> element as the first child, even if the filter that
	   excluded <parent> elements include <child> elements. */

	virtual void SetStopAtFilter(Node *node) MAYBE_PURE_VIRTUAL;
	/**< Sets a filter that causes the tree traversal functions to return NULL
	     if 'node' is between the origin node and the would-be result node in
	     document order, or if the would-be result node is 'node'.  If other
	     filters are set, it doesn't matter if 'node' is included by them or
	     not. */
	virtual void SetNodeTypeFilter(NodeType nodetype, BOOL join_text_and_cdata_section) MAYBE_PURE_VIRTUAL;
	/**< Sets a filter that causes the tree traversal functions to return only
	     nodes of the right type.  If 'nodetype' is TYPE_TEXT and
	     'join_text_and_cdata_section' is TRUE, nodes of the type
	     TYPE_CDATA_SECTION will also be included. */
	virtual OP_STATUS SetElementNameFilter(const XMLExpandedName &name, BOOL copy = TRUE, unsigned *data1 = NULL, unsigned *data2 = NULL) MAYBE_PURE_VIRTUAL;
	/**< Sets a filter that causes the tree traversal functions to return only
	     TYPE_ELEMENT nodes with the right name.  If the specified name has an
	     empty local part, any element with the right namespace URI will be
	     returned.  If the specified name has an empty namespace URI, only
	     elements in the null namespace will be returned.  The function
	     GetRoot() will return the root node regardless of set filters, but
	     all other non-element types of node are skipped.

	     If 'copy' is TRUE, all strings in 'name' are copied.  If 'copy' is
	     FALSE, the strings in 'name' (but not the XMLExpandedName object
	     itself) needs to remain valid (and unchanged) until the filter is
	     reset by a call to ResetFilters() or by a new call to this function.
	     Since no copying is made if 'copy' is FALSE, this function is
	     guaranteed to return OpStatus::OK in that case.

	     To make filter setting more efficient, the caller can let the tree
	     accessor store some data in two unsigned int:s by passing pointers to
	     them via 'data1' and 'data2'.  If it does, the int:s must be set to
	     the value '~0u' initially, and not be updated after the first time
	     the filter is set with the same name.  The values set by the tree
	     accessor is assumed to be specific for the name used when they were
	     stored.  The caller can always reset them to '~0u' if the name has
	     changed.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if 'copy' is TRUE and
	             the copying fails. */
	virtual OP_STATUS SetAttributeNameFilter(const XMLExpandedName &name, BOOL copy = TRUE, unsigned *data1 = NULL, unsigned *data2 = NULL) MAYBE_PURE_VIRTUAL;
	/**< Sets a filter that causes the tree traversal functions to return only
	     TYPE_ELEMENT nodes with an attribute with the specified name.  If the
	     specified name has an empty local part, any element with an attribute
	     with the right namespace URI will be returned.  If the specified name
	     has an empty namespace URI, only elements with an attribute in the
	     null namespace will be returned.  If an element name filter is also
	     set, the combined effect is that only element nodes with a matching
	     name and an attribute with a matching name will be returned.  The
	     function GetRoot() will return the root node regardless of set
	     filters, but all other non-element types of node are skipped.

	     If 'copy' is TRUE, all strings in 'name' are copied.  If 'copy' is
	     FALSE, the strings in 'name' (but not the XMLExpandedName object
	     itself) needs to remain valid (and unchanged) until the filter is
	     reset by a call to ResetFilters() or by a new call to this function.
	     Since no copying is made if 'copy' is FALSE, this function is
	     guaranteed to return OpStatus::OK in that case.

	     To make filter setting more efficient, the caller can let the tree
	     accessor store some data in two unsigned int:s by passing pointers to
	     them via 'data1' and 'data2'.  If it does, the int:s must be set to
	     the value '~0u' initially, and not be updated after the first time
	     the filter is set with the same name.  The values set by the tree
	     accessor is assumed to be specific for the name used when they were
	     stored.  The caller can always reset them to '~0u' if the name has
	     changed.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if 'copy' is TRUE and
	             the copying fails. */
	virtual OP_STATUS SetAttributeValueFilter(const uni_char *value, BOOL copy = TRUE) MAYBE_PURE_VIRTUAL;
	/**< Sets a filter that causes the tree traversal functions to return only
	     TYPE_ELEMENT nodes with an attribute with the specified value.  If an
	     attribute name filter is also set, the combined effect is that only
	     element nodes with an attribute with a matching name and matching
	     value will be returned.  The function GetRoot() will return the root
	     node regardless of set filters, but all other non-element types of
	     node are skipped.

	     If 'copy' is TRUE, the string 'value' is copied.  If 'copy' is FALSE,
	     the string needs to remain valid (and unchanged) until the filter is
	     reset by a call to ResetFilters() or by a new call to this function.
	     Since no copying is made if 'copy' is FALSE, this function is
	     guaranteed to return OpStatus::OK in that case.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if 'copy' is TRUE and
	             the copying fails. */
	virtual void ResetFilters() MAYBE_PURE_VIRTUAL;
	/**< Reset all set filters. */

	virtual BOOL FilterNode (Node *node) MAYBE_PURE_VIRTUAL;
	/**< Apply current filters to 'node' and return FALSE if it would have
	     been filtered out by them, and TRUE otherwise.  If no filters are
	     set, TRUE is returned.  Filters set by SetStopAtFilter() do not
	     affect the outcome of this check.

	     @return FALSE if 'node' would be filtered out by current filters. */

	virtual void GetName(XMLCompleteName &name, Node *node) = 0;
	/**< Returns the node's name, if it has one. */

	virtual XMLCompleteName GetName(Node *node);
	/**< Returns the node's name, if it has one. */

	virtual const uni_char *GetPITarget(Node *node) = 0;
	/**< Returns the target of the processing instruction */

	/** Iterator for the attributes of an element node.  The iterator object
	    is owned by the tree accessor object and is expected to be reused each
	    time an element's attributes are accessed.  It is thus not possible to
	    have several active attribute iterators at the same time.  Starting an
	    iteration is however excepted to be cheap. */
	class Attributes
	{
	public:
		virtual unsigned GetCount() = 0;
		/**< Returns the number of to be iterated (attributes with default
		     values and namespace declaration attributes may be excluded from
		     the count, as requested in the corresponding call to
		     XMLTreeAccessor::GetAttributes().)  Is expected to count the
		     attributes in each call, so calling code should typically store
		     the result in a local variable. */

		virtual OP_STATUS GetAttribute(unsigned index, XMLCompleteName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer) = 0;
		/**< Retrieves the name and value of the index:th attribute.  The
		     index must be lower than the number returned by GetCount().  It
		     is possible that the value needs to be generated on the fly, into
		     the buffer specified by the 'buffer' argument.  If that argument
		     is NULL and the argument value needed to be generated, 'value' is
		     set to NULL.  This can be used as an optimization in case only
		     the name and attribute flags was of interest.

		     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if the value
		             needed to be generated, a buffer to generate it into was
		             specified and the generation failed. */

	protected:
		virtual ~Attributes() {}
		/**< Destructor.  The Attributes object is owned by the tree accessor
		     and cannot be deallocated by users. */
	};

	virtual void GetAttributes(Attributes *&attributes, Node *node, BOOL ignore_default, BOOL ignore_nsdeclarations) = 0;
	/**< Start an iteration over the node's attributes.  If the node is of any
	     type other than TYPE_ELEMENT, the iterator object will report zero
	     attributes.  Attributes with default values and namespace declaration
	     attributes can be ignored.  See the Attributes class' documentation
	     for details. */

	virtual Attributes *GetAttributes(Node *node, BOOL ignore_default, BOOL ignore_nsdeclarations);
	/**< Start an iteration over the node's attributes.  If the node is of any
	     type other than TYPE_ELEMENT, the iterator object will report zero
	     attributes.  Attributes with default values and namespace declaration
	     attributes can be ignored.  See the Attributes class' documentation
	     for details. */

	virtual OP_STATUS GetAttribute(Node *node, const XMLExpandedName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer) MAYBE_PURE_VIRTUAL;
	/**< Retrieves the value of an attribute.  It is possible that the value
	     needs to be generated on the fly, into the buffer specified by the
	     'buffer' argument.  If that argument is NULL and the argument value
	     needed to be generated, 'value' is set to NULL.  This can be used as
	     an optimization in case only the attribute flags was of interest.

	     @param node Node whose attributes to look at.
	     @param name Name of attribute of interest.
	     @param value Set to the attribute's value.
	     @param id Set to TRUE if the attribute is an ID attribute.
	     @param specified Set to TRUE if the attribute was specified and false
	                      if its value is a default value defined in a DTD.
	     @param buffer Optional buffer to use for generating the attribute's
	                   value.
	     @return OpStatus::OK, OpStatus:ERR if the attribute didn't exist and
	             OpStatus::ERR_NO_MEMORY if the value needed to be generated,
	             a buffer to generate it into was specified and the generation
	             failed. */

	virtual OP_STATUS GetAttribute(Attributes *attributes, const XMLExpandedName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer) MAYBE_PURE_VIRTUAL;
	/**< Retrieves the value of an attribute.  It is possible that the value
	     needs to be generated on the fly, into the buffer specified by the
	     'buffer' argument.  If that argument is NULL and the argument value
	     needed to be generated, 'value' is set to NULL.  This can be used as
	     an optimization in case only the attribute flags was of interest.

	     The attributes object should be initialized using GetAttributes(),
	     and the 'ignore_default' and 'ignore_nsdeclarations' arguments to
	     that function determines the set of attributes this function will
	     consider.

	     @param attributes Attributes to look at.
	     @param name Name of attribute of interest.
	     @param value Set to the attribute's value.
	     @param id Set to TRUE if the attribute is an ID attribute.
	     @param specified Set to TRUE if the attribute was specified and false
	                      if its value is a default value defined in a DTD.
	     @param buffer Optional buffer to use for generating the attribute's
	                   value.
	     @return OpStatus::OK, OpStatus:ERR if the attribute didn't exist and
	             OpStatus::ERR_NO_MEMORY if the value needed to be generated,
	             a buffer to generate it into was specified and the generation
	             failed. */

	virtual unsigned GetAttributeIndex(Node *node, const XMLExpandedName &name) MAYBE_PURE_VIRTUAL;
	/**< Returns the index of the attribute named 'name' on 'node'.  This is
	     the index which, when used in a call to Attributes::GetAttribute(),
	     returns the attribute with the matching name.  It is an error if no
	     attribute exists; the return value in this case is undefined.

	     @param node Node whose attributes to look at.
	     @param name Name of attribute of interest.
	     @return The attribute's index. */

	virtual unsigned GetAttributeIndex(Attributes *attributes, const XMLExpandedName &name) MAYBE_PURE_VIRTUAL;
	/**< Returns the index of the attribute named 'name' in 'attributes'.
	     This is the index which, when used in a call to
	     Attributes::GetAttribute(), returns the attribute with the matching
	     name.  It is an error if no attribute exists; the return value in
	     this case is undefined.

	     @param attributes Attributes to look at.
	     @param name Name of attribute of interest.
	     @return The attribute's index. */

	virtual BOOL HasAttribute(Node *node, const XMLExpandedName &name, const uni_char *value = NULL) MAYBE_PURE_VIRTUAL;
	/**< Returns TRUE if 'node' has an attribute named 'name' with value
	     'value' (or with any value, if value is NULL.)

	     @param node Node whose attributes to look at.
	     @param name Name of attribute of interest.
	     @param value Optional attribute value.
	     @return TRUE or FALSE. */

	virtual BOOL HasAttribute(Attributes *attributes, const XMLExpandedName &name, const uni_char *value = NULL) MAYBE_PURE_VIRTUAL;
	/**< Returns TRUE if 'attributes' contains an attribute named 'name' with
	     value 'value' (or with any value, if value is NULL.)

	     @param node Node whose attributes to look at.
	     @param name Name of attribute of interest.
	     @param value Optional attribute value.
	     @return TRUE or FALSE. */

	/** Iterator for the namespace declarations in scope at an element node.
	    The iterator object is owned by the tree accessor object and is
	    expected to be reused each time an element's in-scope namespace
	    declarations are accessed.  It is thus not possible to have several
	    active namespace iterators at the same time. */
	class Namespaces
	{
	public:
		virtual unsigned GetCount() = 0;
		/**< Counts the number of namespace declarations in scope.  If more
		     than one ancestor element has a namespace declaration with a
		     given prefix, they all count as a single namespace declaration
		     with the value from the nearest ancestors declaration.  A default
		     namespace declaration on an ancestor element is not included if a
		     nearer ancestor element has a default namespace declaration with
		     an empty value. */

		virtual void GetNamespace(unsigned index, const uni_char *&uri, const uni_char *&prefix) = 0;
		/**< Retrieves the index:th namespace declaration.  The index must be
		     lower than the number returned by GetCount().  If the index:th
		     namespace declaration is a default namespace declaration, the
		     'prefix' argument is set to NULL.  The 'uri' argument is never
		     set to NULL. */
	protected:
		virtual ~Namespaces() {} // Silence compiler
	};

	virtual OP_STATUS GetNamespaces(Namespaces *&namespaces, Node *node) MAYBE_PURE_VIRTUAL;
	/**< Start an iteration over the namespace declarations in scope at the
	     node.  If the node is of any type other than TYPE_ELEMENT, the
	     iterator object will report zero namespace declarations (even if the
	     node has a parent element node with namespace declarations in scope.)
	     Starting an iteration is expected to create a separate data structure
	     containing all namespace declarations on the argument node and its
	     ancestors, and is thus not necessarily cheap, and can fail due to
	     OOM (unlike starting an iteration over attributes.)

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual unsigned GetNamespaceIndex(Node *node, const uni_char *uri, const uni_char *prefix) MAYBE_PURE_VIRTUAL;

	virtual OP_STATUS GetData(const uni_char *&data, Node *node, TempBuffer *buffer) = 0;
	/**< Retrieves the data of 'node', if node is of a type that has data (all
	     types but TYPE_ROOT and TYPE_ELEMENT.)  If possible, 'data' is set to
	     point to a string owned by the tree accessor and 'buffer' is not
	     used.  If that is not possible, for instance because the data is
	     stored encoded or in several chunks, the data is appended to 'buffer'
	     and 'data' is set to 'buffer->GetStorage()'.  If 'buffer' was NULL,
	     'data' is set to NULL instead.

	     Note: If 'buffer' is non-empty before the call and data needed to be
	     generated into it, the string set to 'data' will contain the buffer's
	     previous content as well as the node's data.  This is intentional,
	     mostly so that 'data==buffer->GetStorage()' can be used to determine
	     whether the buffer was used.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if the data needed to
	             be generated, a buffer to generate it into was specified and
	             the generation failed. */

	virtual OP_STATUS GetCharacterDataContent(const uni_char *&data, Node *node, TempBuffer *buffer) MAYBE_PURE_VIRTUAL;
	/**< Retrieves the character data content of 'node'.  If the node's type
	     is TYPE_TEXT or TYPE_CDATA_SECTION calling this function is
	     equivalent to calling the GetData() function, otherwise if the node's
	     type is TYPE_ROOT or TYPE_ELEMENT, the result is the concatenation of
	     all TYPE_TEXT and TYPE_CDATA_SECTION descendants of the node, and
	     otherwise the result is the empty string.

	     Buffer usage and data generation works exactly the same as for
	     GetData().

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if the data needed to
	             be generated, a buffer to generate it into was specified and
	             the generation failed. */

	virtual OP_STATUS GetElementById(Node *&node, const uni_char *id) MAYBE_PURE_VIRTUAL;
	/**< Locates the first element in document order that has an ID attribute
	     with a value identical to 'id'.  If no such element exists, 'node' is
	     set to NULL.  It is expected that the first call to this function on
	     a tree accessor object may traverse the whole tree collecting all
	     element nodes with ID attributes, and that all other calls on the
	     tree accessor object are O(1) in non-worst case scenarios. */

	virtual URL GetDocumentURL() = 0;
	/**< Returns the absolute URL of the document accessed by this object or
	     an empty URL if it was not loaded from a URL. */

	virtual const XMLDocumentInformation *GetDocumentInformation() = 0;
	/**< Returns an object containing information from the XML declaration and
	     DOCTYPE declaration in the document represented by this tree
	     accessor, if such were present.  If an XML declaration was present,
	     it would have preceded the root node's first child, but it will not
	     be reported as a node in the tree.  If a DOCTYPE declaration was
	     present, there will be a TYPE_DOCTYPE node in the tree as a child of
	     the root node, preceding the root element node. */

protected:
	XMLTreeAccessor()
		: fallback_impl(0)
	{
	}
	/**< Constructor. */

	virtual ~XMLTreeAccessor() MAYBE_PURE_VIRTUAL;
	/**< Destructor.  The lifetime of a tree accessor object is inherently
	     determined by the particular implementation, and the destruction is
	     therefor defined by the implementation as well, hence the protected
	     destructor. */

	XMLTreeAccessor *fallback_impl;

	OP_STATUS ConstructFallbackImplementation();
	/**< Creates a fallback tree accessor that implements all functions in
	     this API on top of this tree accessor's versions of the pure virtual
	     functions.  It's implementations of the pure virtual functions cannot
	     be called.  The non-pure virtual functions in this API default to
	     calling the fallback tree accessor's corresponding function.  It is
	     an error if a function in this API is not overridden by a tree
	     accessor for which ConstructFallbackImplementation() hasn't been
	     called. */
};

#endif // XMLUTILS_XMLTREEACCESSOR_SUPPORT
#endif // XMLTREEACCESSOR_H
