/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLNAMES_H
#define XMLNAMES_H

#include "modules/logdoc/namespace.h"

#define XMLNSURI_XML UNI_L("http://www.w3.org/XML/1998/namespace")
#define XMLNSURI_XML_LENGTH 36

#define XMLNSURI_XMLNS UNI_L("http://www.w3.org/2000/xmlns/")
#define XMLNSURI_XMLNS_LENGTH 29

class XMLExpandedNameN;
class XMLCompleteNameN;

/** Class representing an expanded name (namespace URI + local part)
    consisting of null-terminated strings.

    The constructors will set the object up so that the object does
    not own the strings.  To create a persistent object that copies
    and owns the strings, use one of the Set() or SetL() functions. */
class XMLExpandedName
{
protected:
	uni_char *uri, *localpart;
	BOOL owns_strings;

	void Free(), Reset();

public:
	XMLExpandedName();
	/**< Default constuctor.  Sets both URI and local part to NULL. */

	XMLExpandedName(const uni_char *localpart);
	/**< Constructor.  Sets URI to NULL.

	     @param localpart Local part.  Can be NULL. */

	XMLExpandedName(const uni_char *uri, const uni_char *localpart);
	/**< Constructor.

	     @param uri Namespace URI.  Can be NULL.
	     @param localpart Local part.  Can be NULL. */

	XMLExpandedName(const XMLExpandedName &name);
	/**< Copy constructor.

	     @param name Name to copy. */

	XMLExpandedName(HTML_Element *element);
	/**< Constructor that sets URI and local part to represent the
	     element's expanded name.

	     @param element Element to fetch name from. */

	XMLExpandedName(NS_Element *nselement, const uni_char *localpart);
	/**< Constructor that gets the namespace URI from a NS_Element
	     object.

	     @param nselement NS_Element that provides the namespace
	                      URI.
	     @param localpart Local part. */

	~XMLExpandedName();
	/**< Destructor.  Frees the strings if the object owns them. */

	const uni_char *GetUri() const  { return uri; }
	/**< Returns the name's namespace URI component.

	     @return A string or NULL. */

	const uni_char *GetLocalPart() const  { return localpart; }
	/**< Returns the name's local part.

	     @return A string or NULL. */

	NS_Type GetNsType() const;
	int GetNsIndex() const;

	BOOL IsXHTML() const;
	/**< Returns TRUE if the namespace URI is the namespace URI for
	     XHTML (http://www.w3.org/1999/xhtml.)

	     @return TRUE or FALSE. */

	BOOL IsXML() const;
	/**< Returns TRUE if the namespace URI is the namespace URI for
	     XML (http://www.w3.org/XML/1998/namespace.)

	     @return TRUE or FALSE. */

	BOOL IsXMLNamespaces() const;
	/**< Returns TRUE if the namespace URI is the namespace URI for
	     XML Namespaces (http://www.w3.org/2000/xmlns/.)

	     @return TRUE or FALSE. */

#ifdef SVG_SUPPORT
	BOOL IsSVG() const;
	/**< Returns TRUE if the namespace URI is the namespace URI for
	     SVG (http://www.w3.org/2000/svg.)

	     @return TRUE or FALSE. */
#endif // SVG_SUPPORT
#ifdef _WML_SUPPORT_
	BOOL IsWML() const;
	/**< Returns TRUE if the namespace URI is the namespace URI for
	     WML (http://www.wapforum.org/2001/wml.)

	     @return TRUE or FALSE. */
#endif // _WML_SUPPORT_
#ifdef XSLT_SUPPORT
	BOOL IsXSLT() const;
	/**< Returns TRUE if the namespace URI is the namespace URI for
	     XSLT (http://www.w3.org/1999/XSL/Transform.)

	     @return TRUE or FALSE. */
#endif // XSLT_SUPPORT

	BOOL IsId(const XMLExpandedName &elementname) const;
	/**< Returns TRUE if an attribute with this expanded name on an
	     element with the expanded name in 'elementname' is known to
	     be an ID attribute without being declared as such in the DTD.
	     This true for the attribute 'xml:id' and attributes in the
	     null namespace, named 'id' on elements with any name in the
	     XHTML, SVG or WML and on elements in the XSLT namespace with
	     the local part 'stylesheet' or 'transform'.

	     @param elementname Expanded name of an element.
	     @return TRUE if the attribute is an ID attribute. */

	BOOL operator==(const XMLExpandedName &other) const;
	BOOL operator==(const XMLExpandedNameN &other) const;
	BOOL operator!=(const XMLExpandedName &other) const { return !(*this == other); }
	BOOL operator!=(const XMLExpandedNameN &other) const { return !(*this == other); }

	XMLExpandedName &operator=(const XMLExpandedName &other);

	OP_STATUS Set(const XMLExpandedName &other);
	OP_STATUS Set(const XMLExpandedNameN &other);
	void SetL(const XMLExpandedName &other);
	void SetL(const XMLExpandedNameN &other);

	class HashFunctions
		: public OpHashFunctions
	{
	public:
		virtual UINT32 Hash(const void *key);
		virtual BOOL KeysAreEqual(const void *key1, const void *key2);
	};
};

class XMLCompleteName
	: public XMLExpandedName
{
protected:
	uni_char *prefix;

	void Free(), Reset();
	OP_STATUS CopyStrings();

public:
	XMLCompleteName();
	XMLCompleteName(const uni_char *localpart);
	XMLCompleteName(const uni_char *uri, const uni_char *prefix, const uni_char *localpart);
	XMLCompleteName(const XMLExpandedName &name);
	XMLCompleteName(const XMLCompleteName &name);
	XMLCompleteName(HTML_Element *element);
	XMLCompleteName(NS_Element *nselement, const uni_char *localpart);
	~XMLCompleteName();

	const uni_char *GetPrefix() const  { return prefix; }

	int GetNsIndex() const;

	BOOL operator==(const XMLCompleteName &other) const;
	BOOL operator==(const XMLCompleteNameN &other) const;
	BOOL operator!=(const XMLCompleteName &other) const { return !(*this == other); }
	BOOL operator!=(const XMLCompleteNameN &other) const { return !(*this == other); }

	BOOL SameQName(const XMLCompleteName &other) const;
	/**< Returns TRUE if the prefix and local part of this name and
	     'other' are equal, disregarding the URI. */

	XMLCompleteName &operator=(const XMLExpandedName &other);
	XMLCompleteName &operator=(const XMLCompleteName &other);

	OP_STATUS Set(const XMLExpandedName &other);
	OP_STATUS Set(const XMLExpandedNameN &other);
	OP_STATUS Set(const XMLCompleteName &other);
	OP_STATUS Set(const XMLCompleteNameN &other);
	void SetL(const XMLExpandedName &other);
	void SetL(const XMLExpandedNameN &other);
	void SetL(const XMLCompleteName &other);
	void SetL(const XMLCompleteNameN &other);

	OP_STATUS SetUri(const uni_char *new_uri);
	OP_STATUS SetPrefix(const uni_char *new_prefix);
};

class XMLExpandedNameN
{
protected:
	const uni_char *uri, *localpart;
	unsigned uri_length, localpart_length;

public:
	XMLExpandedNameN();
	XMLExpandedNameN(const uni_char *uri, unsigned uri_length, const uni_char *localpart, unsigned localpart_length);
	XMLExpandedNameN(const XMLExpandedName &other);
	XMLExpandedNameN(const XMLExpandedNameN &other);

	const uni_char *GetUri() const { return uri; }
	unsigned GetUriLength() const { return uri_length; }
	const uni_char *GetLocalPart() const { return localpart; }
	unsigned GetLocalPartLength() const { return localpart_length; }

	NS_Type GetNsType() const;
	int GetNsIndex() const;

	BOOL IsXHTML() const;
	BOOL IsXML() const;
	BOOL IsXMLNamespaces() const;
#ifdef SVG_SUPPORT
	BOOL IsSVG() const;
#endif // SVG_SUPPORT
#ifdef _WML_SUPPORT_
	BOOL IsWML() const;
#endif // _WML_SUPPORT_
#ifdef XSLT_SUPPORT
	BOOL IsXSLT() const;
#endif // XSLT_SUPPORT

	BOOL IsId(const XMLExpandedNameN &elementname) const;

	BOOL operator==(const XMLExpandedName &other) const { return other == *this; }
	BOOL operator==(const XMLExpandedNameN &other) const;
	BOOL operator!=(const XMLExpandedName &other) const { return !(*this == other); }
	BOOL operator!=(const XMLExpandedNameN &other) const { return !(*this == other); }

	void SetUri(const uni_char *uri, unsigned uri_length);

	class HashFunctions
		: public OpHashFunctions
	{
	public:
		virtual UINT32 Hash(const void *key);
		virtual BOOL KeysAreEqual(const void *key1, const void *key2);
	};
};

class XMLCompleteNameN
	: public XMLExpandedNameN
{
protected:
	const uni_char *prefix;
	unsigned prefix_length;

public:
	XMLCompleteNameN();
	XMLCompleteNameN(const uni_char *qname, unsigned qname_length);
	XMLCompleteNameN(const uni_char *uri, unsigned uri_length, const uni_char *prefix, unsigned prefix_length, const uni_char *localpart, unsigned localpart_length);
	XMLCompleteNameN(const XMLCompleteName &other);
	XMLCompleteNameN(const XMLCompleteNameN &other);

	const uni_char *GetPrefix() const { return prefix; }
	unsigned GetPrefixLength() const { return prefix_length; }

	int GetNsIndex() const;

	BOOL operator==(const XMLCompleteName &other) const { return other == *this; }
	BOOL operator==(const XMLCompleteNameN &other) const;
	BOOL operator!=(const XMLCompleteName &other) const { return !(*this == other); }
	BOOL operator!=(const XMLCompleteNameN &other) const { return !(*this == other); }

	BOOL SameQName(const XMLCompleteNameN &other) const;
	/**< Returns TRUE if the prefix and local part of this name and
	     'other' are equal, disregarding the URI. */
};

/** Class representing a namespace declaration.  A namespace
    declaration is a mapping from a namespace prefix to a namespace
    URI.

    XMLNamespaceDeclaration objects are elements in a single linked
    list representing all namespace declarations in scope at some
    point.  Each object references the previous object (the object
    that represents the declaration that was the most recent
    declaration when the next object was created.)  These references
    are never reset, and each object keeps the previous object, and
    thus the entire chain of previous declarations, alive while it is
    alive.

    Objects of this class are reference counted.  The reference count
    can be manipulated manually using the IncRef() and DecRef()
    functions, or automatically by the "auto pointer" style class
    Reference.  The latter is probably less error prone. */
class XMLNamespaceDeclaration
{
protected:
	XMLNamespaceDeclaration *previous, *defaultns;
	uni_char *uri, *prefix;
	unsigned level, refcount;

	XMLNamespaceDeclaration(XMLNamespaceDeclaration *previous, unsigned level);
	~XMLNamespaceDeclaration();

public:
	/** Class for managing a reference to a XMLNamespaceDeclaration
	    object.  Will automatically release the reference when
	    destroyed. */
	class Reference
	{
	protected:
		XMLNamespaceDeclaration *declaration;

	public:
		Reference() : declaration(0) {}
		/**< Default constructor.  Initializes the reference to
		     NULL. */

		Reference(XMLNamespaceDeclaration *declaration) : declaration(XMLNamespaceDeclaration::IncRef(declaration)) {}
		/**< Constructor.  Increments the declaration's reference
		     count by one.

		     @param declaration Declaration to reference. */

		Reference(const Reference &reference) : declaration(XMLNamespaceDeclaration::IncRef(reference.declaration)) {}
		/**< Copy constructor.  Copies another reference and
		     increments the referenced declaration's reference count
		     by one.

		     @param reference Reference to copy. */

		~Reference() { XMLNamespaceDeclaration::DecRef(declaration); }
		/**< Destructor.  Decrements the referenced declaration's
		     reference count by one. */

		XMLNamespaceDeclaration *operator= (XMLNamespaceDeclaration *declaration);
		/**< Assignment operator.  Decrements the currently referenced
		     declaration's reference count and increments the new
		     declaration's.  It is safe to assign the same declaration
		     as is currently referenced or a declaration that
		     references or is referenced by the currently referenced
		     declaration.

		     @param declaration Declaration to assign.
		     @return The newly referenced declaration. */

		XMLNamespaceDeclaration *operator= (const Reference &reference) { return *this = reference.declaration; }
		/**< Copy assignment operator.  Decrements the currently
		     referenced declaration's reference count and increments
		     the new declaration's.  It is safe to assign the same
		     declaration as is currently referenced or a declaration
		     that references or is referenced by the currently
		     referenced declaration.

		     @param reference Reference to copy.
		     @return The newly referenced declaration. */

		XMLNamespaceDeclaration &operator* () const { return *declaration; }
		/**< Dereferencing operator.

		     @return A reference to the reference declaration. */

		XMLNamespaceDeclaration *operator-> () const { return declaration; }
		/**< Member access operator.

		     @return The referenced declaration. */

		operator XMLNamespaceDeclaration *() const { return declaration; }
		/**< Conversion operator.  Converts the reference to a pointer
		     to the referenced declaration. */
	};

	XMLNamespaceDeclaration *GetPrevious() const { return previous; }
	/**< Returns the previous namespace declaration from this one or
	     NULL if there is none.

	     @return A namespace declaration object or NULL. */

	const uni_char *GetUri() const { return uri; }
	/**< Returns this namespace declaration's URI.  If the URI is
	     NULL, this declaration represents an attribute used to
	     undeclare a namespace prefix.

	     @return A string or NULL. */

	const uni_char *GetPrefix() const { return prefix; }
	/**< Returns this namespace declaration's prefix or NULL if this
	     is a default namespace declaration.

	     @return A string or NULL. */

	unsigned GetLevel() const { return level; }
	/**< Returns the level specified when this namespace declaration
	     was pushed.

	     @return This declaration's level. */

	NS_Type GetNsType();
	/**< Returns the namespace type or NS_NONE if the namespace URI is
	     not known.

	     @return A namespace type. */

	int GetNsIndex();
	/**< Returns the namespace index of this namespace declaration.
	     Note: this will allocate a namespace index if one does not
	     already exist, so use only if necessary.

	     @return A namespace index. */

	static BOOL IsDeclared(XMLNamespaceDeclaration *declaration, const uni_char *prefix, const uni_char *uri);
	/**< Returns TRUE if 'prefix' is currently declared and the URI
	     associated with it equals 'uri'. */

	static unsigned CountDeclaredPrefixes(XMLNamespaceDeclaration *declaration);
	/**< Returns the number of unique declared prefixes.  The default
	     namespace declaration, if present, is not counted. */

	static XMLNamespaceDeclaration *FindDeclaration(XMLNamespaceDeclaration *current, const uni_char *prefix, unsigned prefix_length = ~0u);
	/**< Returns the most recent namespace declaration for the prefix.
	     If 'prefix' is NULL it returns the most recent default
	     namespace declaration.

	     @param current The most recent namespace declaration or NULL
	                    if no namespaces have been declared.
	     @param prefix A namespace prefix or NULL.
	     @param prefix_length The length of prefix, or ~0u to use
	                          uni_strlen to calculate it.
	     @return A namespace declaration or NULL if not found. */

	static XMLNamespaceDeclaration *FindDeclarationByIndex(XMLNamespaceDeclaration *declaration, unsigned index);
	/**< Return the last declaration declaring the index:th unique
	     declared prefix. */

	static XMLNamespaceDeclaration *FindDefaultDeclaration(XMLNamespaceDeclaration *current, BOOL return_undeclaration = TRUE);
	/**< Return the last declaration declaring the default namespace.
	     If 'return_undeclaration' is FALSE and the declaration that
	     would be returned has a NULL URI and thus undeclares the
	     default namespace, NULL is returned instead.

	     @param current The most recent namespace declaration or NULL
	                    if no namespaces have been declared.
	     @param return_undeclaration TRUE if a declaration that
	                                 undeclares the default namespace
	                                 should be returned, FALSE if NULL
	                                 should be returned instead in
	                                 that case.

	     @return A namespace declaration or NULL. */

	static const uni_char *FindUri(XMLNamespaceDeclaration *current, const uni_char *prefix, unsigned prefix_length = ~0u);
	/**< Returns the URI currently associated with the prefix.  If
	     'prefix' is NULL it returns URI currently declared as the
	     default namespace.

	     @param current The most recent namespace declaration or NULL
	                    if no namespaces have been declared.
	     @param prefix A namespace prefix or NULL.
	     @param prefix_length The length of 'prefix,' or ~0u to use
	                          uni_strlen to calculate it.
	     @return A namespace URI or NULL if not found or if the most
	             recent namespace declaration for the prefix
	             undeclared the prefix. */

	static const uni_char *FindDefaultUri(XMLNamespaceDeclaration *current);

	static const uni_char *FindPrefix(XMLNamespaceDeclaration *current, const uni_char *uri, unsigned uri_length = ~0u);
	/**< Returns the prefix of the most recent namespace declaration
	     in scope with the URI 'uri'.  Note that this function will
	     skip any namespace declarations in the list, with a matching
	     URI, but whose prefix has been undeclared or redeclared by
	     more recent namespace declarations.  That is, the namespace
	     prefix returned by this function, if any, is guaranteed to
	     resolve to the correct URI if passed to FindUri.

	     @param current The most recent namespace declaration or NULL
	                    if no namespaces have been declared.
	     @param uri A namespace URI.  Cannot be NULL.
	     @param uri_length The length of 'uri,' or ~0u to use uni_strlen
	                       to calculate it.
	     @return A namespace prefix or NULL if not found. */

	static OP_BOOLEAN ResolveName(XMLNamespaceDeclaration *current, XMLCompleteName &name, BOOL use_default = TRUE);
	/**< Resolves a complete name (prefix + local part) by filling in
	     the URI component.  Optionally (if 'use_default' is TRUE)
	     uses default namespace declarations when the prefix component
	     is NULL.  Returns FALSE if the prefix component was not NULL
	     and no such prefix was declared, otherwise returns TRUE (that
	     is, if the prefix component was NULL but there was no default
	     namespace declared, this function still returns TRUE, since
	     that is typically not an error.)

	     @param current The most recent namespace declaration or NULL
	                    if no namespaces have been declared.
	     @param name Complete name whose prefix component is used to
	                 find a namespace URI.
	     @param use_default If TRUE, default namespace declarations
	                        are used if the prefix component is NULL.
	                        If FALSE and the prefix component is NULL,
	                        the URI component is unconditionally set
	                        to NULL.
	     @return OpBoolean::IS_FALSE if the prefix component was not
	             NULL and no such prefix was declared, OpBoolean::TRUE
	             if that was not the case and OpStatus::ERR_NO_MEMORY
	             on OOM. */

	static BOOL ResolveName(XMLNamespaceDeclaration *current, XMLCompleteNameN &name, BOOL use_default = TRUE);
	/**< Resolves a complete name (prefix + local part) by filling in
	     the URI component.  Optionally (if 'use_default' is TRUE)
	     uses default namespace declarations when the prefix component
	     is NULL.  Returns FALSE if the prefix component was not NULL
	     and no such prefix was declared, otherwise returns TRUE (that
	     is, if the prefix component was NULL but there was no default
	     namespace declared, this function still returns TRUE, since
	     that is typically not an error.)

	     @param current The most recent namespace declaration or NULL
	                    if no namespaces have been declared.
	     @param name Complete name whose prefix component is used to
	                 find a namespace URI.
	     @param use_default If TRUE, default namespace declarations
	                        are used if the prefix component is NULL.
	                        If FALSE and the prefix component is NULL,
	                        the URI component is unconditionally set
	                        to NULL.
	     @return FALSE if the prefix component was not NULL and no
	             such prefix was declared and TRUE if that was not the
	             case. */

	static BOOL ResolveNameInScope(HTML_Element *element, XMLCompleteNameN &name);
	/**< Resolves a complete name (prefix + local part) in the scope of
	     element by filling in the URI component.

		 @param[in]      element The element. Cannot be NULL.
		 @param[in/out]  name Complete name whose prefix component is used to
		                      find a namespace URI.
		 @return TRUE if it was able to resolve the name. */

	static OP_STATUS Push(XMLNamespaceDeclaration::Reference &current, const uni_char *uri, unsigned uri_length, const uni_char *prefix, unsigned prefix_length, unsigned level);
	/**< Push a namespace declaration.  If the URI is NULL, the
	     declaration serves to undeclare the prefix.  If the prefix is
	     NULL, the declaration is a default namespace declaration.

	     The pointer 'current' is updated to point at the new
	     declaration.  The caller must own a reference to the
	     declaration that 'current' points at.  That reference is
	     transferred to the new declaration on success.  If the
	     operation fails, 'current' is left unchanged and no reference
	     counts are modified.

	     @param current The most recent namespace declaration or NULL
	                    if no namespaces have been declared.  Will be
	                    updated to point to the newly pushed
	                    declaration.
	     @param uri Namespace URI or NULL.
	     @param uri_length The length of 'uri,' or ~0u to use uni_strlen
	                       to calculate it.
	     @param prefix Namespace prefix or NULL.
	     @param prefix_length The length of 'prefix,' or ~0u to use
	                          uni_strlen to calculate it.
	     @param level Level of the pushed declaration.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	static void PushL(XMLNamespaceDeclaration::Reference &current, const uni_char *uri, unsigned uri_length, const uni_char *prefix, unsigned prefix_length, unsigned level);
	/**< Push a namespace declaration.  If the URI is NULL, the
	     declaration serves to undeclare the prefix.  If the prefix is
	     NULL, the declaration is a default namespace declaration.

	     The pointer 'current' is updated to point at the new
	     declaration.  The caller must own a reference to the
	     declaration that 'current' points at.  That reference is
	     transferred to the new declaration on success.  If the
	     operation fails, 'current' is left unchanged and no reference
	     counts are modified.

	     LEAVEs on OOM.

	     @param current The most recent namespace declaration or NULL
	                    if no namespaces have been declared.  Will be
	                    updated to point to the newly pushed
	                    declaration.
	     @param uri Namespace URI or NULL.
	     @param uri_length The length of 'uri'.
	     @param prefix Namespace prefix or NULL.
	     @param prefix_length The length of 'prefix'.
	     @param level Level of the pushed declaration. */

	static void Pop(XMLNamespaceDeclaration::Reference &current, unsigned level);
	/**< Pops all declarations with a level higher than or equal to
	     'level'.

	     The pointer 'current' is updated to point at the most recent
	     declaration not popped.  The caller must own a reference to
	     the declaration that 'current' points at.  That reference is
	     transferred to the new current declaration, or released if
	     all declarations are popped.

	     @param current The most recent namespace declaration or NULL
	                    if no namespaces have been declared.  Will be
	                    updated to point to the newly pushed
	                    declaration.
	     @param level Level of declarations to pop. */

	static OP_STATUS ProcessAttribute(XMLNamespaceDeclaration::Reference &current, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length, unsigned level);
	/**< Process an attribute.  If the attribute is a namespace
	     declaration attribute, a namespace declaration is pushed as
	     by a call to Push with appropriate arguments.  If the
	     attribute is not a namespace declaration attribute, this
	     function does nothing.

	     @param current The most recent namespace declaration or NULL
	                    if no namespaces have been declared.  Will be
	                    updated to point to the newly pushed
	                    declaration if a declaration is pushed.
	     @param name The attribute's name.
	     @param value The attribute's value.
	     @param value_length The lenght of the attribute's value.
	     @param level Level of the pushed declaration.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static OP_STATUS PushAllInScope(XMLNamespaceDeclaration::Reference &current, HTML_Element *element, BOOL elements_actual_ns = FALSE);
	/**< Push all namespace declarations in scope at 'element'.  The
	     resulting list of namespace declarations would be suitable
	     for correctly resolving namespace prefixes of attributes on
	     'element'.

	     @param current The most recent namespace declaration or NULL
	                    if no namespaces have been declared.  Will be
	                    updated to point to the newly pushed
	                    declaration if a declaration is pushed.
	     @param element An element.  Cannot be NULL.
	     @param elements_actual_ns FALSE by default.
	                    If TRUE, every element's actual namespace is
	                    pushed in addition to namespaces found in
	                    namespace declaration attributes.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

private:
	friend class Reference;
	/**< Calls IncRef() and DecRef(). */

	static XMLNamespaceDeclaration *IncRef(XMLNamespaceDeclaration *declaration);
	/**< Increment the declaration's reference counter.  Consider
	     using the Reference class instead of calling this function
	     directly.  If the argument is NULL, the function does
	     nothing.

	     @param declaration A namespace declaration or NULL.
	     @return The 'declaration' argument. */

	static void DecRef(XMLNamespaceDeclaration *declaration);
	/**< Decrement the declaration's reference counter.  If the
	     counter reaches zero, the object is deleted.  Consider using
	     the Reference class instead of calling this function
	     directly.  If the argument is NULL, the function does
	     nothing.

	     @param declaration A namespace declaration or NULL. */
};

#ifdef XMLUTILS_XMLNAMESPACENORMALIZER_SUPPORT

/** Helper class for normalizing namespace declarations in a tree.  Given the
    name of each element, and all of the attributes specified in the elements
    start tag, the namespace normalizer will add namespace declaration
    attributes and/or rename specified attributes so that the resulting set of
    specified attributes makes the tree as a whole namespace valid. */
class XMLNamespaceNormalizer
{
public:
	XMLNamespaceNormalizer(BOOL copy_strings, BOOL perform_normalization = TRUE);
	/**< Constructor.

	     If 'copy_strings' is TRUE, the functions StartElement() and
	     AddAttribute() will copy all names and attribute values they are
	     given, and the information returned by subsequent GetAttribute() will
	     be owned by the XMLNamespaceNormalizer object.  If 'copy_strings' is
	     FALSE nothing will be copied, no strings will be owned by the
	     XMLNamespaceNormalizer object and the information returned by
	     GetAttribute() calls will be the same as given to StartElement() or
	     AddAttribute() calls.

	     If 'perform_normalization' is FALSE, the normalizer will in fact not
	     normalize anything, and is essentially reduced to a simple attribute
	     collection.  It will still maintain a view of the current namespace
	     declarations however. */

	void SetPerformNormalization(BOOL new_perform_normalization) { perform_normalization = new_perform_normalization; }
	/**< Updates whether this normalizer actually normalizes.  In practice
	     this flag only affects each call to FinishAttributes(), and so can be
	     changed freely whenever. */

	~XMLNamespaceNormalizer();
	/**< Destructor.  Frees all memory allocated by the object.  The object
	     can be destroyed at any time, regardless of its current state. */

	OP_STATUS StartElement(const XMLCompleteName &name);
	/**< Start a new element named 'name'.  The local part of the name is
	     insignificant; the namespace uri and prefix of it may cause a
	     namespace declaration declaring that prefix to be introduced.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	OP_STATUS AddAttribute(const XMLCompleteName &name, const uni_char *value, BOOL overwrite_existing);
	/**< Add an attribute.  Must be called between calls to StartElement() and
	     FinishAttributes().  The attribute name's local part and its value
	     are only significant if the attribute is a namespace declaration
	     attribute, but will be stored and returned later in any case.  If the
	     value is known not to be significant, it can be NULL.

	     If 'overwrite_existing' TRUE, an existing attribute with the same
	     expanded name will be overwritten by the new attribute.  If it is
	     FALSE, and existing attribute with the same expanded name will cause
	     this function to return OpStatus::ERR without changing anything.

	     @param name The attribute's name.
	     @param value The attribute's value.
	     @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY. */

	BOOL RemoveAttribute(const XMLExpandedName &name);
	/**< Remove an attribute.  Returns TRUE if such an attribute was found
	     (and thus removed) and FALSE if not.

	     @param name Name of attribute to remove.
	     @return TRUE or FALSE. */

	OP_STATUS FinishAttributes();
	/**< Called when all attributes have been finished.  The resulting set of
	     specified attributes after namespace normalization can be read using
	     GetAttributesCount() and GetAttribute() after this function has been
	     called, until either StartElement() or EndElement() has been called.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	const XMLCompleteName &GetElementName() { return element_name; }
	/**< Returns the current element's name. */

	XMLNamespaceDeclaration *GetNamespaceDeclaration() { return ns; }
	/**< Returns the current namespace declarations.  Only modified by calls
	     to FinishAttributes() and EndElement().  After a call to
	     FinishAttributes(), this includes all declarations introduced by
	     namespace declaration attributes on the current element as well as
	     declarations introduced during normalization.  After a call to
	     EndElement(), all declarations introduced by attributes on the ended
	     element are gone. */

	unsigned GetAttributesCount() { return attributes_count; }
	/**< Returns number the number of specified attributes on the current
	     element. */

	const XMLCompleteName &GetAttributeName(unsigned index) { return attributes[index]->name; }
	/**< Fetch the index:th attribute's name.  The index must be lower than
	     the attributes count returned by GetAttributesCount(). */

	const uni_char *GetAttributeValue(unsigned index) { return attributes[index]->value; }
	/**< Fetch the index:th attribute's value.  The index must be lower than
	     the attributes count returned by GetAttributesCount(). */

	void EndElement();
	/**< End the current element.  Note that StartElement()/EndElement() calls
	     are used to specify a tree structure.  They should be matched (same
	     number of calls to StartElement() and EndElement()) unless the
	     normalization is aborted, and at any time the number of calls to
	     EndElement() cannot be higher to the number of calls to
	     StartElement().

	     Note that EndElement() resets both element name and the list of
	     attributes of the current element, and that it doesn't restore those
	     of the previous element. */

private:
	BOOL copy_strings, perform_normalization;
	XMLNamespaceDeclaration::Reference ns;
	unsigned depth;
	XMLCompleteName element_name;
	class Attribute
	{
	public:
		Attribute() : value(NULL) {}
		XMLCompleteName name;
		const uni_char *value;
	};
	Attribute **attributes;
	unsigned attributes_count, attributes_total;

	void Reset();
};

#endif // XMLUTILS_XMLNAMESPACENORMALIZER_SUPPORT
#endif // XMLNAMES_H
