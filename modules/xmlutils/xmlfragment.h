/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLFRAGMENT_H
#define XMLFRAGMENT_H

#ifdef XMLUTILS_XMLFRAGMENT_SUPPORT

#include "modules/xmlutils/xmltypes.h"

class XMLFragmentData;
class XMLExpandedName;
class XMLCompleteName;
class XMLNamespaceDeclaration;
class XMLFragmentSerializerBackend;
class XMLTreeAccessor;
class ByteBuffer;
class OpFileDescriptor;

/**

  Convenience class intended for easily implemented parsers and
  generators of known simple XML dialects; principal use being the
  Opera 'scope' module and its support code in other modules.  The
  parser is not optimized for minimal memory footprint, since it
  parses the entire fragment into an internal structure.

  Primarily, the parser has two important states:
    - the current element
	- the position

  The current element is simply a pointer to an element in the
  fragment.  All attribute access functions access attributes on this
  element.  The current element might be null, if the parser is on the
  top level of the fragment.  In this case, the following functions
  should not be used:
    GetElementName(),
    GetNamespaceDeclaration(),
    MakeSubFragment(),
    GetAttribute(),
    GetAttributeFallback(),
    GetNextAttribute(),
	GetId() and
    GetAllText().

  The function HasCurrentElement() can be used to check if there is a
  current element.

  The position can be thought of as a cursor positioned between two
  characters in the source text.  It is always positioned inside the
  current element, when there is a current element.  Additionally, it
  is never positioned inside character data, always "between tokens."

  Plain text and CDATA sections are considered the same, and all
  consecutive occurences of plain text and/or CDATA sections are
  merged.  Whitespace is handled as described in the description of
  the SetDefaultWhitespaceHandling() function.

  All comments and processing instructions are completely ignored,
  unless API_XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT is imported, in
  which case they can be accessed as well.  This extended content
  handling is mainly so that an XMLFragment can be used to parse and
  later reserialize as described in "Canonical XML 1.0".  To enable
  storage (and thus access) of comments and processing instructions,
  the function SetStoreCommentsAndPIs() must be called before parsing.

  <h3>Parsing XML source code</h3>

  To parse XML source code into an XMLFragment is very simple:

  <pre>
    const uni_char *sourcecode = UNI_L("...");
    XMLFragment fragment;
	RETURN_IF_ERROR(fragment.Parse(sourcecode));
  </pre>

  If parsing fails, Parse returns OpStatus::ERR.  After parsing, the
  fragment is ready for "reading".

  <h3>Reading content</h3>

  An XMLFragment is basically meant to be read sequentially from
  beginning to end.  "Random access" is not really supported.

  Code for reading the contents of an XMLFragment containing a
  document that follows the content model

  <pre>
    <!ELEMENT root (item*)>
    <!ELEMENT item (subitem | text)*>
    <!ELEMENT subitem (data*)>
    <!ELEMENT text (#PCDATA)>
    <!ELEMENT data (#PCDATA)>
  </pre>

  could look something like this:

  <pre>
    TempBuffer buffer;
    if (fragment.EnterElement(UNI_L("root")))
    {
      while (fragment.EnterElement(UNI_L("item")))
      {
        if (fragment.EnterAnyElement())
        {
          if (fragment.GetElementName() == UNI_L("subitem"))
            while (fragment.EnterElement(UNI_L("data")))
            {
              RETURN_IF_ERROR(fragment.GetAllText(buffer));
              HandleData(buffer);
              fragment.LeaveElement();
            }
          else if (fragment.GetElementName() == UNI_L("text"))
          {
            RETURN_IF_ERROR(fragment.GetAllText(buffer));
            HandleText(buffer);
          }
          fragment.LeaveElement();
        }
        fragment.LeaveElement();
      }
    }
  </pre>

  This type of code is not particularly robust in its handling of
  unexpected elements (content model violations, that is.)  Using only
  EnterAnyElement() and GetElementName() enables reading of more
  untrusted documents, but is of course less convienent.

  Reading moves through the document by skipping past text and start
  tags in EnterElement() and EnterAnyElement(), skipping past text in
  GetAllText() and skipping past the "current" end tag (and any
  content preceding the end tag) in LeaveElement().  It is possible to
  move backwards either to the start of the entire fragment, using
  RestartFragment(), and the start of the current element, using
  RestartCurrentElement().

  <h3>Accessing attributes</h3>

  Attributes are accessed on the current element, that is, the element
  a call to LeaveElement() would leave.  After a (successful) call to
  EnterElement(), the entered element's attributes are accessed.
  After a call to LeaveElement(), attributes are accessed on the
  parent of the element just left.

  <b>Read named attribute</b>

  To read a specific attribute by name, use GetAttribute() like

  <pre>
    if (fragment.EnterElement(UNI_L("element")))
    {
      if (const uni_char *attr1 = fragment.GetAttribute(UNI_L("attr1")))
        HandleAttribute(...);

      if (const uni_char *attr2 = fragment.GetAttribute(UNI_L("attr2")))
        HandleAttribute(...);
    }
  </pre>

  GetAttributeFallback() can be used to provide an explicit default
  value in case the attribute wasn't specified in the document.

  <b>Iterate through all attributes</b>

  To iterate through all the attribute's on an element, do something
  like

  <pre>
    if (fragment.EnterElement(UNI_L("element")))
    {
      XMLCompleteName name;
      const uni_char *value;

      while (fragment.GetNextAttribute(name, value))
        HandleAttribute(...);
    }
  </pre>

  The iteration can be restarted using RestartCurrentElement().

  <h3>Generating content</h3>

  Content can be inserted into an XMLFragment as well.  To build a
  document such as the one read in the section about reading, do
  something like

  <pre>
    XMLFragment fragment;

    RETURN_IF_ERROR(fragment.OpenElement(UNI_L("root")));
    RETURN_IF_ERROR(fragment.OpenElement(UNI_L("item")));
    RETURN_IF_ERROR(fragment.AddText(UNI_L("text"), UNI_L("this is some text")));
    RETURN_IF_ERROR(fragment.OpenElement(UNI_L("subitem")));
    RETURN_IF_ERROR(fragment.AddText(UNI_L("data"), UNI_L("this is some data")));
    RETURN_IF_ERROR(fragment.AddText(UNI_L("data"), UNI_L("this is some more data")));
    RETURN_IF_ERROR(fragment.CloseElement()); // close 'subitem'
    RETURN_IF_ERROR(fragment.CloseElement()); // close 'item'
    RETURN_IF_ERROR(fragment.CloseElement()); // close 'root'
  </pre>

  which would generate the document (minus the formatting, typically)

  <pre>
    <root>
      <item>
        <text>this is some text</text>
        <subitem>
          <data>this is some data</data>
          <data>this is some more data</data>
        </subitem>
      </item>
    </root>
  </pre>

  It is not strictly necessary to close all opened elements, closing
  the element only affects where new content is inserted, not whether
  the XML fragment is well-formed.  RestartFragment() can be used when
  generation is finished to prepare the XMLFragment for reading.

  Reading and generating can be combined freely.  To add another
  'text' element at the end of the 'item' element in the fragment
  generated above, one can use the code

  <pre>
    fragment.RestartFragment();
    fragment.EnterElement(UNI_L("root"));
    fragment.EnterElement(UNI_L("item"));
    while (fragment.EnterAnyElement())
      fragment.LeaveElement();
    RETURN_IF_ERROR(fragment.AddText(UNI_L("text"), UNI_L("additional text")));
  </pre>

  The AddText() calls in the code above open an element named as their
  first argument, add a text node containing the second argument in it
  and close it again, that is, the same as the code

  <pre>
    RETURN_IF_ERROR(fragment.OpenElement(UNI_L("text")));
    RETURN_IF_ERROR(fragment.AddText("this is some text"));
    fragment.CloseElement();
  </pre>

  <h3>Retrieving XML source code</h3>

  At any time while reading or generating content, the whole
  XMLFragment can be retrieved as XML source code, either as a single
  unicode string or such a string encoded into binary data using any
  export encoding the encodings module supports, using the functions
  GetXML() and GetEncodedXML().

  When converting to XML source code, characters in character data and
  attribute values that are not valid unescaped in XML, or that are
  not possible to represent in the selected encoding, are escaped
  using hexadecimal character references.

  Note that certain characters are not ever valid in character data or
  attribute values, even as character references.  If such characters
  are present, the serialization will fail and
  GetXML()/GetEncodedXML() return OpStatus::ERR.  See the section
  "Binary data" for a workaround.

  If element or attribute names in the fragment has namespace URI:s,
  appropriate namespace declarations ('xmlns' and 'xmlns:prefix'
  attributes) will be added if needed, and the prefix of those names
  may be changed if that is necessary.

  <h3>Whitespace handling</h3>

  Whitespace handling is slightly complex.  Basically, there are two
  modes:

  <dl>
   <dt>default / XMLWHITESPACEHANDLING_DEFAULT</dt>
   <dd>
    According to XML: use the application's default behaviour.
   </dd>
   <dd>
    In XMLFragment: normalize whitespace characters so that only space
    (#x20) characters occur, and so that they never occur next to each
    other or in the beginning or end of an attribute or character data
    block.
   </dd>
   <dt>preserve / XMLWHITESPACEHANDLING_PRESERVE</dt>
   <dd>
    According to XML: preserve whitespace.
   </dd>
   <dd>
    In XMLFragment: do not modify whitespace in character data, and
    insert the necessary information and use character references in
    generated XML source code to keep it preserved outside of the
    XMLFragment.
   </dd>
  </dl>

  How the choice between these modes is made varies:

  <b>During parsing</b>

  During parsing, the XMLFragment's default whitespace handling mode,
  as set by SetDefaultWhitespaceHandling() or 'default' if not set, is
  initially used on all character data encountered in the document.
  Attribute values, however, are not normalized other than what the
  XML parser does following the XML specification (which, on the other
  hand, is quite a lot of normalization already.)

  The parsed document can change whitespace handling mode inside
  elements if it contains 'xml:space' attributes.  Such attributes
  should have the values 'preserve' or 'default', and set the
  whitespace handling mode inside the elements accordingly.

  <b>During generation</b>

  For content generated using the functions AddText() and
  SetAttribute(), the XMLFragment's default whitespace handling is
  <em>not</em> used.  Instead, those functions each have an argument
  named 'wshandling' that decides which mode to use for that content.
  This argument defaults to XMLWHITESPACEHANDLING_DEFAULT, that is,
  whitespace in the content is normalized by default.

  When generated content is retrieved as XML source code, extra
  'xml:space' attributes will be added on elements, signalling how
  character data inside them is should to be handled.  Essentially, if
  AddText() was used with the argument XMLWHITESPACEHANDLING_PRESERVE
  to add character data immediately inside an element, that element
  will have the attribute specification 'xml:space="preserve"' in its
  start tag in the generated XML source code, unless one of its
  ancestors already had such an attribute.

  Since 'xml:space' attributes do not apply to attribute values,
  non-space whitespace characters in attribute values are always
  escaped in the generated XML source code (since they typically only
  occur there if they occured escaped in the parsed XML source code or
  were added by a call to SetAttribute that preserved whitespace.)

  <h3>Binary data</h3>

  Since some characters simply cannot be represented in character data
  or attribute values in XML, unknown binary data should not be added
  as text in an XMLFragment.  Such data must be encoded in some way
  external to XML.  XMLFragment provides functionality for storing
  binary data encoded using base64 encoding.  The functions
  AddBinaryData() and GetBinaryData() are used the same way as
  AddText() and GetAllText(), but access base64 encoded binary data
  instead of character data.

  Note that XMLFragment doesn't handle base64 encoded character data
  any different from ordinary character data: character data added
  with AddText() can be retrieved using GetBinaryData(), assuming it
  is valid base64 data, and binary data added with AddBinaryData() can
  be retrieved as unencoded character data using GetAllText().

*/
class XMLFragment
{
private:
	friend class XMLFragmentSerializerBackend;
	friend class XMLFragmentTreeAccessor;

	XMLFragmentData *data;
	XMLWhitespaceHandling default_wshandling;
#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	BOOL store_comments_and_pis;
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
#ifdef XML_ERRORS
	const char *error_description;
	XMLRange error_location;
#endif // XML_ERRORS

	OP_STATUS Construct();

public:
	XMLFragment();
	/**< Constructor.  The object is invalid for use until Parse() has
	     been called successfully. */

	virtual ~XMLFragment();
	/**< Destructor. */

	void SetDefaultWhitespaceHandling(XMLWhitespaceHandling wshandling) { default_wshandling = wshandling; }
	/**< Sets default whitespace handling used during parsing.  This
	     is overridden by any xml:space attributes in source text.
	     The default default is XMLWHITESPACEHANDLING_DEFAULT (no pun
	     intended) which means that all whitespace only character data
	     is completely skipped (the parser behaves in all regards as
	     if it wasn't in the source text at all,) that leading and
	     trailing whitespace characters are stripped and that all
	     sequences of one or more consecutive whitespace characters
	     are normalized into a single space character.

	     Whitespace is defined as all characters data matched by the
	     'S' production in XML 1.0/1.1:

	       S ::= (#x20 | #x9 | #xD | #xA)+

	     Linebreaks are also normalized as specified by the used XML
	     version, meaning for instance that in XML 1.1, the sequence
	     (#xD #x85) comes out of the XML parser as a single #xA, which
	     then is normalized into a single #x20.  As usual XML 1.0 is
	     the default if no XML declaration is present at the beginning
	     of the document.

	     This function must be called before the call to Parse() to
	     have any effect.

	     @param wshandling New default whitespace handling. */

#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	void SetStoreCommentsAndPIs(BOOL value = TRUE) { store_comments_and_pis = value; }
	/**< If 'value' is TRUE, comments and processing instructions are
	     stored in the fragment by the Parse() functions.  By default,
	     they are not.  The functions GetComment() and
	     GetProcessingInstruction() can be used to access comments and
	     processing instructions.  The functions EnterElement(),
	     EnterAnyElement() and GetText() will all skip comments and
	     processing instructions as if they weren't there. */
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

	OP_STATUS Parse(const uni_char *text, unsigned text_length = ~0u);
	/**< Parses the text.  If the text is a well-formed XML fragment,
	     OpStatus::OK is returned and the object is ready to be used.
	     If the text is not a well-formed XML fragment OpStatus::ERR
	     is returned, and none of the other functions may be called.

	     The fragment may start with an XML declaration specifying XML
	     version, encoding (which is ignored) and standalone, and a
	     document type declaration.

	     External entities are never loaded while internal entities
	     are expanded as usual.

	     Calling this function always resets the object to an initial
	     state for parsing the new text.  That is, all current state
	     is lost.

	     @param text Source text.  Need not be null terminated if its
	                 length is specified.
	     @param text_length Length of 'text' or ~0u to calculate
	                        length using uni_strlen.

	     @return OpStatus::OK on success, OpStatus::ERR on parse
	             errors or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS Parse(ByteBuffer *buffer, const char *charset = NULL);
	/**< Parses the contents of the buffer as if it had been joined
	     into a single string, converted to unicode using either the
	     supplied charset, the charset specified in the XML
	     declaration, if one is present at the beginning of the data,
	     or 'UTF-8', and then used in a call to Parse(const uni_char
	     *, unsigned).

	     @param buffer Buffer containing data to parse.
	     @param charset Optional charset to use when decoding the
	                    data.

	     @return Anything returned by Parse(const uni_char *,
	             unsigned), and OpStatus::ERR if 'charset' is not a
	             supported encoding. */

	OP_STATUS Parse(OpFileDescriptor *fd, const char *charset = NULL);
	/**< Reads all data from the file descriptor and parses it.  The
	     file descriptor should be open for reading.  If reading from
	     the file fails, the error returned by OpFileDescriptor::Read
	     will be returned by this function.  Otherwise, this function
	     works exactly like Parse(ByteBuffer *, const char *).

	     @param fd A file descriptor open for reading.
	     @param charset Optional charset to use when decoding the
	                    data.

	     @return OpStatus::OK on success, or anything returned by
	             OpFileDescriptor::Read, or anything returned by
	             Parse(const ByteBuffer *, const char *). */

	OP_STATUS Parse(URL url);
	/**< Creates a URL_DataDescriptor from the URL (by calling
	     URL::GetDescriptor with the arguments mh=NULL,
	     follow_ref=TRUE, override_content_type=URL_XML_CONTENT and
	     otherwise the defaults,) reads all data from and parses it.
	     The URL should be loaded or otherwise possible to read all
	     data from.  If no data is available to read from the URL or
	     if not all data is available to read, this function will
	     behave as if Parse(const uni_char *, unsigned) was called
	     with an empty string or truncated string respectively.

	     @param url URL to read and parse data from.

		 @return OpStatus::OK on success, or anything thrown by
	             URL_DataDescriptor::RetrieveDataL(), or anything
	             returned by Parse(const uni_char *, unsigned), or
	             OpStatus::ERR_NO_MEMORY if URL::GetDescriptor()
	             returns NULL (which it might do if the URL is not
	             ready for use as it should be, in addition to on
	             OOM.) */

	OP_STATUS Parse(HTML_Element *element);
	/**< Creates an XMLFragment containing the same information as the
	     tree of elements whose root is 'element'.  The element does
	     not have to be an actual root element (that is, it can have
	     both ancestors and siblings) but the resulting XMLFragment
	     will look like if it was an actual root element.

	     @param element An element.

	     @return OpStatus::OK on success or OpStatus::ERR_NO_MEMORY on
	             OOM. */

#ifdef XML_ERRORS
	/* Parse error access.  Enabled by FEATURE_XML_ERRORS. */

	const char *GetErrorDescription() { return error_description; }
	/**< Returns a string describing the parse error, or NULL if the
	     there was some other kind of problem.  Should only be used
	     after one of the Parse() functions has returned
	     OpStatus::ERR.

	     @return Error message.  The string is not owned by the
	             caller and should not be freed. */

	XMLRange GetErrorLocation() { return error_location; }
	/**< Returns the location of the parse error.  Should only be used
	     after one of the Parse() functions has returned
	     OpStatus::ERR.  A range with an invalid start point is
	     returned if no error location is known.

	     @return An error location. */
#endif // XML_ERRORS

	void RestartFragment();
	/**< Restarts the fragment, which means moving position to
	     before the very first content and setting no current
	     element. */

	BOOL HasMoreContent();
	/**< Returns TRUE if there is any more content and FALSE if the
	     end of the fragment or the current element (if an element has
	     been entered) has been reached.

	     @return TRUE or FALSE. */

	BOOL HasMoreElements();
	/**< Like HasMoreContent() but counts only elements.

	     @return TRUE or FALSE. */

	BOOL HasCurrentElement();
	/**< Returns TRUE if there is a current element, or FALSE if the
	     position is on the top-level of the fragment.

	     @return TRUE or FALSE. */

	/** Content types. */
	enum ContentType
	{
		CONTENT_ELEMENT,
		CONTENT_TEXT,
#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
		CONTENT_COMMENT,
		CONTENT_PROCESSING_INSTRUCTION,
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
		CONTENT_END_OF_ELEMENT,
		CONTENT_END_OF_FRAGMENT
	};

	ContentType GetNextContentType();
	/**< Returns the type of content directly after the position.

	     If type is CONTENT_ELEMENT a successful call to EnterElement
	     will not have skipped any content.

	     If type is CONTENT_TEXT a call to GetText() would succeed.

	     If type is CONTENT_END_OF_FRAGMENT there is no more content.

	     @return The type of content directly after the position. */

	BOOL SkipContent();
	/**< Moves to the next position in the current element.

		 @return TRUE if we moved to the next content, else FALSE. */

	BOOL EnterElement(const XMLExpandedName &name);
	/**< Enters an element named 'name' and returns TRUE, if the next
	     element is named 'name', otherwise does nothing and returns
	     FALSE.  Only elements in the current element, if there is a
	     current element, or top-level elements, if there is no
	     current element, are considered.  If 'name' has no prefix,
	     any element with the right local part and namespace URI is
	     entered, regardless of its prefix.  If 'name' has a prefix,
	     only elements with exactly the right complete name are
	     considered.

	     If an element is entered, the position is moved to after the
	     element's start tag.  Empty element tags are handled as a
	     start tag directly followed by an end tag; that is, a call to
	     LeaveElement() is also required to move the position to after
	     the tag.  If there is non-element content between the
	     position and the start tag, it is skipped.

	     If an element is not entered, the position is not moved, not
	     even to skip content that would have been skipped if an
	     element had been entered.

	     @param name The element name's.

	     @return TRUE or FALSE. */

	BOOL EnterAnyElement();
	/**< Like EnterElement, except it enters the next element, if
	     there is one, regardless of its name.

	     @return TRUE or FALSE. */

	void RestartCurrentElement();
	/**< Restarts the current element, which means moving position to
	     where it was right after EnterElement, and restarting the
	     attribute iteration performed by GetNextAttribute(). */

	const XMLCompleteName &GetElementName();
	/**< Returns the name of the current element (last one entered and
	     not left.)  The strings returned by the object are valid
	     during the whole life time of the XMLFragment object, but no
	     longer. */

	XMLNamespaceDeclaration *GetNamespaceDeclaration();
	/**< Returns a chain of namespace declarations representing all
	     namespaces in scope at the current element.  Its reference
	     count is not increased by this function, and need not be
	     increased at all unless it's used after the XMLFragment
	     object has been destroyed.  If no namespaces are in scope at
	     the current element, NULL is returned.

	     @return A namespace declaration chain or NULL. */

	OP_STATUS MakeSubFragment(XMLFragment *&fragment);
	/**< Creates an XMLFragment object that is limited to the
	     remaining content of the current element.  The state of this
	     XMLFragment is not affected by this function or by use of the
	     created XMLFragment.

	     The created XMLFragment will initially have no current
	     element.  Modifications made to the created XMLFragment will
	     not affect the this XMLFragment.

	     @param fragment Set to the created object.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	const uni_char *GetAttribute(const XMLExpandedName &name);
	/**< Returns the value of the attribute named 'name' on the
	     current element.  If the attribute was not specified but had
	     a declared default value, the default value is returned.  If
	     the attribute was not specified and had no declared default
	     value, NULL is returned.  If the name has a namespace URI but
	     no prefix, attributes with a prefix declared to that URI *or*
	     attributes with no prefix on an element with that namespace
	     URI are considered.  If the name has no namespace URI, only
	     namespaces with no prefix are considered, regardless of the
	     element's namespace URI.  If the name has both a namespace
	     URI and a prefix, only attributes with names exactly matching
	     the provided namespace URI and prefix are considered.

	     @param name The attribute name.

	     @return The attribute's value or NULL. */

	const uni_char *GetAttributeFallback(const XMLExpandedName &name, const uni_char *fallback);
	/**< Like GetAttribute(), but returns 'fallback' instead of NULL.

	     @param name The attribute name.
	     @param fallback Fallback value or NULL.

	     @return The attribute's value or 'fallback'. */

	const uni_char *GetId();
	/**< Returns the value of the first attribute declared to be of
	     the type ID.  The attributes 'xml:id' is always declared to
	     be of type ID, as is a prefixless attribute named 'id' on an
	     element with XHTML, SVG, WML or XSLT namespace.  Using
	     'xml:id' or declaring the attribute to be of type ID in the
	     internal document type subset is highly recommended when
	     authoring fragments to be parsed.  Relying on the attribute's
	     implicit type in some namespace is bad.

	     If no appropriate attribute was found, NULL is returned.  A
	     declared default value is never returned, even if it for some
	     reason might seem like it might be.

	     @return An id or NULL. */

	BOOL GetNextAttribute(XMLCompleteName &name, const uni_char *&value);
	/**< Returns the next attribute (or the first, if it hasn't been
	     used on the current element yet.)  Attributes are returned in
	     the order they were specified in the tag, with non-specified
	     attributes with declared default values last.  This function
	     can be used at any time, mixed with any other function calls
	     except EnterFunction() and LeaveFunction(), that change
	     current element and thus change what element's attributes are
	     iterated over.

	     If there were no more attributes, FALSE is returned and the
	     arguments are left unmodified.

	     @param name Set to the next attribute's name.
	     @param value Set to the next attribute's value.

	     @return TRUE if there was a next attribute and FALSE if there
	             wasn't. */

	OP_STATUS GetAllText(TempBuffer &buffer);
	/**< Appends the concatenation of all character data in the
	     current element to 'buffer', including that which is in
	     descendant elements of the current element.  The position is
	     not moved.

	     For whitespace handling, see SetDefaultWhitespaceHandling().

	     @param buffer Buffer to which character data is appended.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS GetAllText(TempBuffer &buffer, const XMLExpandedName &name);
	/**< Like GetAllText(TempBuffer &) but automatically enters an
	     element named 'name' before and leaves it after, as if
	     EnterElement and LeaveElement had been called.  If entering
	     the element fails (because the next element is not named
	     'name' or because there is no next element) no text is
	     appended.  If you need to know, enter the element manually.

	     The position is moved to after the element, if an element is
	     entered, otherwise the position is not changed.

	     For whitespace handling, see SetDefaultWhitespaceHandling().

	     @param buffer Buffer to which character data is appended.
	     @param name Name of element to enter.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	void LeaveElement();
	/**< Leaves the current element.  This moves the position to
	     directly after the element's end tag. */

	const uni_char *GetText();
	/**< Returns the concatenation of all character data up to the
	     next start tag, end tag or empty element tag or the empty
	     string if there is no such character data.  The position is
	     moved past the returned text.

	     For whitespace handling, see SetDefaultWhitespaceHandling().

	     @return Character data or the empty string. */

#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	const uni_char *GetComment();
	/**< Returns the text inside the comment immediately following the
	     current position, or NULL if there is no comment immediately
	     following the current position.  (If GetNextContentType()
	     returns CONTENT_COMMENT, the function will return non-NULL,
	     otherwise it will return NULL.)  The position is moved past
	     the comment if a comment was found, otherwise it is not
	     moved.

	     No whitespace normalization other than basic linebreak
	     normalization is performed on text inside comments.

	     @return Comment text or NULL. */

	BOOL GetProcessingInstruction(const uni_char *&target, const uni_char *&data);
	/**< Returns the target and data of the processing instruction
	     immediately following the current position, and TRUE, if a
	     processing instruction is immediately following the current
	     position, otherwise returns FALSE.  If the processing
	     instruction contained no data (only whitespace followed the
	     target) 'data' will be set to an empty string.  The position
	     is moved past the returned processing instruction if a
	     processing instruction was found, otherwise it is not moved.

	     No whitespace normalization other than basic linebreak
	     normalization is performed on data of a processing
	     instruction, but note that any whitespace between the target
	     and the first non-whitespace character following the target
	     is not part of the data, and thus will not appear at the
	     beginning of the string in 'data'.

	     @param target Set to a non-NULL string if a processing
	                   instruction was found, otherwise not modified.
	     @param data Set to a non-NULL string if a processing
	                 instruction was found, otherwise not modified.

	     @return TRUE if a processing instruction was found, otherwise
	             FALSE. */
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

	OP_STATUS OpenElement(const XMLCompleteName &name);
	/**< Creates a new element as a child of the current element,
	     inserted at the current position and enters it.  The name
	     must be a valid QName.  If it contains a prefix and its
	     namespace URI is NULL, the prefix is stripped.  If it
	     contains a prefix and that prefix is not declared, an
	     appropriate namespace declaration attribute is automatically
	     added.  If it contains no prefix and its namespace URI is not
	     NULL and the default namespace is not that URI, an
	     appropriate default namespace declaration is automatically
	     added.

	     After a successful call, the current element is the newly
	     created element and the position is after its start tag.  To
	     leave the element, use CloseElement.  Note however that
	     calling CloseElement is not required in order for the element
	     to have an end tag, the end tag is inserted immediately by
	     this function.

	     @param name The element's name.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS SetAttribute(const XMLCompleteName &name, const uni_char *value, XMLWhitespaceHandling wshandling = XMLWHITESPACEHANDLING_DEFAULT);
	/**< Sets an attribute on the current element.  If an attribute
	     with the same expanded name already exists, it is removed
	     first.  If the name has a namespace URI but no prefix, and
	     the current element's namespace URI is not the same as the
	     name's, an appropriate namespace declaration attribute is
	     added first.  If it is not possible to declare the name's
	     prefix like that, the name's prefix is changed to some other
	     available prefix, and an appropriate namespace declaration
	     attribute declared that prefix is added first.

	     Note: markup characters, such as '<' and '&', in 'value'
	     should not and can not be escaped with entity or character
	     references.  The value is automatically escaped as needed if
	     the fragment is converted to XML text.

	     Whitespace is handled according to 'wshandling'.  If the
	     value is XMLWHITESPACEHANDLING_DEFAULT, whitespace is
	     normalized as described in the description of
	     SetDefaultWhitespaceHandling, otherwise the value is not
	     modified and all whitespace characters other than space in
	     the attribute value will be escaped in generated XML source
	     code, to keep the next XML parser parsing it from normalizing
	     those whitespace characters into spaces.

	     @param name The attribute's name.
	     @param value The attribute's value.
	     @param wshandling Whitespace handling.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS SetAttributeFormat(const XMLCompleteName &name, const uni_char *format, ...);
	/**< Like SetAttribute, but the value is preprocessed as if by
	     a call to OpString::AppendFormat. */

	void CloseElement();
	/**< Closes the current element, making its parent element the
	     current element and moving the position to after its end
	     tag. */

	OP_STATUS AddText(const uni_char *text, unsigned text_length = ~0u, XMLWhitespaceHandling wshandling = XMLWHITESPACEHANDLING_DEFAULT);
	/**< Adds text.  The added text is automatically merged with any
	     text content right before or after the position where the new
	     text is inserted.

	     Whitespace is handled according to the 'wshandling'
	     attribute, see SetDefaultWhitespaceHandling for the
	     interpretation of different values.  Any xml:space attributes
	     on the current element or its ancestors are not directly
	     taken into account, but such an attribute is added on the
	     current element if necessary when the fragment is serialized
	     to XML text, in order to preserve the whitespace handling
	     when the fragment is parsed again.  Note that if xml:space
	     attributes are added manually, or if text is added in the
	     same element with different whitespace handling specified,
	     the end result may not be what you intend.

	     Note: the text added is really text.  It is not parsed for
	     markup, and any markup characters in it are escaped with
	     character references if the fragment is converted to XML
	     text.

	     @param text Text to add.  Need not be null terminated if its
	                 length is specified.
	     @param text_length Length of 'text' or ~0u to calculate
	                        length using uni_strlen.
	     @param wshandling Whitespace handling.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS AddText(const XMLExpandedName &name, const uni_char *text, unsigned text_length = ~0u, XMLWhitespaceHandling wshandling = XMLWHITESPACEHANDLING_DEFAULT);
	/**< Adds an element named 'name' containing the specified text.
	     Calling this function corresponds to the sequence

	       OpenElement(name);
	       AddText(text, text_length, wshandling);
	       CloseElement();

		 @param name Name of element to create.
	     @param text Text to add.  Need not be null terminated if its
	                 length is specified.
	     @param text_length Length of 'text' or ~0u to calculate
	                        length using uni_strlen.
	     @param wshandling Whitespace handling.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS AddBinaryData(const XMLExpandedName &name, const char *data, unsigned length);
	/**< Adds an element named 'name' containing the binary data
	     encoded using base64 encoding.  Calling this function
	     corresponds to the sequence

	       OpenElement(name);
	       AddText(base64);
	       CloseElement();

	     where base64 is the string resulting from converting data.

	     Note: this is the only way to add data that can contain null
	     characters and should always be used when storing data that
	     is not actually text.

	     @param name Name of element to create.
	     @param data Binary data to add.
	     @param length Number of bytes in 'data'.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS AddBinaryData(const XMLExpandedName &name, const ByteBuffer &buffer);
	/**< Like

	       AddBinaryData(const XMLExpandedName &, const char *, unsigned)

	     but adds all data from the supplied buffer.

	     @param name Name of element to create.
	     @param buffer Buffer containing data to add.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_BOOLEAN GetBinaryData(const XMLExpandedName &name, char *&data, unsigned &length);
	/**< Retrieves base64 encoded binary data from the next element,
	     if that element is named 'name'.  Calling this function
	     corresponds to the code

	       if (EnterElement(name))
	       {
	         TempBuffer buffer;
	         GetAllText(buffer);
	         // decode base64 into data/length
	         return OpBoolean::IS_TRUE;
	       }
	       else
	         return OpBoolean::IS_FALSE;

	     The returned data is stored in an array allocated with new
	     char[] that must be freed by the caller using delete[].

	     Note: only you know that the contents of the element really
	     is base64 encoded binary data.  This function only knows if
	     it isn't, and returns OpStatus::ERR if it isn't.

	     @param name Name of element.
	     @param data Set to an allocated array, on success.
	     @param length Set to the number of bytes in 'data' on
	                   success.

	     @return OpBoolean::IS_TRUE on success, OpBoolean::IS_FALSE if
	             the next element is not named 'name', OpStatus::ERR
	             if base64 decoding failed and OpStatus::OUT_OF_MEMORY
	             on OOM. */

	OP_BOOLEAN GetBinaryData(const XMLExpandedName &name, ByteBuffer &buffer);
	/**< Like

	       GetBinaryData(const XMLExpandedName &, char *&, unsigned &)

	     but appends the extracted data to the supplied buffer.

	     @param name Name of element.
	     @param buffer Buffer to which data is appended on success.

	     @return OpBoolean::IS_TRUE on success, OpBoolean::IS_FALSE if
	             the next element is not named 'name', OpStatus::ERR
	             if base64 decoding failed and OpStatus::OUT_OF_MEMORY
	             on OOM. */

	OP_STATUS AddFragment(XMLFragment *fragment);
	/**< Adds all content from a fragment (regardless of the fragments
	     current element or position.)  The argument fragment will be
	     empty after the operation.  Note: if the first or last
	     inserted content is text, and it is inserted right after or
	     right before text, that adjacent text content is merged.

	     After the operation, position will be after the last inserted
	     content, or, if the last inserted content was text that was
	     merged with adjacent text, after the merged text content.
	     The current element will not be changed.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	class GetXMLOptions
	{
	public:
		GetXMLOptions(BOOL include_xml_declaration)
			: include_xml_declaration(include_xml_declaration),
			  encoding(NULL),
			  format_pretty_print(FALSE),
#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
			  canonicalize(CANONICALIZE_NONE),
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
			  scope(SCOPE_WHOLE_FRAGMENT)
		{
		}

		BOOL include_xml_declaration;
		/**< If TRUE, an XML declaration is appended before the rest
		     of the content, saying this is XML 1.0 and what encoding
		     it uses, if an encoding is supplied.  Note: if an XML
		     declaration is requested, and no encoding is supplied,
		     the resulting text will in fact not be a well- formed
		     external parsed entity (it will not match the
		     extParsedEnt production,) since its XML declaration will
		     not be a valid text declaration (it will not match the
		     TextDecl production.)  If this is important to you, don't
		     include the XML declaration or do supply an encoding. */

		const char *encoding;
		/**< If not NULL, characters in character data and attribute
		     values that are not possible to represent in that
		     encoding are escaped using character references.  Any
		     such characters in element names, attribute names,
		     processing instruction targets or data or in any other
		     markup besides comments will cause a serialization
		     failure and the GetXML() or GetEncodedXML() function will
		     return OpStatus::ERR.  Unrepresentable characters in
		     comments will be replaced by question marks.

		     Note: when GetXML() is used the result will still be an
		     ordinary unicode string, no actual conversion to the
		     specified encoding will be performed.  But the unicode
		     string will be guaranteed to be convertible into the
		     specified encoding without complications. */

		BOOL format_pretty_print;
		/**< If TRUE, the generated XML will indented according to
		     simple rules.  No character data will be modified inside
		     elements with xml:space="preserve".  Note that there is
		     no guarantee that this option doesn't modify the meaning
		     of the document according to whatever content language
		     the document is in. */

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
		enum Canonicalize
		{
			CANONICALIZE_NONE,
			/**< The default; the generated XML will not be
			     canonicalized, instead it will be output as is, or
			     pretty printed if requested, and with namespace
			     declarations normalized as required to preserve
			     document semantics. */

			CANONICALIZE_WITHOUT_COMMENTS,
			/**< Canonicalize without comments. */

			CANONICALIZE_WITH_COMMENTS,
			/**< Canonicalize with comments.  The function
			     SetStoreCommentsAndPIs() must be used, or else there
			     will not be any comments to include. */

			CANONICALIZE_WITHOUT_COMMENTS_EXCLUSIVE,
			/**< Same as CANONICALIZE_WITHOUT_COMMENTS but using the
			     "Exclusive" algorithm. */

			CANONICALIZE_WITH_COMMENTS_EXCLUSIVE
			/**< Same as CANONICALIZE_WITH_COMMENTS but using the
			     "Exclusive" algorithm. */
		};

		Canonicalize canonicalize;
		/**< If not CANONICALIZE_NONE, the generated XML will follow
		     the rules in 'Canonical XML 1.0'.  This overrides all the
		     other options, since canonical XML cannot include an XML
		     declaration, must use UTF-8 and shouldn't be pretty
		     printed in any way.

		     Note that in practice, 'Canonical XML 1.0' compatible
		     serialization can only be performed on documents parsed
		     while loading external entities, and that none of the
		     Parse() functions in XMLFragment typically does this.
		     But if the document is standalone (does not have an
		     external DTD subset, or reference any external entities
		     declared in the internal DTD subset,) this of course
		     doesn't matter as the serialization will be canonical in
		     any case.

		     Also note that 'Canonical XML 1.0' defines its own rules
		     for how to serialize XML namespace declarations, that in
		     practice means that the normal automatic fixup of such
		     attributes made by the XML serializer is disabled.  This
		     means that the serialized fragment must contain all the
		     XML namespace declaration attributes itself, or the
		     result of the serialization will not be well-formed, or
		     will mean something else than it was meant to mean. */
#endif // XMLUTILS_CANONICAL_XML_SUPPORT

		enum Scope {
			SCOPE_WHOLE_FRAGMENT,
			/**< All content in the fragment, regardless of current
			     element or position. */
			SCOPE_CURRENT_ELEMENT_INCLUSIVE,
			/**< The current element is rendered as the root element
			     of the generated XML code. */
			SCOPE_CURRENT_ELEMENT_EXCLUSIVE
			/**< The children of the current element are rendered.
			     This can lead to a result that is not a well-formed
			     XML document, since it may produce more than one
			     top-level element and/or top-level character data. */
		};

		Scope scope;
		/**< Scope of serialization.

		     When SCOPE_CURRENT_ELEMENT_INCLUSIVE or
		     SCOPE_CURRENT_ELEMENT_EXCLUSIVE are combined with the
		     'canonicalize' option, attributes on ancestor elements of
		     the current element (and in the latter case on the
		     current element itself) may affect the result even though
		     those elements are not included in the result, as
		     specified in 'Canonical XML 1.0'. */
	};

	OP_STATUS GetXML(TempBuffer &buffer, const GetXMLOptions &options);
	/**< Appends all the contents of this fragment (ignoring current
	     element and position) as a well-formed XML entity (and
	     document, if there is only a single top-level element and no
	     non-whitespace top-level character data) to 'buffer'.

	     @param buffer Buffer into which the result is appended.
	     @param options Serialization options.

	     @return OpStatus::OK, OpStatus::ERR if the serialization
	             fails or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS GetXML(TempBuffer &buffer, BOOL include_xml_declaration, const char *encoding = NULL, BOOL format_pretty_print = FALSE);
	/**< Appends all the contents of this fragment (ignoring current
	     element and position) as a well-formed XML entity (and
	     document, if there is only a single top-level element and no
	     non-whitespace top-level character data) to 'buffer'.

	     See the documentation for the corresponding members of the
	     class GetXMLOptions for more information about the arguments.

	     @param buffer Buffer into which the result is appended.
	     @param include_xml_declaration If TRUE, an XML declaration
	                                    is included.
	     @param encoding A character encoding name or NULL.
	     @param format_pretty_print If TRUE, the output is reformatted
	                                by adding white-space.

	     @return OpStatus::OK, OpStatus::ERR on if 'encoding' is
	             invalid or if the serialization failed, or
	             OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS GetEncodedXML(ByteBuffer &buffer, const GetXMLOptions &options);
	/**< Like GetXML, but converts the result to the specified
	     encoding after the serialization.

	     @param buffer Buffer into which the result is appended.
	     @param options Serialization options.

	     @return OpStatus::OK, OpStatus::ERR if the serialization
	             fails or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS GetEncodedXML(ByteBuffer &buffer, BOOL include_xml_declaration, const char *encoding = "UTF-8", BOOL format_pretty_print = FALSE);
	/**< Like GetXML, but converts the result to the specified
	     encoding after the serialization.

	     @param buffer Buffer into which the encoded result is
	                   appended.
	     @param include_xml_declaration If TRUE, an XML declaration
	                                    is included.
	     @param encoding A character encoding name.
	     @param format_pretty_print If TRUE, the output is reformatted
	                                by adding white-space.

	     @return OpStatus::OK, OpStatus::ERR on if 'encoding' is
	             invalid or if the serialization failed, or
	             OpStatus::ERR_NO_MEMORY on OOM. */

#ifdef XMLUTILS_XMLFRAGMENT_XMLTREEACCESSOR_SUPPORT
	/* Enabled by importing API_XMLUTILS_XMLFRAGMENT_XMLTREEACCESSOR. */

	OP_STATUS CreateXMLTreeAccessor(XMLTreeAccessor *&treeaccessor);
	/**< Creates an XMLTreeAccessor instance that accesses the tree
	     represented by this fragment.  The tree accessor is "live"
	     meaning any future changes made to the fragment are reflected
	     by it, but typically tree accessors are assumed to access
	     immutable trees, so it's best to finish the fragment and then
	     create and use a tree accessor.

	     The created object is owned by the caller and must be
	     destroyed using FreeXMLTreeAccessor().  The tree accessor
	     must not be used after the fragment has been destroyed, but
	     can be destroyed after it.

	     @param treeaccessor Set to the created object on success.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static void FreeXMLTreeAccessor(XMLTreeAccessor *treeaccessor);
	/**< Destroys an XMLTreeAccessor instance previously created using
	     CreateXMLTreeAccessor().

	     @param treeaccessor An XMLTreeAccessor instance create by
	                         CreateXMLTreeAccessor(), or NULL. */
#endif // XMLUTILS_XMLFRAGMENT_XMLTREEACCESSOR_SUPPORT

#ifdef XMLUTILS_XMLFRAGMENT_XPATH_SUPPORT
	/* Enabled by importing API_XMLUTILS_XMLFRAGMENT_XPATH. */

	OP_STATUS EvaluateXPathToNumber(double &result, const uni_char *expression, XMLNamespaceDeclaration *namespaces = NULL);
	/**< Evaluate the expression and convert the result to a number as
	     by the XPath function number().  If the expression is invalid
	     or the evaluation fails, OpStatus::ERR is returned.  The
	     context node for the evaluation is the current element, or
	     the root node if there is no current element.

	     If 'namespaces' is not NULL, it is used to resolve any
	     namespace prefixes in the expression.  If 'namespaces' is
	     NULL and there are qualified names in the expression, or if
	     any prefix in the expression is not declared, OpStatus::ERR
	     is returned.

	     @param result Set to TRUE or FALSE on success, otherwise not
	                   modified.

	     @return OpStatus::OK, OpStatus::ERR on XPath errors or
	             OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS EvaluateXPathToBoolean(BOOL &result, const uni_char *expression, XMLNamespaceDeclaration *namespaces = NULL);
	/**< Evaluate the expression and convert the result to a boolean
	     as by the XPath function boolean().  If the expression is
	     invalid or the evaluation fails, OpStatus::ERR is returned.
	     The context node for the evaluation is the current element,
	     or the root node if there is no current element.

	     If 'namespaces' is not NULL, it is used to resolve any
	     namespace prefixes in the expression.  If 'namespaces' is
	     NULL and there are qualified names in the expression, or if
	     any prefix in the expression is not declared, OpStatus::ERR
	     is returned.

	     @param result Set to TRUE or FALSE on success, otherwise not
	                   modified.

	     @return OpStatus::OK, OpStatus::ERR on XPath errors or
	             OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS EvaluateXPathToString(OpString &result, const uni_char *expression, XMLNamespaceDeclaration *namespaces = NULL);
	/**< Evaluate the expression and convert the result to a string as
	     by the XPath function string().  If the expression is invalid
	     or the evaluation fails, OpStatus::ERR is returned.  The
	     context node for the evaluation is the current element, or
	     the root node if there is no current element.

	     If 'namespaces' is not NULL, it is used to resolve any
	     namespace prefixes in the expression.  If 'namespaces' is
	     NULL and there are qualified names in the expression, or if
	     any prefix in the expression is not declared, OpStatus::ERR
	     is returned.

	     @param result Set to TRUE or FALSE on success, otherwise not
	                   modified.

	     @return OpStatus::OK, OpStatus::ERR on XPath errors or
	             OpStatus::ERR_NO_MEMORY on OOM. */
#endif // XMLUTILS_XMLFRAGMENT_XPATH_SUPPORT
};

#endif // XMLUTILS_XMLFRAGMENT_SUPPORT
#endif // XMLFRAGMENT_H
