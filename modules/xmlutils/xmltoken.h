/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLTOKEN_H
#define XMLTOKEN_H

#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmltypes.h"

class XMLParser;
class XMLTokenBackend;
class XMLDocumentInformation;

/** XML token.  One is produced for every DOCTYPE declaration,
    comment, CDATA section, start tag, end tag, empty element tag,
    processing instruction and sequence of character data in the
    document.  In addition, a special token is produced to signal that
    the end of the document has been found. */
class XMLToken
{
public:
	/** Token type.  Some types are "pseudo tokens," all other types
	    are "normal tokens."  The functions that are valid to call on
	    a token object depends on the type of the token.  GetType and
	    GetParser are always valid to call.  Other functions are
	    divided into groups as follows:

          name: GetName

	      data: GetData and GetDataLength

	      literal: GetLiteralIsWhitespace, GetLiteralIsSimple,
	               GetLiteral, GetLiteralLength

	      attributes: GetAttribute, GetAttributes, GetAttributesCount

	      docinfo: GetDocumentInformation

	      entity: GetEntity

	    The documentation for each type specifies which groups of
	    functions are available to call on tokens of that type. */
	enum Type
    {
		TYPE_Unknown,
		/**< Uninitialized or reset token (pseudo token).  A token
		     handler will never be called to handle a token of this
		     type. */

		TYPE_XMLDecl,
		/**< XML declaration (the "processing instruction" with target
		     "xml" at the beginning of the document.)  Available
		     function groups: name (always "xml",) attributes (one,
		     two or all of "version", "standalone" and "encoding")
		     and docinfo. */

		TYPE_PI,
		/**< Processing instruction.  Available function groups: name,
		     data and, depending on the name, attributes. */

		TYPE_Comment,
		/**< Comment.  Valid function groups: literal. */

		TYPE_CDATA,
		/**< CDATA section.  Valid function groups: literal. */

		TYPE_DOCTYPE,
		/**< Doctype (pseudo token).  Valid function groups: docinfo.
		     The document information can also be fetched from the
		     XMLParser object at any time (after it has been parsed,
		     obviously.) */

		TYPE_STag,
		/**< Start tag.  Valid function groups: name and
		     attributes. */

		TYPE_ETag,
		/**< End tag.  Valid function groups: name. */

		TYPE_EmptyElemTag,
		/**< Empty element tag.  Valid function groups: name and
		     attributes. */

		TYPE_Text,
		/**< Text. Valid function groups: literal. */

#ifdef XML_ENTITY_REFERENCE_TOKENS
		TYPE_EntityReferenceStart,
		/**< Start of entity replacement (pseudo token).  Tokens of
		     this type are only produced if it has been requested by a
		     call to XMLParser::SetEntityReferenceTokens. Valid
		     function groups: entiy. */

		TYPE_EntityReferenceEnd,
		/**< End of entity replacement (pseudo token).  Tokens of this
		     type are only produced if it has been requested by a call
		     to XMLParser::SetEntityReferenceTokens.  Valid function
		     groups: entiy. */
#endif // XML_ENTITY_REFERENCE_TOKENS

		TYPE_Finished
        /**< Parsing finished (pseudo token.) */
    };

	/** Object representing an attribute, either specified in a start
	    tag, empty element tag or processing instruction tag with a
	    known target; or declared with a default value in the DTD.

	    The only processing instruction target for which the token's
	    data is interpreted as attributes is "xml-stylesheet".

	    The Attribute objects are always owned by the XMLToken
	    object. */
	class Attribute
	{
	public:
		const XMLCompleteNameN &GetName() const { return name; }
		/**< Returns the attribute's name. */

		XMLCompleteNameN &GetNameForUpdate() { return name; }
		/**< Returns a non-const reference to the attribute's name. */

		const uni_char *GetValue() const { return value; }
		/**< Returns the attribute's value after normalization.  The
		     string is not null-terminated, unless TakeOverValue is
		     called and returns TRUE. */

		unsigned GetValueLength() const { return value_length; }
		/**< Returns the length of the string returned by GetValue. */

		BOOL OwnsValue() { return owns_value != 0; }
		/**< Returns TRUE if this Attribute object owns a copy of its
		     value (in which case TakeOverValue would return TRUE if
		     called) and FALSE if not. */

		BOOL TakeOverValue();
		/**< Take over ownership of the value, if possible.  If it is
		     possible, TRUE is returned and the caller is
		     responsible for deallocating the value (with delete[]).
		     If it is not possible, FALSE is returned and the value
		     must be copied. */

		BOOL GetSpecified() const { return specified != 0; }
		/**< Returns TRUE if the attribute was specified in the start
		     tag (or empty element tag) and FALSE if it was not but
		     was declared with a default value in the DTD. */

		BOOL GetId() const { return id != 0; }
		/**< Returns TRUE if the attribute was declared in the DTD to
		     be of the type ID. */

		void SetName(const uni_char *name, unsigned name_length);
		void SetName(const XMLCompleteNameN &name);
		void SetValue(const uni_char *value, unsigned value_length, BOOL need_copy, BOOL specified);
		void SetId();

		void Reset();

	protected:
		XMLCompleteNameN name;
		const uni_char *value;
		unsigned value_length;

		unsigned owns_value:1;
		unsigned specified:1;
		unsigned id:1;
	};

	/** Object representing a literal that is represented as a
	    sequence of strings. */
	class Literal
	{
	public:
		Literal();
		~Literal();

		const uni_char *GetPart(unsigned index) const { return parts[index].data; }
		/**< Returns the index'th part of the literal.  The string is
		     not null-terminated, unless TakeOverPart is called and
		     returns TRUE.  */

		unsigned GetPartLength(unsigned index) const { return parts[index].data_length; }
		/**< Returns the length of the index'th part of the
		     literal. */

		BOOL TakeOverPart(unsigned index);
		/**< Take over ownership of the part, if possible.  If it is
		     possible, TRUE is returned and the caller is now
		     responsible for deallocating the part (with delete[]).
		     If it is not possible, FALSE is returned and the part
		     must be copied. */

		unsigned GetPartsCount() const { return parts_count; }
		/**< Returns the number of parts in this literal. */

		OP_STATUS SetPartsCount(unsigned count);
		OP_STATUS SetPart(unsigned index, const uni_char *data, unsigned data_length, BOOL need_copy);
		void SetTokenBackend(XMLTokenBackend *backend);

	protected:
		class Part
		{
		public:
			~Part();

			const uni_char *data;
			unsigned data_length;
			BOOL owns_value;
		};

		Part *parts;
		unsigned parts_count;
		XMLTokenBackend *backend;
	};

	XMLToken(XMLParser *parser, XMLTokenBackend *backend = 0);
    ~XMLToken();

    Type GetType() const { return type; }
	/**< Returns the token's type. */

    const XMLCompleteNameN &GetName() const { return name; }
	/**< Retrieve the token's name.  Valid for all normal tokens
	     except Comment, CDATA and Text tokens.  For PI,
	     EntityReferenceStart and EntityReferenceEnd tokens, only
	     the localpart of the name is relevant. */

	XMLCompleteNameN &GetNameForUpdate() { return name; }
	/**< Retrieve a non-const reference to the token's name, to use
	     for updating or changing the name.  Don't use this unless you
	     know the token's owner doesn't care! */

    const uni_char *GetData() const { return data; }
	/**< Retrieve the token's data.  Only valid for PI tokens.  Use
	     GetLiteral to retrieve the value of Comment, CDATA or Text
	     tokens.  The string is not null-terminated! */

    unsigned GetDataLength() const { return data_length; }
	/**< Retrieve the length of the token's data. */

	BOOL GetLiteralIsWhitespace() const;
	/**< Returns TRUE if a literal token (Comment, CDATA or Text) is
	     all whitespace characters. */

	const uni_char *GetLiteralSimpleValue() const;
	/**< Returns the literal's value as a string.  If the literal is
	     split into several parts, NULL is returned.  If so,
	     GetAllocatedValue or GetLiteral must be used.  The string is
	     not null-terminated! */

	uni_char *GetLiteralAllocatedValue() const;
	/**< Returns the literal's value as a string that is owned by the
	     caller.  It must be deallocated using the delete[] operator.
	     The string is null-terminated. */

	unsigned GetLiteralLength() const;
	/**< Returns the length of the literal's value. */

	OP_STATUS GetLiteral(Literal &literal) const;
	/**< Initializes the given Literal object to represent the
	     literal's value. */

	Attribute *GetAttribute(const uni_char *qname, unsigned qname_length = ~0u) const;
	/**< Retrieves an attribute by qname.  Returns NULL if no such
	     attribute was specified or declared with a default value in
	     the DTD.  If qname_length is ~0 'qname' must be a null-
	     terminated string, otherwise only the qname_length first
	     characters in name are used.  Only valid for STag,
	     EmptyElemTag tokens and PI tokens. */

    Attribute *GetAttributes() const { return attributes; }
	/**< Returns all attributes (specified or declared with a default
	     value in the DTD.)  Only valid for STag, EmptyElemTag and PI
	     tokens. */

    unsigned GetAttributesCount() const { return attributes_count; }
	/**< Returns the number of attributes in the array returned by
	     GetAttributes(). */

	const XMLDocumentInformation *GetDocumentInformation() { return docinfo; }
	/**< Returns a reference to a XMLDocumentInformation object that
	     contains the information available from the XML declaration
	     and document type declaration.  Only available for XMLDecl
	     and DOCTYPE tokens.  Note: when fetched from an XMLDecl
	     token, information about the document type declaration will
	     typically not be available since the XML declaration always
	     precedes the document type declaration. */

	void Initialize();
	/**< Initialize or reinitialize the token's state. */

	void SetType(Type new_type) { type = new_type; }
	/**< Set the token's type. */

	void SetName(const XMLCompleteNameN &name);
	/**< Set the token's name. */

	void SetData(const uni_char *data, unsigned data_length);
	/**< Set processing instruction data. */

	OP_STATUS AddAttribute(Attribute *&attribute);
	/**< Add an attribute. */

	void RemoveAttribute(unsigned index);
	/**< Remove an attribute. */

	void SetDocumentInformation(const XMLDocumentInformation *new_docinfo) { docinfo = new_docinfo; }

	void Reset();

	XMLParser *GetParser() const { return parser; }

#ifdef XML_ERRORS
	BOOL GetTokenRange(XMLRange &range);
	/**< Sets 'range' to represent the whole token and returns TRUE.
	     If the range is not known, FALSE is returned and 'range' is
	     left unmodified. */

	BOOL GetAttributeRange(XMLRange &range, unsigned index);
	/**< Sets 'range' to represent the index-th attribute and returns
	     TRUE.  If the range is not known, FALSE is returned and
	     'range' is left unmodified.  Note: the range of unspecified
	     (defaulted) attributes is always unknown. */
#endif // XML_ERRORS

protected:
	Type type;

	XMLCompleteNameN name;

	const uni_char *data;
	unsigned data_length;

	Attribute *attributes;
	unsigned attributes_count, attributes_total;

	XMLParser *parser;
	XMLTokenBackend *backend;
	const XMLDocumentInformation *docinfo;
};

#endif // XMLTOKEN_H
