/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPATH_H
#define XPATH_H

#ifdef XPATH_SUPPORT

#include "modules/xmlutils/xmltreeaccessor.h"

/** @mainpage XPath module
 *
 * This is the auto-generated API documentation for the XPath module.  For more
 * information about the module, see the module's
 *   <a href="http://wiki.intern.opera.no/cgi-bin/wiki/wiki.pl?Modules/dom">Wiki page</a>.
 *
 * @section howto Howto
 *
 * Here is a brief description on how to evaluate an XPath expression in C++
 * in Opera.
 *
 * @subsection step1 1. Prepare namespace prefix => URI lookup table
 *
 * If there are any qualified names in the expression, the XPath compiler
 * needs to be able to lookup namespace URIs to match the prefixes.  It does
 * that using an XPathNamespaces object, which is nothing more than table of
 * prefix => URI pairs.  Any prefixes not found in the table when the
 * expression is being compiled results in a compilation error.
 *
 * The XPathNamespaces table can be constructed in two ways:
 *
 *  -# Create an empty table using the function
 *     XPathNamespaces::Make(XPathNamespaces*&) and just add all available
 *     prefix => URI pairs.  The set of available prefix => URI pairs could
 *     for instance be identified by finding all namespace declarations in
 *     scope at a certain element in the document.
 *  -# Create a table prefilled with all namespace prefixes found in the XPath
 *     expression using the function
 *     XPathNamespaces::Make(XPathNamespaces*&,const uni_char*) and then set
 *     the appropriate URI for each prefix by iterating through the prefixes.
 *
 * Either way, if any prefix is encountered in the expression for which no URI
 * has been set in the XPathNamespaces object compilation fails.
 *
 * The XPathNamespaces object can be used to compile any number of expressions
 * (though if strategy 2 above is used to create it, the set of namespace
 * prefix => URI pairs is of course tailored for a particular expression).
 * When the XPathNamespaces object is not needed any more, it must be freed by
 * calling the function XPathNamespaces::Free.
 *
 * @subsection step2 2. Compile the expression
 *
 * After taking care of namespace prefixes, the expression (a unicode string)
 * is compiled into a XPathExpression object by calling the function
 * XPathExpression::Make.  The arguments are the expression source and an
 * XPathNamespaces object to use for namespace prefix lookup.
 *
 * @subsection step3 3. Evaluate the expression
 *
 * Once an expression has been compiled and a XPathExpression object has been
 * created, the expression can be evaluated using the function
 * XPathExpression::Evaluate.  A compiled expression can be evaluated any
 * number of times with different context nodes (or the same context node).
 * Compiled expressions use fairly little memory, so caching them can be a
 * good idea.  On the other hand, compiling an expression is not a very heavy
 * operation either, so caching them is probably not very necessary (unless,
 * of course, the expression is expected to be evaluated very often).
 *
 * @subsection step4 4. Cleaning up
 *
 * All XPathNamespaces, XPathExpression, XPathResult and XPathNode objects
 * created or returned during usage of the XPath module need to be freed by
 * calling the appropriate Free function (XPathNamespaces::Free,
 * XPathExpression::Free, XPathResult::Free or XPathNode::Free).  Objects can
 * always be freed in any order (even though one object was created using
 * another).  XPathResult objects can hold on to large amounts of memory (if
 * the result was a large node-set or a long string).
 *
 * @section result Results
 *
 * The result of a successful evaluation is represented by an XPathResult
 * object.  XPathResult object can be reused to hold the result of different
 * evaluations (but of course only one at a time).  The result can be of one
 * of four different basic types: number, string, boolean and node-set.
 *
 * @subsection resultSimple Number, string or booolean
 *
 * The first three are simple and can occur either because the expression's
 * actual result was of that type or because conversion to that type was
 * requested via the 'type' argument to XPathExpression::Evaluate.  The actual
 * values are accessed using the functions XPathResult::GetNumberValue,
 * XPathResult::GetStringValue and XPathResult::GetBooleanValue.  Numbers are
 * represented as <code>double</code>s and strings are represented as unicode
 * strings (owned by the XPathResult object).
 *
 * @subsection resultNodeset Node-sets
 *
 * The fourth basic result type is the node-set.  This is the major reason for
 * using XPath expressions at all (although they can be used for useful things
 * such as simple numeric calculations or string manipulation).
 *
 * There are three major variations of node-sets (single node, iterator and
 * snapshot) and two sub variations of each (ordered and unordered).  Which
 * type is returned is entirely dependent on the 'type' argument to
 * XPathExpression::Evaluate.  If ANY_TYPE is used and the actual result type
 * is a node-set, the unordered iterator type is returned.
 *
 * It is usually more efficient to request an unordered set instead of an
 * ordered one, since the resulting set may have to be sorted in the latter
 * case.  Sorting a large set can be very expensive.  The implementation tries
 * its very best to avoid sorting however, and the result of simple straight-
 * forward expression (such as "id('x')/foo[1]/bar") is almost always
 * automatically ordered, requiring no extra sorting.
 *
 * The single node variation (especially the unordered one) can be even more
 * efficient, since the evaluation can sometimes be terminated immediately
 * when the first contained node is found.
 *
 * The iterator type node-set may be generated by incrementally evaluating the
 * expression.  If it is not (and it is not in the current implementation),
 * there is no real difference between iterator type node-sets and snapshot
 * type node-sets.
 *
 * Results of number, string or boolean type cannot be converted into node
 * sets, but node-sets can be converted into number, string or boolean type
 * (as if the expression had been wrapped with "number(...)", "string(...)" or
 * "boolean(...)").
 *
 * @section changes Keeping up with changes in the document
 *
 * Node-sets represented by XPathResult objects are not live.  They are not
 * updated or invalidated if the document changes.  Elements referenced from
 * XPathNode objects in the node-set may even be deleted without the XPath
 * code noticing it.
 *
 * It is therefore not recommended to keep XPathResult or XPathNode objects
 * around while other things are going on unless the referenced elements are
 * kept alive or the result is manually invalidated and discarded when the
 * document changes.
 *
 * The DOM 3 XPath implementation, for instance, always copies the sequence of
 * nodes in a snapshot type node-set and protects them from garbage collection
 * manually and invalidates iterator type node-sets whenever the document is
 * modified (as prescribed by the DOM 3 XPath specification).
 *
 * It should be noted however that even after elements referenced from
 * XPathNode objects have been deleted, many operations on the XPathNode
 * objects are still valid and perfectly safe.  See the documentation for the
 * XPathNode class for more information.
 */

class TempBuffer;
class XMLExpandedName;
class XMLExpandedNameN;
class XMLCompleteName;
class XMLCompleteNameN;
class XMLNamespaceDeclaration;
class XPathExtensions;
class XPathContext;
class XPathPattern;

/** A collection of prefix/URI pairs used to translate the namespace
    prefixes used in an XPath expression to complete URIs.  If a
    prefix is encountered in an XPath expression that is not found in
    the XPathNamespaces collection supplied an error is signalled.

    All XPathNamespaces objects created must be freed using the
    XPathNamespaces::Free() function.  They can be reused any number
    of times. */
class XPathNamespaces
{
public:
	static void Free(XPathNamespaces *namespaces);
	/**< Deletes the XPathNamespaces object and frees all resources allocated
	     by it.

	     @param namespaces Object to free or NULL. */

	static OP_STATUS Make(XPathNamespaces *&namespaces, XMLNamespaceDeclaration *nsdeclaration);
	/**< Creates a collection containing the prefix/URI pairs of all namespace
	     declarations that are in scope, as defined by the supplied
	     XMLNamespaceDeclaration chain.  This function will not steal a
	     reference to 'nsdeclaration'; it will increment its reference count
	     if necessary.

	     @param namespaces Set to the newly created object.
	     @param nsdeclaration Namespace declarations in scope.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static OP_STATUS Make(XPathNamespaces *&namespaces);
	/**< Creates an empty collection.  This object should be filled with
	     prefix/URI pairs using the Add(const uni_char *, const uni_char *)
	     function and later passed to the XPathExpression::Make function.

	     @param namespaces Set to the newly created object.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS Add(const uni_char *prefix, const uni_char *uri) = 0;
	/**< Add a prefix/URI pair to the collection.  If a pair with the same
	     prefix is already present in the collection, the old pair is removed.

	     @param prefix Namespace prefix.  Can not be NULL.
	     @param uri Namespace URI.  Can not be NULL.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS Remove(const uni_char *prefix) = 0;
	/**< Remove a prefix/URI pair from the collection.

	     @param prefix Namespace prefix.

	     @return OpStatus::OK or OpStatus::ERR if the prefix was not found. */

	static OP_STATUS Make(XPathNamespaces *&namespaces, const uni_char *expression);
	/**< Creates a collection with the prefixes found in the given XPath
	     expression.  The prefixes should be given URIs using the GetCount(),
	     GetPrefix(unsigned) and SetURI(unsigned, const uni_char*) functions.

	     @param namespaces Set to the newly created object.
	     @param expression Expression to collect namespace prefixes from.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual unsigned GetCount() = 0;
	/**< Returns the number of pairs in the collection.

	     @return The number of prefix/URI pairs in the collection. */

	virtual const uni_char *GetPrefix(unsigned index) = 0;
	/**< Returns the prefix of the prefix/URI pair at the given position.

	     @param index The index of the pair.  Must be less than the number of
	                  pairs in the collection, as returned by GetCount().

	     @return A namespace prefix. */

	virtual const uni_char *GetURI(unsigned index) = 0;
	/**< Returns the URI of the prefix/URI pair at the given position.

	     @param index The index of the pair.  Must be less than the number of
	                  pairs in the collection, as returned by GetCount().

	     @return A namespace URI. */

	virtual const uni_char *GetURI(const uni_char *prefix) = 0;
	/**< Returns the URI of the prefix/URI pair with the given prefix, or NULL
	     if no such pair exists.

	     @param prefix A namespace prefix.  Can not be NULL.

	     @return A namespace URI. */

	virtual OP_STATUS SetURI(unsigned index, const uni_char *uri) = 0;
	/**< Sets the URI for the prefix/URI pair at the given position.  If an
	     URI has already been set for that pair, the old URI will be replaced.

	     @param index The index of the pair.  Must be less than the number of
	                  pairs in the collection, as returned by GetCount().
	     @param uri A namespace URI.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

protected:
	virtual ~XPathNamespaces();
	/**< Destructor.  Do not free XPathNamespace objects using the delete
	     operator, use the Free function. */
};

/** An XPath node object.  Can either be created using the Make, MakeAttribute
    or MakeNamespace functions and used as context nodes or be fetched from
    XPathResult objects as part of the result of evaluating an XPath
    expression.

    Node objects are not invalidated or updated in any way when the document
    to which they belong changes.  Such changes may make the node object
    invalid in which case the pointer returned by the GetElement function may
    be invalid.  The GetType, GetAttributeName, GetAttributeNsIdx,
    GetNamespacePrefix and GetNamespaceURI functions are however always safe
    to call (although the attribute or namespace declaration specified may
    have disappeared from the element at which they were found during the
    evaluation of the XPath expression).

    All XPathNode objects created or returned from XPathResult objects must be
    freed using the Free function.  XPathNode objects will never be returned
    more than once from any XPathNode or XPathResult function, and any number
    of XPathNode objects may refer to the same node at any time. */
class XPathNode
{
public:
	enum Type
	{
		ROOT_NODE,
		ELEMENT_NODE,
		TEXT_NODE,
		ATTRIBUTE_NODE,
		NAMESPACE_NODE,
		PI_NODE,
		COMMENT_NODE,

#ifdef XPATH_PATTERN_SUPPORT
		DOCTYPE_NODE,
		/**< Internal: only returned by a pattern that calls the
		     function "doctype" in the http://xml.opera.com/xslt
		     namespace. */
#endif // XPATH_PATTERN_SUPPORT

		LAST_NODE_TYPE
	};

	static void Free(XPathNode *node);
	/**< Deletes the XPathNode object and frees all resources allocated by
	     it.

	     @param node Object to free or NULL. */

	static OP_STATUS Make(XPathNode *&node, XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode);
	/**< Creates a node object for the given element.  The created node
	     will be of one of the types ROOT_NODE, ELEMENT_NODE, TEXT_NODE,
	     PI_NODE or COMMENT_NODE.

	     @param node Set to the newly created object.
	     @param tree Tree accessor for accessing the tree to which the node
	                 belongs.
	     @param treenode The tree accessor's representation of the node.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static OP_STATUS MakeAttribute(XPathNode *&node, XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode, const XMLCompleteName &name);
	/**< Creates an attribute node object for an attribute of the given
	     element.  The created node will be of the type ATTRIBUTE_NODE.  No
	     checking is performed to make sure the named attribute exists.

	     @param node Set to the newly created object.
	     @param tree Tree accessor for accessing the tree to which the node
	                 belongs.
	     @param treenode The tree accessor's representation of the element.
	                     Must be of the type XMLTreeAccessor::TYPE_ELEMENT.
	     @param name The attribute's name.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static OP_STATUS MakeNamespace(XPathNode *&node, XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode, const uni_char *prefix, const uni_char *uri);
	/**< Creates a namespace node object for a namespace declaration in scope
	     at the given element.  The created node will be of the type
	     NAMESPACE_NODE.  No checking is performed to make sure the named
	     namespace declaration exists.

	     @param node Set to the newly created object.
	     @param tree Tree accessor for accessing the tree to which the node
	                 belongs.
	     @param treenode The tree accessor's representation of the element.
	                     Must be of the type XMLTreeAccessor::TYPE_ELEMENT.
	     @param prefix The namespace declaration's prefix.
	     @param uri The namespace declaration's uri.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static OP_STATUS MakeCopy(XPathNode *&node, XPathNode *original);
	/**< Creates a node that is a copy of another node.

       @param node Set to the newly created object.
       @param original Node to copy.
       @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static BOOL IsSameNode(XPathNode *node1, XPathNode *node2);
	/**< Compares two node objects and returns TRUE if they represent the same
	     actual node.

	     @param node1 First node to compare.
	     @param node2 Second node to compare.

	     @return TRUE if the two nodes are the same. */

	virtual Type GetType() = 0;
	/**< Returns the type of the node.

	     @return The type of the node. */

	virtual XMLTreeAccessor *GetTreeAccessor() = 0;
	/**< Returns the tree accessor associated with this node.

	     @return An XMLTreeAccessor object.  Never NULL. */

	virtual XMLTreeAccessor::Node *GetNode() = 0;
	/**< Returns the tree accessor's representation of the node, or if the
	     type of the node is ATTRIBUTE_NODE, the tree accessor's
	     representation of the element to which the attribute belongs, or if
	     the type of the node is NAMESPACE_NODE, the tree accessor's
	     representation of the element at which the namespace declaration was
	     in scope (not necessarily where it was declared.)

	     @return An element.  Never NULL. */

	virtual void GetNodeName(XMLCompleteName &name) = 0;
	/**< Sets 'name' to the node's complete name.  For element and attribute
	     nodes, this is the element's och attribute's full name.  For all
	     other types of nodes, 'name' is left unchanged. */

	virtual void GetExpandedName(XMLExpandedName &name) = 0;
	/**< Retrieves the node's 'expanded-name' as defined by the XPath
	     specification.  If necessary, the strings are copied and owned by the
	     supplied XMLExpandedName object after the call, but only if
	     necessary.

	     @param name Assigned the node's 'expanded-name'.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             the node has no 'expanded-name' (ROOT_NODE, TEXT_NODE and
	             COMMENT_NODE). */

	virtual OP_STATUS GetStringValue(TempBuffer &value) = 0;
	/**< Retrieves the node's 'string-value' as defined by the XPath
	     specification.

	     @param value Buffer to which the node's 'string-value' is appended.
	                  The buffer is not cleared first.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual BOOL IsWhitespaceOnly() = 0;
	/**< Returns TRUE if the type of the node is TEXT_NODE and its
	     string-value matches the S production in XML 1.0/1.1.

	     @return TRUE if this is a white-space only text node. */

	virtual const uni_char *GetNamespacePrefix() = 0;
	/**< Returns the prefix of the namespace declaration if the type of the
	     node is NAMESPACE_NODE, otherwise returns NULL.  If the namespace
	     declaration is a default namespace declaration, an empty string is
	     returned.

	     @return An namespace prefix or NULL. */

	virtual const uni_char *GetNamespaceURI() = 0;
	/**< Returns the URI of the namespace declaration if the type of the
	     node is NAMESPACE_NODE, otherwise returns NULL.

	     @return An namespace URI or NULL. */

protected:
	virtual ~XPathNode();
	/**< Destructor.  Do not free XPathNode objects using the delete
	     operator, use the Free function. */
};


#ifdef XPATH_NODE_PROFILE_SUPPORT

/** An XPath node profile object represents a rough model of what kind of
    nodes an expression can return.  The objects can be fetched from
    XPathExpression objects (and XPathPattern objects) and are always owned by
    the object from which they are fetched.  They can also be created
    explictly for a certain node type, in which case they are owned by the
    creator. */
class XPathNodeProfile
{
public:
	static OP_STATUS Make(XPathNodeProfile *&profile, XPathNode::Type type);
	/**< Creates a node profile including all nodes of the specified type.
	     The node profile is owned by the caller.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static void Free(XPathNodeProfile *profile);
	/**< Destroys a node profile previously created by a call to Make().
	     Can typically not be used on node profiles from other sources! */

	virtual BOOL Includes(XPathNode *node) = 0;
	/**< Returns TRUE if this profile includes the specified node.  This
	     means, for an expression, that it is not impossible for the
	     expression to have returned the node.  It is no guarantee, however,
	     that it has or could have done so.  FALSE is returned if it is
	     impossible for the expression to have returned the node.

	     @param node Node to query about.
	     @return TRUE or FALSE. */

protected:
	virtual ~XPathNodeProfile();
	/**< Destructor.  Objects are owned by the object from which it was
	     returned, and is destroyed together with it. */
};

#endif // XPATH_NODE_PROFILE_SUPPORT


#ifdef XPATH_EXTENSION_SUPPORT
# define XPATH_INCLUDE_XPATHEXTENSIONS
# include "modules/xpath/xpathextensions.h"
# undef XPATH_INCLUDE_XPATHEXTENSIONS
#endif // XPATH_EXTENSION_SUPPORT


/** An XPath expression object used to evaluate XPath expressions.

    To support namespace prefixes in the expression, first create an
    XPathNamespaces object (either create an empty and just add all supported
    namespace prefixes to it or create one from the expression source and the
    set a namespace URI for each namespace prefix found in the expression
    source).

    This interface corresponds to the DOM interface
      <a href="http://www.w3.org/TR/2004/NOTE-DOM-Level-3-XPath-20040226/xpath.html#XPathExpression">XPathExpression</a>
    in DOM 3 XPath.

    All XPathExpression objects created must be freed using the
    XPathExpression::Free() function.  They can be reused (that is, evaluated)
    any number of times. */
class XPathExpression
{
public:
	static void Free(XPathExpression *expression);
	/**< Deletes the XPathExpression object and frees all resources
	     allocated by it.

	     @param expression Object to free or NULL. */

	/** Expression data structure, used when creating XPathExpression objects.
	    All referenced objects are owned by the calling code (that is, not the
	    xpath module.) */
	class ExpressionData
	{
	public:
		ExpressionData();
		/**< Constructors.  Sets all members to NULL. */

		const uni_char *source;
		/**< Source code of the expression.  Cannot be NULL. */

		XPathNamespaces *namespaces;
		/**< Collection of prefix:uri bindings available.  Can be NULL. */

#ifdef XPATH_EXTENSION_SUPPORT
		XPathExtensions *extensions;
		/**< Collection of extension functions and variables available.  Can
		     be NULL. */
#endif // XPATH_EXTENSION_SUPPORT

#ifdef XPATH_ERRORS
		OpString *error_message;
		/**< If non-NULL, storage used to generate error message for errors
		     detected during parsing or compilation of expression.  The string
		     will be cleared first if it is not empty.  If no error is
		     detected, the string is left unchanged.  The error message is
		     typically an unformatted single line string. */
#endif // XPATH_ERRORS
	};

	static OP_STATUS Make(XPathExpression *&expression, const ExpressionData &data);
	/**< Creates an XPathExpression object from the information stored in
	     'data'.  If API_XPATH_ERRORS has been imported and errors are
	     detected, an error message is generated into 'data.error_message' if
	     non-NULL.  If NULL, no error message is generated, but error
	     detection is of course unaffected.

	     The created expression is owned by the caller and must be freed using
	     the XPathExpression::Free() function.  The expression object itself
	     is stateless, and can be used any number of times, even multiple
	     times in parallell.

	     @param expression Set to the created XPathExpression object.
	     @param data Expression data.

	     @return OpStatus::OK, OpStatus::ERR if an error is detected and
	             OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_STATUS DEPRECATED(Make(XPathExpression *&expression, const uni_char *source, XPathNamespaces *namespaces));
	/**< Creates an XPathExpression object from the given string using the
	     given XPathNamespaces object to resolve any namespace prefixes.

	     If parsing fails, OpStatus::ERR is returned.

	     @param expression Set to the created XPathExpression object.
	     @param source The expression source string.
	     @param namespaces Collection of namespace prefix/URI pairs.

	     @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY. */

#ifdef XPATH_EXTENSION_SUPPORT
	static OP_STATUS DEPRECATED(Make(XPathExpression *&expression, const uni_char *source, XPathNamespaces *namespaces, XPathExtensions *extensions));
	/**< Creates an XPathExpression object from the given string using the
	     given XPathNamespaces object to resolve any namespace prefixes and
	     the given XPathExtensions object to resolve variable references and
	     calls to unknown functions.

	     If parsing fails, OpStatus::ERR is returned.

	     @param expression Set to the created XPathExpression object.
	     @param source The expression source string.
	     @param namespaces Collection of namespace prefix/URI pairs.
	     @param extensions Collection of extension functions and variables.

	     @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY. */
#endif // XPATH_EXTENSION_SUPPORT

	enum Flags
	{
		FLAG_NEEDS_CONTEXT_SIZE = 1,
		/**< This expression needs to know the top-level context size.  This
		     is true for the expression "last()" and variants, and possibly
		     functions that call extension functions, if those claim to need
		     to know the context size. */

		FLAG_NEEDS_CONTEXT_POSITION = 2
		/**< Like FLAG_NEEDS_CONTEXT_SIZE but for context position.  This is
		     true for the expression "position()" and variants. */
	};

	virtual unsigned GetExpressionFlags() = 0;
	/**< Returns zero or more flags from Flags OR:ed together, signalling
	     properties of the expression.

	     @return A flag bitfield. */

	/** State object for evaluating an XPath expression.

	    The object is never owned by the XPath module and must be destroyed
	    using the Free() function.  Destroying an Evaluate object before the
	    evaluation has finished means cancelling the evaluation. */
	class Evaluate
	{
	public:
		static OP_STATUS Make(Evaluate *&evaluate, XPathExpression *expression);

		static void Free(Evaluate *evaluate);
		/**< Deletes the Evaluate object and frees all resources owned by
		     it.

		     @param evaluate Object to free or NULL. */

		virtual void SetContext(XPathNode *node, unsigned position = 1, unsigned size = 1) = 0;
		/**< Define the context of the evaluation.  All evaluations initiated
		     by code external to the XPath module must have a context set;
		     evaluating with no context node is undefined.  Evaluate objects
		     created by the XPath engine and supplied to extension functions
		     as the function call arguments however have a context set by
		     default.

		     The Evaluate object assumes ownership of the context node. */

#ifdef XPATH_EXTENSION_SUPPORT
		virtual void SetExtensionsContext(XPathExtensions::Context *context) = 0;
		/**< Set the extensions context for this evaluation.  Will be passed
		     to any XPathFunction or XPathVariable implementation used during
		     the evaluation.  The object is owned by the calling code and will
		     not be deallocated or otherwise manipulated by the XPath engine.

		     If not set, or set to NULL, the corresponding argument to
		     extension implementations will be NULL. */
#endif // XPATH_EXTENSION_SUPPORT

		enum Type
		{
			TYPE_INVALID = 0,

			PRIMITIVE_NUMBER = 0x01,
			PRIMITIVE_BOOLEAN = 0x02,
			PRIMITIVE_STRING = 0x04,

			NODESET_SINGLE = 0x10,
			/**< Retrieve only a single node.  If an ordered result is
			     requested, the first node in document order is returned,
			     otherwise any node in the result is returned (though often
			     the first node found is the first in document order.)

			     The node must be retrieved using Evalute::GetSingleNode(). */
			NODESET_ITERATOR = 0x20,
			/**< Retrieve nodes one by one.  If possible, evaluation will be
			     progress only far enough to return the next node.  At any
			     time, the evaluation can be terminated by destroying the
			     Evaluate object.

			     Nodes must be retrieved using Evaluate::GetNextNode(). */
			NODESET_SNAPSHOT = 0x40,
			/**< Evaluate the expression completely and provide random access
			     to the nodes in the result.  Likely has slightly smaller
			     overhead than the iterative evaluation of NODESET_ITERATOR,
			     but uses more memory as all nodes in the resulting node-set
			     needs to be alive at the same time.

			     The number of nodes in the result is retrieved using
			     Evaluate::GetNodesCount() and the nodes must be retrieved
			     using Evaluate::GetNode(). */

			NODESET_FLAG_ORDERED = 0x100
			/**< Flag that, when combined with NODESET_SINGLE,
			     NODESET_ITERATOR or NODESET_SNAPSHOT, means that nodes should
			     be returned in document order.  When not used, nodes could be
			     returned out of order.  In some cases, ordering means all
			     nodes need to be collected before the first can be returned,
			     and expensive sorting must be performed.  Usually it doesn't
			     make much of a difference, though.

			     Note that PRIMITIVE_NUMBER and PRIMITIVE_STRING both imply
			     this flag automatically, since the string value of the first
			     node in document order is used to produce the primitive
			     value. */
		};

		virtual void SetRequestedType(unsigned type) = 0;
		/**< Set the requested result type.  The result will be converted to
		     the specified type if possible.  Such a conversion is always
		     possible if the requested result type is primitive, whereas if
		     the requested result type is a node-set, the actual result must
		     be a node-set.  If the conversion is not possible, the evaluation
		     will fail.

		     This function can be called any number of times before or during
		     evaluation, but once evaluation has started, the requested type
		     cannot differ from the previous call. */

		virtual void SetRequestedType(Type when_number, Type when_boolean, Type when_string, unsigned when_nodeset) = 0;
		/**< Set the requested result type depending on the actual result
		     type.  The types 'when_number', 'when_boolean' and 'when_string'
		     should be primitive, since converting a primitive actual result
		     into a node-set is not possible.  The type 'when_nodeset' can be
		     any type.  If any of the arguments are zero (TYPE_INVALID,) such
		     an actual result is considered invalid and causes the evaluation
		     to fail, rather than being converted into another type.

		     This function can be called any number of times before or during
		     evaluation, but once evaluation has started, the requested type
		     cannot differ from the previous call. */

		virtual void SetCostLimit(unsigned limit) = 0;
		/**< Sets a limit on how much work a single call one of the functions
		     CheckResultType(), GetNumberResult(), GetBooleanResult(),
		     GetStringResult(), GetSingleNode(), GetNextNode() and
		     GetNodesCount() can perform.  If the limit is reached before the
		     called function finished, it will pause and return
		     OpBoolean::IS_FALSE.  An excessively low value will increase
		     over-head, but anything in the range 512-1024 and up will
		     probably achieve completely acceptable speed and over-head.  The
		     default cost limit is controlled by a tweak. */

		virtual unsigned GetLastOperationCost() = 0;
		/**< Returns the cost of the last call to one of the functions listed
		     in the description of SetCostLimit(). */

		virtual OP_BOOLEAN CheckResultType(unsigned &type) = 0;
		/**< Check the (actual or converted) type of the result.  Need only be
		     called if no conversion was requested, or different conversions
		     were requested depending on the actual type of the result.  Can
		     always be called, however.

		     If OpBoolean::IS_FALSE is returned, evaluation was paused.  See
		     this class' documentation for more details on what that means and
		     how to handle it.  This function can be called any number of
		     times after it has returned OpBoolean::IS_TRUE, and will never
		     return anything but OpBoolean::IS_TRUE if it has returned it
		     once. */

		virtual OP_BOOLEAN GetNumberResult(double &result) = 0;
		/**< Retrieve the result as a number.  Only valid if such a conversion
		     was requested, or CheckResultType() was called and reported that
		     the result is a number.

		     If OpBoolean::IS_FALSE is returned, evaluation was paused.  See
		     this class' documentation for more details on what that means and
		     how to handle it. */

		virtual OP_BOOLEAN GetBooleanResult(BOOL &result) = 0;
		/**< Retrieve the result as a boolean.  Only valid if such a
		     conversion was requested, or CheckResultType() was called and
		     reported that the result is a boolean.

		     If OpBoolean::IS_FALSE is returned, evaluation was paused.  See
		     this class' documentation for more details on what that means and
		     how to handle it. */

		virtual OP_BOOLEAN GetStringResult(const uni_char *&result) = 0;
		/**< Retrieve the result as a string.  Only valid if such a conversion
		     was requested, or CheckResultType() was called and reported that
		     the result is a string.  The returned string is owned by this
		     object and will be valid until this object is destroyed.

		     If OpBoolean::IS_FALSE is returned, evaluation was paused.  See
		     this class' documentation for more details on what that means and
		     how to handle it. */

		virtual OP_BOOLEAN GetSingleNode(XPathNode *&node) = 0;
		/**< Retrieve the result as a single node.  Only valid if the actual
		     result was a node-set, and the NODESET_SINGLE type was requested
		     in that case.  If the node-set was empty, node will be NULL.  The
		     returned node object is owned by the Evaluate object.

		     If OpBoolean::IS_FALSE is returned, evaluation was paused.  See
		     this class' documentation for more details on what that means and
		     how to handle it. */

		virtual OP_BOOLEAN GetNextNode(XPathNode *&node) = 0;
		/**< Retrieve the next node in the result.  Only valid if the actual
		     result was a node-set, and the NODESET_ITERATOR type was
		     requested in that case.  The returned node object is owned by the
		     evaluate object and has to be copied if it
		     is needed past next call to GetNextNode

		     If OpBoolean::IS_FALSE is returned, evaluation was paused.  See
		     this class' documentation for more details on what that means and
		     how to handle it. */

		virtual OP_BOOLEAN GetNodesCount(unsigned &count) = 0;
		/**< Retrieve the number of nodes in the result.  Only valid if the
		     actual result was a node-set, and the NODESET_SNAPSHOT type was
		     requested in that case.

		     If OpBoolean::IS_FALSE is returned, evaluation was paused.  See
		     this class' documentation for more details on what that means and
		     how to handle it.  This function can be called any number of
		     times after it has returned OpBoolean::IS_TRUE, and will never
		     return anything bug OpBoolean::IS_TRUE if it has returned it
		     once. */

		virtual OP_STATUS GetNode(XPathNode *&node, unsigned index) = 0;
		/**< Retrieve the indext'th node in the result.  GetNodesCount() must
		     have been called at least once, and 'index' must be less than the
		     reported count.  The returned node object is owned by the evaluate
		     object.

		     Since this function can only be used when the evaluation has
		     finished, it never returns OpBoolean::IS_FALSE like the other
		     functions. */

#ifdef XPATH_ERRORS
		virtual const uni_char *GetErrorMessage() = 0;
		/**< Returns a string describing the failure if the evaluation failed.
		     If the evaluation hasn't failed (yet) NULL is returned.  The
		     string is owned by the evaluate object and will remain valid
		     until it is destroyed.

		     @return Error message or NULL. */
#endif // XPATH_ERRORS

	protected:
		virtual ~Evaluate();
		/**< Destructor.  Do not free Evaluate objects using the delete
		     operator, use the Free function. */
	};

#ifdef XPATH_NODE_PROFILE_SUPPORT
	virtual OP_STATUS GetNodeProfile(XPathNodeProfile *&profile) = 0;
	/**< Calculates a node profile representing the nodes this expression can
	     return.  If it is unknown, or if this expression cannot return nodes
	     (its intrinsic value is a primitive) then 'profile' is set to NULL,
	     otherwise 'profile' is set to a node profile object owned by this
	     expression.

	     @param profile Set to a node profile on success, if possible.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */
#endif // XPATH_NODE_PROFILE_SUPPORT

protected:
	virtual ~XPathExpression();
	/**< Destructor.  Do not free XPathExpression objects using the delete
	     operator, use the Free function. */
};


#ifdef XPATH_EXTENSION_SUPPORT
# define XPATH_INCLUDE_XPATHVALUE
# define XPATH_INCLUDE_XPATHFUNCTION
# define XPATH_INCLUDE_XPATHVARIABLE
# include "modules/xpath/xpathextensions.h"
# undef XPATH_INCLUDE_XPATHVALUE
# undef XPATH_INCLUDE_XPATHFUNCTION
# undef XPATH_INCLUDE_XPATHVARIABLE
#endif // XPATH_EXTENSION_SUPPORT


#ifdef XPATH_PATTERN_SUPPORT
# define XPATH_INCLUDE_XPATHPATTERN
# include "modules/xpath/xpathpattern.h"
# undef XPATH_INCLUDE_XPATHPATTERN
#endif // XPATH_PATTERN_SUPPORT

/** Container class for utility functions. */
class XPath
{
public:
	static BOOL IsSupportedFunction (const XMLExpandedName &name);
	/**< Returns TRUE if 'name' is the name of a supported builtin function.

	     @param name Name of function.

	     @return TRUE or FALSE. */
};

#endif // XPATH_SUPPORT
#endif // XPATH_H
