/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef __UPLOAD2_H__
#define __UPLOAD2_H__

#include "modules/util/simset.h"
#include "modules/util/opstring.h"
#include "modules/util/opfile/opfile.h"
#include "modules/url/url2.h"
#include "modules/util/twoway.h"

#define UPLOAD_LONG_BINARY_BUFFER_SIZE 1000000
#define UPLOAD_MIN_BUFFER_SIZE_WHEN_LONG (128*1024)

#define UPLOAD_BASE64_LINE_LEN	64

class Header_Boundary_Parameter;

class Boundary_List;
struct KeywordIndex;

/** Specifies which separators should be used between headers or arguments */
enum Header_Parameter_Separator {
	SEPARATOR_NONE, 
	SEPARATOR_SPACE, 
	SEPARATOR_NEWLINE, 
	SEPARATOR_COLON, // In Header name only: Implies CRLF after header
	SEPARATOR_COMMA, 
	SEPARATOR_SEMICOLON, 
	SEPARATOR_COMMA_NEWLINE, 
	SEPARATOR_SEMICOLON_NEWLINE
};

/** What encoding will be used 
 *  Autodetect chooses the proper mode based on the transfer mode
 */
enum MIME_Encoding {
	ENCODING_NONE, 
	ENCODING_AUTODETECT, 
	ENCODING_QUOTED_PRINTABLE, 
	ENCODING_BASE64, 
	ENCODING_7BIT, 
	ENCODING_8BIT, 
	ENCODING_BINARY
};

/** Where should the header be inserted relative to an item? */
enum Header_InsertPoint {
	HEADER_INSERT_FRONT, 
	HEADER_INSERT_BEFORE,  
	HEADER_INSERT_SORT_BEFORE,  
	HEADER_INSERT_SORT_AFTER, 
	HEADER_INSERT_AFTER, 
	HEADER_INSERT_LAST
};

/** How is the input buffer going to be used? */
enum Upload_Buffer_Ownership {
	UPLOAD_TAKE_OVER_BUFFER, 
	UPLOAD_COPY_BUFFER, 
	UPLOAD_REFERENCE_BUFFER,
	UPLOAD_COPY_EXTRACT_HEADERS
};

/** What kind of restrictions are placed on this upload?  */
enum Upload_Transfer_Mode {
	UPLOAD_BINARY_NO_CONVERSION,
	UPLOAD_BINARY_TRANSFER,
	UPLOAD_7BIT_TRANSFER,
	UPLOAD_8BIT_TRANSFER
};

/** Boundary search status */
enum Boundary_Compare_Status {
	Boundary_Not_Found,  // No boundary found
	Boundary_Partial_Match, // Part of the boundary found at end of buffer
	Boundary_Match // Found boundary in buffer;
};

/** Bodunary kind */
enum Boundary_Kind {
	Boundary_First,
	Boundary_Internal,
	Boundary_Last,
	Boundary_First_Last
};

/** Type of data in upload element */
enum Upload_Element_Type {
	UPLOAD_BINARY_BUFFER,
	UPLOAD_EXTERNAL_BUFFER,
	UPLOAD_STRING8,
	UPLOAD_STRING16,
	UPLOAD_ENCAPSULATED,
	UPLOAD_EXTERNAL_BODY,
	UPLOAD_MULTIPART,
	UPLOAD_URL,
	UPLOAD_TEMPORARY_URL,
	UPLOAD_PGP_CLEAR_SIGNED,
	UPLOAD_PGP_ENCRYPTED
};

/** When an externally generated header is parsed, which headers are permitted? */
enum Header_Protection_Policy {
	HEADER_ONLY_REMOVE_CUSTOM,
	HEADER_REMOVE_HTTP, // plus custom
	HEADER_REMOVE_HTTP_CONTENT_TYPE, // plus custom
	HEADER_REMOVE_BCC // plus custom
};

typedef MIME_Encoding Header_Encoding;

/** Upload error codes */
class Upload_Error_Status : public OpStatus
{
public:
	enum {
		/** We actually found the boundary prefix string in the body of one of the elements, and must find a new one */
		ERR_FOUND_BOUNDARY_PREFIX = USER_ERROR + 100
	};
};


/** Boundary specification, twoway pointer linked to Header_Boundary_Parameter class
 *
 *	Boundaries are strings starting with 10 "-"es, followed by a number of random 
 *	alphanumeric characters picked from the 62 first Base64 encoding characters, resulting in 
 *	a string at least 24 characters long.
 *
 *	Boundaries are generally created from a 24 character template, which has been used while 
 *	scanning the content to be encoded to prevent collisions. The actual boundary is then created 
 *	by appending 8 more random characters. 
 *
 *	The Boundary_List class which is used to adminsitrate the generation makes sure that no boundaries collide. 
 */
class Boundary_Item : public Link, public TwoWayPointer_Target
{
public:
	enum {
		/** Signal to Header_Boundary_Parameter owner to indicate that the boundary has been generated, and that the owner should take the necessary preparation steps */
		BOUNDARY_GENERATE_ACTION = TWOWAYPOINTER_ACTION_USER + 0
	};

private:
	/** Actual boundary string */
	OpString8	boundary_string;
	/** Cached boundary string length */
	uint32		boundary_string_length;

public:
	/** Destructor */
	virtual ~Boundary_Item();

	/** Generate a boundary either based on the template, or to the specified length (default 24)
	 *
	 *	@param	base	If non-NULL the boundary of this element will be used as a prefix for the boundary, and 8 bytes will be added
	 *	@param	len		If base is NULL and this value is larger than 24 this will be the length of the boundary
	 */
	void GenerateL(Boundary_List *base, int len=0);

	/** @return the boundary string */
	OpStringC8 &Boundary() { return boundary_string; }
	/** Clear the boundary string */
	void Clear() { boundary_string.Empty(); }

	/** Calculate the length of a boundary
	 *	@param	boundary_kind	What kind of boundary is being written (the first, last or an internal). This
	 *							decides how many CRLFs and extra dashes ("-") are written.
	 *	@return uint32			Length of this kind of boundary
	 */
	uint32 Length(Boundary_Kind boundary_kind);

	/** Write the boundary to the given buffer 
	 *
	 *	@param	target			target buffer
	 *	@param	remaining_len	On entry the number of characters in the buffer, MUST be at least 
	 *							the boundary length + 8 characters long.
	 *							On exit, the remaining length of the buffer after the boundary has been written
	 *	@param	boundary_kind	What kind of boundary is being written (the first, last or an internal). This 
	 *							decides how many CRLFs and extra dashes ("-") are written.
	 *
	 *	@return	unsigned char *	Pointer to location where next character is to be written, immediately after the boundary
	 */
	unsigned char *WriteBoundary(unsigned char *target, uint32 &remaining_len, Boundary_Kind boundary_kind);

	/** Scan the input buffer in order to find any copies of this boundary in the buffer. 
	 *	
	 *	@param	buffer		Buffer to scan
	 *	@param	buffer_len	Length of buffer
	 *	@param	committed	How much of the buffer has been read and conclusively declared free of the boundary?
	 *						If this is  less than the length of the buffer, it indicates that the end of the 
	 *						buffer contains a prefix of the boundary
	 *
	 *	@return	Boundary_Compare_Status		What was the result of the scan? None found, a full match or just a partial match at the end?
	 */
	Boundary_Compare_Status ScanForBoundary(const unsigned char *buffer, uint32 buffer_len, uint32 &committed) ;

	Boundary_Item *Suc(){return (Boundary_Item *) Link::Suc();};
	Boundary_Item *Pred(){return (Boundary_Item *) Link::Pred();};
};

/** Contains the list of boundaries encountered by the upload preparation routines
 *	Upon successful completion of the preparation the boundary contained in this item 
 *	will be used as a template for the generated boundaries.
 */
class Boundary_List : public Head
{
private:
	/** Boundary template */
	Boundary_Item base_boundary;
	int base_boundary_length;

	/** Generates the boundary template on demand */
	void GenerateBaseBoundaryL(){if (base_boundary.Boundary().IsEmpty()) base_boundary.GenerateL(NULL, base_boundary_length);}

public:
	/** Destructor */
	virtual ~Boundary_List();

	/** Initiales the boundary template with the specified length (at least 24 characters) */
	void InitL(int len=24){base_boundary.Clear(); base_boundary_length=len;};

	/** Use list of boundaries to create unique boundaries, based on this prefix */
	void GenerateL();

	/** Get the value of the boundary template */
	OpStringC8 &GetBoundaryL(){GenerateBaseBoundaryL(); return base_boundary.Boundary();}

	Boundary_Item *First(){return (Boundary_Item *) Head::First();};
	Boundary_Item *Last(){return (Boundary_Item *) Head::Last();};

	/** Scan the input buffer in order to find any copies of this boundary in the buffer. 
	 *	
	 *	@param	buffer		Buffer to scan
	 *	@param	buffer_len	Length of buffer
	 *	@param	committed	How much of the buffer has been read and conclusively declared free of the boundary?
	 *						If this is  less than the length of the buffer, it indicates that the end of the 
	 *						buffer contains a prefix of the boundary
	 *
	 *	@return	Boundary_Compare_Status		What was the result of the scan? None found, a full match or just a partial match at the end?
	 */
	Boundary_Compare_Status ScanForBoundaryL(const unsigned char *buffer, uint32 buffer_len, uint32 &committed){GenerateBaseBoundaryL(); return base_boundary.ScanForBoundary(buffer, buffer_len, committed);}
};

/** The API for the individual parameters in a header
 *	A single header may have multiple elements derived from this class
 */
class Header_Parameter_Base : public Link
{
public:
	/** Destructor */
	virtual ~Header_Parameter_Base();

	/** Returns length of parameter string (excluding null terminator) */
	virtual uint32 CalculateLength() const = 0;

	/** Writes the parameters to the string, which is assumed to be long enough. 
	 *	Returns pointer to the next position. Null terminates string.
	 */
	virtual char *OutputParameters(char *target) const = 0;

	/** Is this object empty (no parameter(s)) ? */
	virtual BOOL IsEmpty()=0;

	Header_Parameter_Base *Suc(){return (Header_Parameter_Base *) Link::Suc();};
	Header_Parameter_Base *Pred(){return (Header_Parameter_Base *) Link::Pred();};
};

/** Header_Parameter 
 *
 *	Header parameter Item
 *
 *	Output example: foo=bar
 *
 *	Empty Name (NULL or empty string) -> Value is entire string : bar
 *	Empty Value (NULL) -> No assignment character : foo
 *	Empty Value (empty string) -> Assignment character added: foo=
 */
class Header_Parameter : public Header_Parameter_Base
{
private:
	OpStringS8 parameter_name;
	OpString8 parameter_value;

public:

	/** Constructor */
	Header_Parameter();
	/** Destructor */
	virtual ~Header_Parameter();

	/** opaque value initialization*/
	void InitL(const OpStringC8 &p_value);

	/** name and value intialization, optional quoting */
	void InitL(const OpStringC8 &p_name, const OpStringC8 &p_value, BOOL quote_value = FALSE);

	/** Get the name of the parameter */
	OpStringC8 &GetName(){return parameter_name;}
	/** Get the value of the parameter */
	OpStringC8 &GetValue(){return parameter_value;}

	/** Update the name of the parameter */
	void SetNameL(const OpStringC8 &p_name);
	/** Update the value of the parameter, optionally adding qoutes */
	void SetValueL(const OpStringC8 &p_value, BOOL quote_value = FALSE);

	/** Returns length of parameter string (excluding null terminator) */
	virtual uint32 CalculateLength() const;

	/** Writes the parameters to the string, which is assumed to be long enough. 
	 *	Returns pointer to the next position. Null terminates string.
	 */
	virtual char *OutputParameters(char *target) const;

	/** Is this object empty (no parameter(s)) ? */
	virtual BOOL IsEmpty();
};

/** Header_Parameter_Collection 
 *
 *	Contains a collection of parameters, separated by the specified separator
 *
 */
class Header_Parameter_Collection : public Header_Parameter_Base, public AutoDeleteHead
{
private:
	/** The type of separators used between elements (but not after the last, which must be added separately*/
	Header_Parameter_Separator separator;

public:

	/** Construct for the given separator type */
	Header_Parameter_Collection(Header_Parameter_Separator sep);

	/** Set the separator type */
	void SetSeparator(Header_Parameter_Separator sep);

	/** opaque value initialization (format unknown) */
	void AddParameterL(const OpStringC8 &p_value);

	/** name and value intialization, optional quoting */
	void AddParameterL(const OpStringC8 &p_name, const OpStringC8 &p_value, BOOL quote_value = FALSE);

	/** Takes over param */
	void AddParameter(Header_Parameter_Base *param);


	/** Returns length of parameter string (excluding null terminator) */
	virtual uint32 CalculateLength() const;

	/** Writes the parameters to the string, which is assumed to be long enough. 
	 *	Returns pointer to the next position. Null terminates string.
	 */
	virtual char *OutputParameters(char *target) const;

	/** Returns length of separator string */
	virtual uint32 SeparatorLength() const;

	/** Writes the separator to the string, which is assumed to be long enough. 
	 *	Returns pointer to the next position. Null terminates string.
	 */
	virtual char *OutputSeparator(char *target) const;

	/** Is this object empty (no parameter(s)) ? */
	virtual BOOL IsEmpty();


	Header_Parameter_Base *First() const {return (Header_Parameter_Base *) Head::First();};
	Header_Parameter_Base *Last() const {return (Header_Parameter_Base *) Head::Last();};
};

#ifdef URL_UPLOAD_QP_SUPPORT
/** Creates a parameter with qouted printable value  (RFC 2047)*/
class Header_QuotedPrintable_Parameter : public Header_Parameter
{
public:
	/** name and value intialization
	 *
	 *	@param	p_name		name of parameter
	 *	@param	p_value		value of parameter
	 *	@param	charset		charset to use in QP encoding
	 *	@param	encoding	Which encoding (auto, QP or Base64) to use in generation
	 *	@param	quote_if_not_encoded	if TRUE, and if no encoding is perform add qoutes around the value 
	 */
	void InitL(const OpStringC8 &p_name, const OpStringC8 &p_value, const OpStringC8 &charset, Header_Encoding encoding=ENCODING_AUTODETECT, BOOL quote_if_not_encoded=FALSE);

	/** name and value intialization
	 *
	 *	@param	p_name		name of parameter
	 *	@param	p_value		value of parameter
	 *	@param	charset		charset to use in QP encoding
	 *	@param	encoding	Which encoding (auto, QP or Base64) to use in generation
	 *	@param	quote_if_not_encoded	if TRUE, and if no encoding is perform add qoutes around the value 
	 */
	void InitL(const OpStringC8 &p_name, const OpStringC16 &p_value, const OpStringC8 &charset, Header_Encoding encoding=ENCODING_AUTODETECT, BOOL quote_if_not_encoded=FALSE);
};
#endif

#ifdef URL_UPLOAD_RFC_2231_SUPPORT
/** Creates a parameter with RFC 2231 qouted printable value 
 *	Language support is presently not available
 */
class Header_RFC2231_Parameter : public Header_Parameter_Collection
{
public:
	/** Constructor */
	Header_RFC2231_Parameter();

	/** name and value intialization
	 *
	 *	@param	p_name		name of parameter
	 *	@param	p_value		value of parameter
	 *	@param	charset		charset to use in QP encoding
	 */
	void InitL(const OpStringC8 &p_name, const OpStringC8 &p_value, const OpStringC8 &charset);

	/** name and value intialization
	 *
	 *	@param	p_name		name of parameter
	 *	@param	p_value		value of parameter
	 *	@param	charset		charset to use in QP encoding
	 */
	void InitL(const OpStringC8 &p_name, const OpStringC16 &p_value, const OpStringC8 &charset);

	enum RFC2231_SpecialCharacter {
		RFC2231_NOT_SPECIAL,
		RFC2231_QUOTABLE,
		RFC2231_ESCAPABLE
	};

	/**
	 *	@param	token		input character
	 *	@return an RFC2231_SpecialCharacter describing what kind of encoding token needs
	 */
	static RFC2231_SpecialCharacter IsRFC2231Special(unsigned char token);

	/** check parameter value for special characters and update charset
	 *
	 *	@param	p_value		input parameter value
	 *	@param	specials_type	returned RFC2231_SpecialCharacter describing what kind of encoding the parameter value needs
	 *	@param	charset		(non-const!) will be set to empty if the charset is not needed for the encoding
	 */
	static void CheckRFC2231Special(const OpStringC8 &p_value, RFC2231_SpecialCharacter &specials_type, OpStringC8 &charset);
};
#endif

/** Header_Boundary_Parameter 
 *	When the boundary is generated this object is signalled, and creates the parameter's value
 */
class Header_Boundary_Parameter : public Header_Parameter, public TwoWayAction_Target
{
private:
	/** Boundary generator */
	TwoWayPointer_WithAction<Boundary_Item> boundary;
	/** Add qoutes around boundary value? */
	BOOL quote_boundary;

public:
	/** Constructor */
	Header_Boundary_Parameter();
	/** Destructor */ 
	virtual ~Header_Boundary_Parameter();

	/** name and boundary intialization
	 *
	 *	@param	p_name		name of parameter
	 *	@param	boundary	boundary generator, owned by caller
	 *	@param	quote_value	Add qoutes around value;
	 */
	void InitL(const OpStringC8 &p_name, Boundary_Item *boundary, BOOL quote_value = FALSE);

	/** Action function for Boundary generation action */
	virtual void TwoWayPointerActionL(TwoWayPointer_Target *action_source, uint32 action_val);
};

/** Header_Item
 *  Single header with name and parameter collection */
class Header_Item : public Link
{
public:
	enum Temp_Disabled_State
	{
		TEMP_DISABLED_NO = 0,			// Normal operation
		TEMP_DISABLED_YES,				// Temporary disabled
		TEMP_DISABLED_SIGNAL_REMOVE		// Replaces parameters with "-"
	};

private:
	/** Name of header */
	OpStringS8 header_name;

	/** Separator between name and value; SEPARATOR_COLON or SEPARATOR_SPACE are the only accepted values, SEPARATOR_COLON is default*/
	Header_Parameter_Separator name_separator; 

	/** Is this header activated? If not, no output will be generated */
	BOOL enabled;

	/** Is this header temporarily disabled? State automatically reset after next call to OutputHeader */
	Temp_Disabled_State temp_disabled;

	/** Parameter collection */
	Header_Parameter_Collection	parameters;

	/** TRUE if the header is set by a user of url api, and not url itself **/
	BOOL is_external;


public:

	/** Construct with given separator for parameter collection
	 *
	 * @param sep separator the separator
	 * @param  is_external  Set to TRUE if this header is set by an external
	 * 									caller of URL, and not internally in url
	 * 									implementation.
	 **/
	Header_Item(Header_Parameter_Separator sep=SEPARATOR_SEMICOLON, BOOL is_external = FALSE);

	/** Desctuctor */
	virtual ~Header_Item();

	/** Set separator for parameter collection */
	void SetSeparator(Header_Parameter_Separator sep);

	/** Set separator between name and value; SEPARATOR_COLON or SEPARATOR_SPACE are the only accepted values */
	void SetNameSeparator(Header_Parameter_Separator sep){name_separator = sep;}
	/** Get the name separator used */
	Header_Parameter_Separator GetNameSeparator() const{return name_separator;}

	/** opaque value initialization (format unknown)*/
	void AddParameterL(const OpStringC8 &p_value);

	/** name and value intialization, optional quoting */
	void AddParameterL(const OpStringC8 &p_name, const OpStringC8 &p_value, BOOL quote_value = FALSE);

	/* Takes over param */
	void AddParameter(Header_Parameter_Base *param);

	/** opaque value initialization (format unknown). Clears parameters*/
	void InitL(const OpStringC8 &h_name);

	/** name and value intialization. Clears parameters */
	void InitL(const OpStringC8 &h_name, const OpStringC8 &p_value);

	/** Is this header active? */
	BOOL IsEnabled(){return enabled;}

	/** Remove all parameters */
	void ClearParameters(){parameters.Clear();}

	/** Activate (or disable) this header */
	void SetEnabled(BOOL flag){enabled = flag;}

	/** Is this header temporarily disabled? State automatically reset after next call to OutputHeader() */
	void SetTemporaryDisable(Temp_Disabled_State state) { temp_disabled = state; }

	/** Get Name of header */
	OpStringC8 &GetName(){return header_name;}

	/** Returns length of name and parameter string, including separators, excluding null terminator */
	virtual uint32 CalculateLength();

	/** Does this header have any parameters? */
	unsigned int HasParameters(){return !parameters.Empty();}

	/** Writes the parameters with the header name to the string, which is assumed to be long enough. 
	 *	Returns pointer to the next position. Null terminates string.
	 */
	virtual char *OutputHeader(char *target);

	/** Writes the parameters without the header name to the string, which is assumed to be long enough.
	 *  Returns pointer to the next position. Null terminates string.
	 */
	virtual char *GetValue(char *target, BOOL add_crlf = FALSE);

	/** Get the const reference to parameters */
	const Header_Parameter_Collection &GetParameters() const { return parameters; }

	/** return TRUE if the header is set by a
	 * user of url api, and not the inner URL implementation **/
	BOOL IsExternal(){ return is_external; }

	Header_Item *Suc(){return (Header_Item *) Link::Suc();};
	Header_Item *Pred(){return (Header_Item *) Link::Pred();}
};

/** Header_List
 *
 *  List of headers
 *
 *	Unless specified otherwise operations on named headers are performed on the LAST header with the given name
 */
class Header_List : public AutoDeleteHead
{
public:
	/** Constructor */
	Header_List();

	/** Set parameter separator */
	void SetSeparator(Header_Parameter_Separator sep);
	
	/** Initalizes with a NULL terminated array of header names*/
	void InitL(const char **header_names=NULL, Header_Parameter_Separator sep=SEPARATOR_SEMICOLON);

	Header_Item *InsertHeaderL(const OpStringC8 &new_header_name, Header_InsertPoint insert_point = HEADER_INSERT_LAST, Header_Parameter_Separator sep=SEPARATOR_SEMICOLON, const OpStringC8 &header_ref=OpStringC8(NULL));

	/** Insert a Header_Item in the list, at the given point.
	 *
	 *	@param	new_header			The header to be inserted, this object takes ownership of the item
	 *	@param	insert_point		Where to insert the header, default is at the end of the list 
	 *	@param	header_ref			Name of header used as reference point for insertion (if required by the insert point specification) 
	 */
	void InsertHeader(Header_Item *new_header, Header_InsertPoint insert_point = HEADER_INSERT_LAST, const OpStringC8 &header_ref=OpStringC8(NULL));

	/** Insert a list of Header_Items in the list, at the given point.
	 *
	 *	@param	new_header			The list of headers to be inserted, this object takes ownership of the items
	 *	@param	insert_point		Where to insert the header, default is at the end of the list 
	 *	@param	header_ref			Name of header used as reference point for insertion (if required by the insert point specification) 
	 *	@param	remove_existing		If TRUE, then headers existing in both new_header and "this" are removed from "this" before the insertion.
	 */
	void InsertHeaders(Header_List &new_header, Header_InsertPoint insert_point = HEADER_INSERT_LAST, const OpStringC8 &header_ref=OpStringC8(NULL), BOOL remove_existing=FALSE);

	/** Change the separator of the named header (if necessary it will be created)
	 *
	 *	@param	header_name		Name of header
	 *	@param	sep				Parameter Separator to use
	 */
	void SetSeparatorL(const OpStringC8 &header_name, Header_Parameter_Separator sep);

	/** opaque value initialization of named header (free format value)*/
	void AddParameterL(const OpStringC8 &header_name, const OpStringC8 &p_value);

	/** name and value parameter intialization, optional quoting, in named header */
	void AddParameterL(const OpStringC8 &header_name, const OpStringC8 &p_name, const OpStringC8 &p_value, BOOL quote_value = FALSE);

	/** Adds the "param" argument to the parameters held by the named header*/
	void AddParameterL(const OpStringC8 &header_name, Header_Parameter_Base *param);

	/** Add a qouted printable parameter to the named header 
	 *
	 *	@param	header_name		Name of header
	 *	@param	p_name			name of parameter
	 *	@param	p_value			value of parameter
	 *	@param	charset			charset to use in QP encoding
	 *	@param	encoding		Which encoding (auto, QP or Base64) to use in generation
	 *	@param	quote_if_not_encoded	if TRUE, and if no encoding is perform add qoutes around the value 
	 */
	void AddQuotedPrintableParameterL(const OpStringC8 &header_name, const OpStringC8 &p_name, const OpStringC8 &p_value,const  OpStringC8 &charset, Header_Encoding encoding=ENCODING_NONE);

	/** Add a qouted printable parameter to the named header 
	 *
	 *	@param	header_name		Name of header
	 *	@param	p_name			name of parameter
	 *	@param	p_value			value of parameter
	 *	@param	charset			charset to use in QP encoding
	 *	@param	encoding		Which encoding (auto, QP or Base64) to use in generation
	 *	@param	quote_if_not_encoded	if TRUE, and if no encoding is perform add qoutes around the value 
	 */
	void AddQuotedPrintableParameterL(const OpStringC8 &header_name, const OpStringC8 &p_name, const OpStringC16 &p_value, const OpStringC8 &charset, Header_Encoding encoding=ENCODING_NONE);

	/** Add a RFC 2231 qouted printable parameter to the named header 
	 *
	 *	@param	header_name		Name of header
	 *	@param	p_name			name of parameter
	 *	@param	p_value			value of parameter
	 *	@param	charset			charset to use in QP encoding
	 */
	void AddRFC2231ParameterL(const OpStringC8 &header_name, const OpStringC8 &p_name, const OpStringC8 &p_value, const OpStringC8 &charset);

	/** Add a RFC 2231 qouted printable parameter to the named header 
	 *
	 *	@param	header_name		Name of header
	 *	@param	p_name			name of parameter
	 *	@param	p_value			value of parameter
	 *	@param	charset			charset to use in QP encoding
	 */
	void AddRFC2231ParameterL(const OpStringC8 &header_name, const OpStringC8 &p_name, const OpStringC16 &p_value, const OpStringC8 &charset);

	/** Clear the current parameter values for named headers and add an opaque value (free format) */
	void ClearAndAddParameterL(const OpStringC8 &header_name, const OpStringC8 &p_value);

	/** Clear the current parameter values for named headers and add a name and value with optional quoting */
	void ClearAndAddParameterL(const OpStringC8 &header_name, const OpStringC8 &p_name, const OpStringC8 &p_value, BOOL quote_value = FALSE);

	/** Clear the current parameter values for named headers and add the argument parameter, which is taken over by the header */
	void ClearAndAddParameterL(const OpStringC8 &header_name, Header_Parameter_Base *param);
	
	/** Find the headers in the buffer, and add it to the present headers, removing those that are dangerous 
	 *	Returns the number of characters consumed
	 *
	 *	The optional list MUST be on the form specified by the KeywordIndex structure (first item is the default)
	 *	value TRUE in the list means the header is excluded, FALSE that it is included
	 */
	uint32 ExtractHeadersL(const unsigned char *src, uint32 len, BOOL use_all = FALSE, Header_Protection_Policy policy=HEADER_REMOVE_HTTP, const KeywordIndex* excluded_headers=NULL, int excluded_list_len=0);

	/** Returns the enabled status for the *LAST* header with the given name */
	BOOL HeaderIsEnabled(const OpStringC8 &header);

	/** Sets the enabled status to the given flag value  for *ALL* headers with the given name, and adds the header if necessary */
	void HeaderSetEnabledL(const OpStringC8 &header, BOOL flag);

	/** Remove all attributes on the given header */
	void ClearHeader(const OpStringC8 &header);

	/** Remove (all instances of) the given header */
	void RemoveHeader(const OpStringC8 &header);

	/** Clear the values of the name header and change the separator (if necessary it will be created)
	 *
	 *	@param	header_name		Name of header
	 *	@param	sep				Parameter Separator to use
	 */
	void ClearAndSetSeparatorL(const OpStringC8 &header_name, Header_Parameter_Separator sep);

	/** Returns length of header list (excluding null terminator) */
	virtual uint32 CalculateLength();

	/** Writes the headers to the string, which is **ASSUMED** to be long enough. 
	 *	Returns pointer to the next position. Null terminates string.
	 */
	virtual char *OutputHeaders(char *target);

	Header_Item *First(){return (Header_Item *) Head::First();};
	Header_Item *Last(){return (Header_Item *) Head::Last();};

	/** Find the named header, optionally the last with that name */
	Header_Item *FindHeader(const OpStringC8 &header_ref, BOOL last);

};


/** The basic functionality for upload
 *	it maintains the list of headers, and the overall status of the generation
 *	Implementations handle all bodypart data.
 *
 *	The headers are prepended the implementation specific payload
 */
class Upload_Base : public Link
{
public:
	/** The headers */
	Header_List  Headers;

protected:
	/** Have the headers been written? */
	BOOL headers_written;
	/** Only write body? When TRUE no header is output, only the body is generated */
	BOOL only_body;

	/** The content type of this body */
	OpStringS8	content_type;

	/** The charset of this body */
	OpStringS8	charset;

public:

	/** Constructor */
	Upload_Base();

	/** Destructor */
	virtual ~Upload_Base();

	/** Initialize with the specified NULL pointer terminated list of headers */
	void InitL(const char **header_names=NULL);

	/** Access the Header list directly */
	Header_List &AccessHeaders(){ return Headers;}

	/** Returns length of element string (excluding null terminator) */
	virtual OpFileLength CalculateLength();

	/** Writes the element to the string, will at most use remaining_len bytes of target
	 *
	 *	@param		target		pointer to binary buffer at least remaining_len bytes long
	 *	@param		remaining_len	input/output of the number of bytes available in the target buffer before/after output
	 *	@param		done		updated to TRUE if all content has been output, FALSE otherwise
	 *
	 *	@return		char *		pointer to next character after what was written
	 */
	virtual unsigned char *OutputContent(unsigned char *target, uint32 &remaining_len, BOOL &done);

	/** Writes the element to the string, will at most use buffer_len bytes of target
	 *
	 *	@param		target		pointer to binary buffer at least remaining_len bytes long
	 *	@param		buffer_len	the number of bytes available in the target buffer
	 *	@param		done		updated to TRUE if all content has been output, FALSE otherwise
	 *
	 *	@return		uint32		returns the number of bytes written to the buffer
	 */
	virtual uint32 GetOutputData(unsigned char *target, uint32 buffer_len, BOOL &done);

	/** Returns length of headers*/
	virtual uint32 CalculateHeaderLength();

	/** Output the headers to the target buffer
	 *
	 *	@param		target		pointer to binary buffer at least remaining_len bytes long
	 *	@param		remaining_len	input/output of the number of bytes available in the target buffer before/after output
	 *	@param		done		updated to TRUE if all content has been output, FALSE otherwise
	 *
	 *	@return		char *		pointer to next character after what was written
	 */
	virtual unsigned char *OutputHeaders(unsigned char *target, uint32 &remaining_len, BOOL &done);

	/**
	 *	Prepares content, returns minimum bufferlength required to store the headers 
	 *
	 *	LEAVES with error code Upload_Error_Status::ERR_FOUND_BOUNDARY_PREFIX if the boundary_prefix is found in the content
	 */
	virtual uint32 PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions);

	/**
	 *	Public interface
	 *	Prepares content, returns minimum bufferlength required to store the headers 
	 *
	 */
	virtual uint32 PrepareUploadL(Upload_Transfer_Mode transfer_restrictions);

	/** Reset the element so that the next OutputContent/OutputHeaders call starts at the beginning of the element*/
	virtual void ResetL();

	/** Non-existent function, will cause link error! Used to catche calls intended for ResetL(), not Link::Reset()*/
	void Reset();

	/** Clean up after we're finished */
	virtual void Release();

	Upload_Base *Suc(){return (Upload_Base *) Link::Suc();};
	Upload_Base *Pred(){return (Upload_Base *) Link::Pred();};

	/** If this upload contains other uploads (multipart), the first child is returned. NULL if no children. */
	virtual Upload_Base *FirstChild();

	/** Set the content type for this object, even if previously set. */
	virtual void ClearAndSetContentTypeL(const OpStringC8 &a_content_type);

	/** Set the content type for this object. Implementation may perform special actions before calling this function */
	virtual void SetContentTypeL(const OpStringC8 &a_content_type);

	/** Get the content type */
	OpStringC8 &GetContentType(){return content_type;}

	/** Set the charset for this object. Implementation may perform special actions before calling this function */
	virtual void SetCharsetL(const OpStringC8 &encoded_charset);

	/** Get the charset */
	OpStringC8 &GetCharset(){return charset;}

	/** Set the content disposition type for this object. Implementation may perform special actions before calling this function */
	virtual void SetContentDispositionL(const OpStringC8 &disposition);
	/** Add a content disposition parameter for this object. Implementation may perform special actions before calling this function */
	virtual void SetContentDispositionParameterL(const OpStringC8 &value);
	/** Add a content disposition parameter with name, value and option qouting for this object. Implementation may perform special actions before calling this function */
	virtual void SetContentDispositionParameterL(const OpStringC8 &name, const OpStringC8 &value, BOOL quote_value= FALSE);

	/** Find the headers in the buffer, and add it to the present headers, removing those that are dangerous 
	 *	Returns the number of characters consumed
	 *
	 *	The optional list MUST be on the form specified by the KeywordIndex structure (first item is the default)
	 *	value TRUE in the list means the header is excluded, FALSE that it is included
	 */
	virtual uint32 ExtractHeadersL(const unsigned char *src, uint32 len, BOOL use_all=FALSE, Header_Protection_Policy policy=HEADER_REMOVE_HTTP,  const KeywordIndex *excluded_headers=NULL, int excluded_list_len=0);

	/** How many files are there in this upload ? */
	virtual uint32 FileCount();      
	/** How many files are have we uploaded so far ? */
	virtual uint32 UploadedFileCount();

#ifdef UPLOAD_NEED_MULTIPART_FLAG
	/** Is this a multipart body? */
	virtual BOOL IsMultiPart();
#endif

	/** Retrieve the next payload block of data from the implementation 
	 *
	 *	@param	buf		buffer to insert the data in
	 *	@param	max_len maximul length of buffer
	 *	@param	more	If TRUE on return there are more data pending, if not, this is the last batch
	 *
	 *	@return	uint32	number of butes written to the buffer
	 */
	virtual uint32 GetNextContentBlock(unsigned char *buf, uint32 max_len, BOOL &more)=0;

	/** Length of payload */
	virtual OpFileLength PayloadLength()=0;

	/** Reset the content for another read using GetNextContentBlock */
	virtual void ResetContent()=0;

	/** Set the flag indicating that only the body of this element should be sent (or not sent) */
	void	SetOnlyOutputBody(BOOL flag){only_body = flag;};
	/** Get the flag indicating that only the body of this element should be sent (or not sent) */
	BOOL	GetOnlyOutputBody(){return only_body;};
};

/** General handler of binary payloads, if necessary encoding them as Base64 qouted printable */
class Upload_Handler : public Upload_Base
{
private:
	/** The internal proocessing buffer */
	unsigned char *internal_buffer;

	/** Length of buffer */
	uint32 buffer_len;

	/** The minimum length of buffer needed */
	uint32 buffer_min_len;

	/** Position in buffer */
	uint32 buffer_pos;

	/** Size of buffer */
	uint32 buffer_size;

	/** Is the current block being processed the last block? */
	BOOL last_block;

	/** What encoding is used ? */
	MIME_Encoding encoding;

	/** How long do we think this payload will be? */
	OpFileLength calculated_payload_size;

	/** what is the length of the current line being encoded? */
	uint32 current_line_len;

public:

	/** Constructor */
	Upload_Handler();

	/** Destructor */
	virtual ~Upload_Handler();

	/** Intialize with the specified encoding and headers (NULL pointer terminated list) */
	void InitL(MIME_Encoding enc=ENCODING_AUTODETECT, const char **header_names=NULL);

	/** Returns length of parameter string (excluding null terminator) */
	virtual OpFileLength CalculateLength();

	/** Writes the parameters to the string, which is assumed to be long enough. 
	 *	Returns pointer to the next position. Null terminates string.
	 */
	virtual unsigned char *OutputContent(unsigned char *target, uint32 &remaining_len, BOOL &done);
	
	/** Prepares content, returns minimum bufferlength required to store the headers */
	virtual uint32 PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions);

	/** Reset for generation */
	virtual void ResetL();

	/** Release all externally held data */
	virtual void Release();

protected:

	/** Reset for loading */
	virtual void ResetContent();

	/** Set the minimum buffer size required to read this upload (headers may be long) */
	void SetMinimumBufferSize(uint32 size);
	/** Get the minimum buffer size required to read this upload (headers may be long) */
	uint32 GetMinimumBufferSize()const {return buffer_min_len;};

	/** Do we need to scan this body concerning encoding and/or multipart boundaries */
	virtual BOOL BodyNeedScan() {return TRUE;}
	/** Do we need to decode this body to be able to accurately determine the length */
	virtual BOOL DecodeToCalculateLength() {return FALSE;}

	/** Which encoding is used? */
	MIME_Encoding GetEncoding() {return encoding;}
	
	/** Force which encoding should be used when encoding */
	void SetEncoding(MIME_Encoding aEncoding){encoding=aEncoding;};

#ifdef UPLOAD_PAYLOAD_ACTION_SUPPORT
	/** Output the payload and perform the subclass-specific action in PerformPayloadActionL */
	void ProcessPayloadActionL();

	/* perform any special actions that have to be done on the payload (e.g. signature digest) */
	virtual void PerformPayloadActionL(unsigned char *src, uint32 src_len);
#endif

private:

	void FreeBuffer();

	void CheckInternalBufferL(OpFileLength max_needed);

	unsigned char *Encode7Or8Bit(unsigned char *target, uint32 &remaining_len, BOOL more);
	unsigned char *EncodeQP(unsigned char *target, uint32 &remaining_len, BOOL more);
	unsigned char *EncodeBase64(unsigned char *target, uint32 &remaining_len, BOOL more);

};


/** Manage a binary payload buffer, e.g for strings */
class Upload_BinaryBuffer : public Upload_Handler
{
private:
	/** The buffer of the binary payload */
	unsigned char *binary_buffer;

	/** Length of buffer */
	uint32 binary_len;

	/** Current read position */
	uint32 binary_pos;

	/** TRUE if this object own the buffer */
	BOOL own_buffer;

public:

	/** Constructor */
	Upload_BinaryBuffer();

	/** Destructor */
	virtual ~Upload_BinaryBuffer();

	/** Initialize with the specified buffer, content type, charset, encoding and headers
	 *
	 *	@param	buf			Input buffer 
	 *	@param	buf_len		Length of input buffer
	 *	@param	action		Does this object take over the object, copy it, or just reference it?
	 *	@param	content_type		Content type for this object
	 *	@param	encoded_charset		Charset used to encode the data (if relevant)
	 *	@param	enc					Transport encoding specification (default is that the preparation decides)
	 *	@param	header_names		Optional NULL-pointer terminated list of headers names to be added
	 */
	void InitL(unsigned char *buf, uint32 buf_len, Upload_Buffer_Ownership action, const OpStringC8 &content_type = OpStringC8(NULL), const OpStringC8 &encoded_charset=OpStringC8(NULL), MIME_Encoding enc=ENCODING_AUTODETECT, const char **header_names=NULL);

	virtual void ResetL();

	/** Clean up after we're finished */
	virtual void Release();

protected:

	virtual uint32 GetNextContentBlock(unsigned char *buf, uint32 max_len, BOOL &more);

	virtual void ResetContent();

	virtual OpFileLength PayloadLength();

};

/** Upload an 8-bit string buffer */
class Upload_OpString8 : public Upload_BinaryBuffer
{
public:

	Upload_OpString8();
	virtual ~Upload_OpString8();

	/** Initialize with the specified buffer, content type, charset, encoding and headers
	 *
	 *	@param	value		String to be uploaded
	 *	@param	content_type		Content type for this object
	 *	@param	encoded_charset		Charset used to encode the string (if relevant)
	 *	@param	enc					Transport encoding specification (default is that the preparation decides)
	 *	@param	header_names		Optional NULL-pointer terminated list of headers names to be added
	 */
	virtual void InitL(const OpStringC8 &value, const OpStringC8 &content_type=OpStringC8(NULL), const OpStringC8 &encoded_charset=OpStringC8(NULL), MIME_Encoding enc=ENCODING_AUTODETECT, const char **header_names=NULL);
};

#ifdef UPLOAD2_OPSTRING16_SUPPORT
/** Upload an unicode string buffer. The string will be converted to the specified charset*/
class Upload_OpString16 : public Upload_BinaryBuffer
{
public:

	Upload_OpString16();
	virtual ~Upload_OpString16();

	/** Initialize with the specified buffer, content type, charset, encoding and headers
	 *
	 *	@param	value		String to be uploaded
	 *	@param	content_type		Content type for this object
	 *	@param	convert_to_charset	Charset that will be used to encode the string (if relevant)
	 *	@param	enc					Transport encoding specification (default is that the preparation decides)
	 *	@param	header_names		Optional NULL-pointer terminated list of headers names to be added
	 */
	virtual void InitL(const OpStringC16 &value, const OpStringC8 &content_type = OpStringC8(NULL), const OpStringC8 &convert_to_charset=OpStringC8(NULL), MIME_Encoding enc=ENCODING_AUTODETECT, const char **header_names=NULL);
};
#endif

#if defined UPLOAD2_EXTERNAL_BUFFER_SUPPORT || defined UPLOAD2_EXTERNAL_MULTI_BUFFER_SUPPORT
/** Descriptor used to manage external buffers 
 *	Implementations own object of this type.
 *	Release indicates that the memory reserved by the description will not 
 *	be used further, AND THAT this object can be deleted 
 */
class Upload_ExternalBuffer_ReleaseDescriptor
{
public:
	virtual ~Upload_ExternalBuffer_ReleaseDescriptor(){};
	/** Release the allocated memory. Also indicates that this object will not be accessed again and that this object can be *deleted*  */
	virtual void Release() = 0;
};
#endif

#ifdef UPLOAD2_EXTERNAL_BUFFER_SUPPORT
/** Used to upload a payload that is owned by an external client, the end of upload is 
 *	indicated to the client via a Upload_ExternalBuffer_ReleaseDescriptor object, also owned by the client */
class Upload_ExternalBuffer : public Upload_BinaryBuffer
{
private:
	/** Implementation specific descriptor that is used to reserve the memory used by this object */
	Upload_ExternalBuffer_ReleaseDescriptor *release_desc; // NULLED after use, owner releases

public:

	Upload_ExternalBuffer();
	virtual ~Upload_ExternalBuffer();


	/** Initialize with the specified externally maintained buffer, content type, charset, encoding and headers
	 *
	 *	@param	buf			Input buffer 
	 *	@param	buf_len		Length of input buffer
	 *	@param	release		Descriptor used to reserver the memory indicated by buf. When this object is cleaned up by Release()
	 *						the function release->Release() will be called. This call indicates that 1) the memory reserved by this object 
	 *						will no longer be accessed, and 2) that the release descriptor will no longer be used (and that it is considered deleted after return
	 *	@param	content_type		Content type for this object
	 *	@param	encoded_charset		Charset used to encode the data (if relevant)
	 *	@param	enc					Transport encoding specification (default is that the preparation decides)
	 *	@param	header_names		Optional NULL-pointer terminated list of headers names to be added
	 */
	void InitL(unsigned char *buf, uint32 buf_len,  Upload_ExternalBuffer_ReleaseDescriptor *release, const OpStringC8 &content_type = OpStringC8(NULL),const OpStringC8 &encoded_charset=OpStringC8(NULL) ,MIME_Encoding enc=ENCODING_AUTODETECT, const char **header_names=NULL);

	/** Clean up after we're finished */
	virtual void Release();

};
#endif

#ifdef  UPLOAD2_EXTERNAL_MULTI_BUFFER_SUPPORT
# include "adjunct/voice/xmlplugins/iovector.h"

/** Manages a list of externally owned buffers that contain the payload
 *	Used to upload a payload that is owned by an external client, the end of upload is 
 *	indicated to the client via a Upload_ExternalBuffer_ReleaseDescriptor object, also owned by the client
 */
class Upload_ExternalMultiBuffer : public Upload_Handler
{
private:
	/** Implementation specific descriptor that is used to reserve the memory used by this object */
	Upload_ExternalBuffer_ReleaseDescriptor *release_desc; // NULLED after use, owner releases

	/** Externally owned list of buffers */
	IOVector_t *buffer_list;
	/** Number of items in the list */
	uint32 buffer_list_parts;

	/** Currently read buffer item */
	uint32 buffer_item;
	/** Position in currently read buffer item */
	uint32 buffer_pos;

public:

	Upload_ExternalMultiBuffer();
	virtual ~Upload_ExternalMultiBuffer();

	/** Initialize with the specified externally maintained buffer, content type, charset, encoding and headers
	 *
	 *	@param	buf_list	List of input buffers, owned by external client
	 *	@param	part		Number of buffers
	 *	@param	release		Descriptor used to reserver the memory indicated by the buflist. When this object is cleaned up by Release()
	 *						the function release->Release() will be called. This call indicates that 1) the memory reserved by this object 
	 *						will no longer be accessed, and 2) that the release descriptor will no longer be used (and that it is considered deleted after return
	 *	@param	content_type		Content type for this object
	 *	@param	encoded_charset		Charset used to encode the data (if relevant)
	 *	@param	enc					Transport encoding specification (default is that the preparation decides)
	 *	@param	header_names		Optional NULL-pointer terminated list of headers names to be added
	 */
	void InitL(IOVector_t *buf_list, uint32 parts,  Upload_ExternalBuffer_ReleaseDescriptor *release, OpStringC8 content_type = OpStringC8(NULL),OpStringC8 encoded_charset=OpStringC8(NULL) ,MIME_Encoding enc=ENCODING_AUTODETECT, const char **header_names=NULL);

	virtual void ResetL();

	/** Clean up after we're finished */
	virtual void Release();

protected:

	virtual uint32 GetNextContentBlock(unsigned char *buf, uint32 max_len, BOOL &more);

	virtual void ResetContent();

	virtual OpFileLength PayloadLength();


};
#endif

/** Upload a the contents of a given URL */
class Upload_URL : public Upload_Handler
{
private:
	/** The URL, with use blocking */
	URL_InUse src_url;
	/** Datadescriptor used to read the data */
	URL_DataDescriptor *desc;
	BOOL m_finished;


	/** Encoding charset */
	OpStringS8 encoding_charset;
	/** suggested filename, overrides the URLs suggestions */
	OpStringS	suggested_filename;

	/** Whether we should force a filename to be set or not in the headers, TRUE by default (FALSE is mainly for mail messages) */
	BOOL		force_filename;

	/** Whether we should force the name parameter of content-type to be quoted-printable */
	BOOL		force_qp_content_type_name;

	/** The starting offset of a ranged upload. */
	OpFileLength upload_start_offset;

	/** If > 0, the length (in bytes) of a ranged upload. 0 means to the end. */
	OpFileLength upload_length;

public:

	Upload_URL();
	virtual ~Upload_URL();

	/** Set up to load the argument URL
	 *
	 *	@param	url					URL object referencing the data 
	 *	@param	suggested_name		Suggested filename for the object
	 *	@param	encoded_charset		Charset used to encode the values in the headers (if necessary), such as filenames
	 *	@param	enc					Transport encoding specification (default is that the preparation decides)
	 *	@param	header_names		Optional NULL-pointer terminated list of headers names to be added
	 */
	void InitL(URL &url, const OpStringC &suggested_name, const OpStringC8 &encoded_charset=OpStringC8(NULL), MIME_Encoding enc=ENCODING_AUTODETECT, const char **header_names=NULL);

	/** Set up to load the argument URL
	 *
	 *	@param	url					URL object referencing the data 
	 *	@param	suggested_name		Suggested filename for the object
	 *	@param	disposition			Disposition type for Content-Dispostion header
	 *	@param	disposition_name	Set Content-Dispostion  Name argument
	 *	@param	encoded_charset		Charset used to encode the values in the headers (if necessary), such as filenames
	 *	@param	enc					Transport encoding specification (default is that the preparation decides)
	 *	@param	header_names		Optional NULL-pointer terminated list of headers names to be added
	 */
	void InitL(URL &url, const OpStringC &suggested_name, const OpStringC8 &disposition, const OpStringC8 &disposition_name, const OpStringC8 &encoded_charset=OpStringC8(NULL), MIME_Encoding enc=ENCODING_AUTODETECT, const char **header_names=NULL);

	/** Set up to upload the data identified by the URL name string
	 *
	 *	@param	url_name			Name of URL referencing the data, need not be correctly formatted as it is resolved before use 
	 *	@param	suggested_name		Suggested filename for the object
	 *	@param	encoded_charset		Charset used to encode the values in the headers (if necessary), such as filenames
	 *	@param	enc					Transport encoding specification (default is that the preparation decides)
	 *	@param	header_names		Optional NULL-pointer terminated list of headers names to be added
	 */
	void InitL(const OpStringC &url_name, const OpStringC &suggested_name, const OpStringC8 &encoded_charset=OpStringC8(NULL), MIME_Encoding enc=ENCODING_AUTODETECT, const char **header_names=NULL);

	/** Set up to upload the data identified by the URL name string
	 *
	 *	@param	url_name			Name of URL referencing the data, need not be correctly formatted as it is resolved before use 
	 *	@param	suggested_name		Suggested filename for the object
	 *	@param	disposition			Disposition type for Content-Dispostion header
	 *	@param	disposition_name	Set Content-Dispostion  Name argument
	 *	@param	encoded_charset		Charset used to encode the values in the headers (if necessary), such as filenames
	 *	@param	enc					Transport encoding specification (default is that the preparation decides)
	 *	@param	header_names		Optional NULL-pointer terminated list of headers names to be added
	 */
	void InitL(const OpStringC &url_name,  const OpStringC &suggested_name, const OpStringC8 &disposition, const OpStringC8 &disposition_name, const OpStringC8 &encoded_charset=OpStringC8(NULL), MIME_Encoding enc=ENCODING_AUTODETECT, const char **header_names=NULL);

	/** Set up an upload of only a portion of the URL. Must be called prior to
	 *  calling PrepareL(), but after InitL().
	 *
	 *  @param offset The starting byte offset of the upload.
	 *  @param length The number of bytes to upload. 0 if to the end.
	 */
	void SetUploadRange(OpFileLength offset, OpFileLength length);

	virtual uint32 PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions);

	virtual void ResetL();
	
	/** Clean up after we're finished */
	virtual void Release();

	virtual uint32 FileCount();      
	virtual uint32 UploadedFileCount();

	/** Set whether this Upload_URL should force setting a filename in the headers or not */
	void SetForceFileName(BOOL force_filename) { this->force_filename = force_filename; }

	/** Set whether we should force the name parameter of content-type to be quoted-printable */
	void SetForceQPContentTypeName(BOOL force_qp_content_type_name) { this->force_qp_content_type_name = force_qp_content_type_name; }

	URLContentType GetSrcContentType();
	

protected:

	virtual uint32 GetNextContentBlock(unsigned char *buf, uint32 max_len, BOOL &more);

	virtual void ResetContent();

	virtual OpFileLength PayloadLength();

	virtual BOOL DecodeToCalculateLength();

private:

	BOOL CheckDescriptor();
};

/** Copies the contents of the argument URL into a temporary URL, the data may contain a header*/
class Upload_TemporaryURL : public Upload_URL
{
public:
	/** Set up to upload a copy of the data identified by the URL
	 *
	 *	@param	url_name			Name of URL referencing the data, need not be correctly formatted as it is resolved before use 
	 *	@param	suggested_name		Suggested filename for the object
	 *	@param	contain_header		Does the data contain a header? 
	 *	@param	encoded_charset		Charset used to encode the values in the headers (if relevant), such as filenames
	 *	@param	enc					Transport encoding specification (default is that the preparation decides)
	 *	@param	header_names		Optional NULL-pointer terminated list of headers names to be added
	 */
	void InitL(URL &url, const OpStringC &suggested_name, BOOL contain_header, const OpStringC8 &encoded_charset=OpStringC8(NULL), MIME_Encoding enc=ENCODING_AUTODETECT, const char **header_names=NULL);

	/** Set up to upload a copy of the data identified by the URL name string
	 *
	 *	@param	url_name			Name of URL referencing the data, need not be correctly formatted as it is resolved before use 
	 *	@param	suggested_name		Suggested filename for the object
	 *	@param	contain_header		Does the data contain a header? 
	 *	@param	encoded_charset		Charset used to encode the values in the headers (if relevant), such as filenames
	 *	@param	enc					Transport encoding specification (default is that the preparation decides)
	 *	@param	header_names		Optional NULL-pointer terminated list of headers names to be added
	 */
	void InitL(const OpStringC &url_name, const OpStringC &suggested_name, BOOL contain_header, const OpStringC8 &encoded_charset=OpStringC8(NULL), MIME_Encoding enc=ENCODING_AUTODETECT, const char **header_names=NULL);
};

/** Manage a sequence of upload handlers, separating each by a MIME boundary */
class Upload_Multipart : public Upload_Handler, public AutoDeleteHead
{
private:
	/** The boundary name of this object */
	Boundary_Item boundary;

	/** The payload item currently being processed */
	Upload_Base *current_item;

	/** Have we started uploading? */
	BOOL started;

public:

	Upload_Multipart();
	virtual ~Upload_Multipart();

	/** Initialize with specified optional content type and optional headernames */
	virtual void InitL(const OpStringC8 &content_type = OpStringC8(NULL), const char **header_names=NULL);

	/** Add a new element to the list */
	virtual void AddElement(Upload_Base *item);

	/** Add a sequence of new elements to the list */
	void AddElements(Upload_Multipart &item);

	virtual OpFileLength CalculateLength();

	/** Prepares content, returns minimum bufferlength required to store the headers */
	virtual uint32 PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions);

	virtual void ResetL();
	
	/** Clean up after we're finished */
	virtual void Release();

	/** Returns First() */
	virtual Upload_Base *FirstChild();

	virtual uint32 FileCount();      
	virtual uint32 UploadedFileCount();

#ifdef UPLOAD_NEED_MULTIPART_FLAG
	virtual BOOL IsMultiPart();
#endif

	Upload_Base *First(){return (Upload_Base *) Head::First();};
	Upload_Base *Last(){return (Upload_Base *) Head::Last();};

protected:

	virtual uint32 GetNextContentBlock(unsigned char *buf, uint32 max_len, BOOL &more);

	virtual void ResetContent();

	virtual OpFileLength PayloadLength();

	virtual BOOL BodyNeedScan();
};

#ifdef UPLOAD2_ENCAPSULATED_SUPPORT
/** encapsulates another upload element in a single object, without adding the multipart extensions 
 *	The bodypart element may encode its header separately, or as a part of this objects header
 */
class Upload_EncapsulatedElement : public Upload_Handler
{
private:
	/** The payload element */
	Upload_Base *element;
	/** Is the element and this object part of the same MIME entity? */
	BOOL separate_body;
	/** Is this object owned by another object? If so do not delete; */
	BOOL external_object;
	/** is the elment already prepared? If so, do not prepare it again */
	BOOL element_prepared;
	/** if not separate body, is headers written? */
	BOOL extra_header_written; 
public:

	Upload_EncapsulatedElement();
	virtual ~Upload_EncapsulatedElement();

	/** Set up to upload the argument element
	 *	Assumes that it is can take over the element, use separate body, element is not prepared and we can force the content type
	 *
	 *	@param	element				The element to be encapsulated, this object takes ownership and deletes it.
	 *	@param	content_type		Content type for this object
	 *	@param	enc					Transport encoding specification (default is that the preparation decides)
	 *	@param	header_names		Optional NULL-pointer terminated list of headers names to be added
	 */
	void InitL(Upload_Base *elm, OpStringC8 content_type = OpStringC8(NULL), MIME_Encoding enc=ENCODING_BASE64, const char **header_names=NULL);

	/** Set up to upload the argument element
	 *
	 *	@param	element				The element to be encapsulated, this object takes ownership and deletes it.
	 *	@param	take_over			If TRUE, take ownership of object, delete after use
	 *	@param	sepbody				Use a separate body (do not combine headers)
	 *	@param	prep_body			If TRUE, element is already prepared
	 *	@param	force_content_type	If TRUE, and no content-type is given, the content-type is set to "application/mime"
	 *	@param	content_type		Content type for this object
	 *	@param	enc					Transport encoding specification (default is that the preparation decides)
	 *	@param	header_names		Optional NULL-pointer terminated list of headers names to be added
	 */
	void InitL(Upload_Base *elm, BOOL take_over, BOOL sepbody, BOOL prepbody, BOOL force_content_type, OpStringC8 content_type = OpStringC8(NULL), MIME_Encoding enc=ENCODING_BASE64, const char **header_names=NULL);

	/** Set up to upload the argument element
	 *
	 *	@param	element				The element to be encapsulated, this object takes ownership and deletes it.
	 *	@param	take_over			If TRUE, take ownership of object, delete after use
	 *	@param	sepbody				Use a separate body (do not combine headers)
	 *	@param	prep_body			If TRUE, element is already prepared
	 */
	void SetElement(Upload_Base *elm, BOOL take_over, BOOL sepbody, BOOL prepbody);

	/** Gets element to upload, as previously set by SetElement, or InitL.
	 *
	 *  @return	The element to upload, or NULL if no element is set.
	 */
	Upload_Base *GetElement() const;

	virtual OpFileLength CalculateLength();

	/** Prepares content, returns minimum bufferlength required to store the headers */
	virtual uint32 PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions);

	virtual void ResetL();
	
	/** Clean up after we're finished */
	virtual void Release();

	BOOL HasPayload(){return (element != NULL ? TRUE : FALSE);}

	virtual uint32 FileCount() { return (element != NULL ? element->FileCount() : Upload_Handler::FileCount()); }
	virtual uint32 UploadedFileCount() { return (element != NULL ? element->UploadedFileCount() : Upload_Handler::UploadedFileCount()); }

protected:

	virtual uint32 GetNextContentBlock(unsigned char *buf, uint32 max_len, BOOL &more);

	virtual void ResetContent();

	virtual OpFileLength PayloadLength();

public:

	virtual uint32 CalculateHeaderLength();

	virtual unsigned char *OutputHeaders(unsigned char *target, uint32 &remaining_len, BOOL &done);
};

#ifdef UPLOAD_EXTERNAL_BODY_SUPPORT
/** Upload the name of the URL as a "message/external-body" body */
class Upload_External_Body : public Upload_EncapsulatedElement
{
public:

	/** Upload the name of the URL, with the specified content.ID */
	void InitL(URL &url, const OpStringC8 &content_id, const char **header_names=NULL);
};
#endif

#endif

#ifdef _URL_EXTERNAL_LOAD_ENABLED_
class Upload_AsyncBuffer : public Upload_Base
{
public:
	Upload_AsyncBuffer();
	virtual ~Upload_AsyncBuffer();

	void InitL(uint32 buf_len, const char **header_names=NULL);

	OP_STATUS AppendData(const void *buf, uint32 bufsize);

	OpFileLength CalculateLength();

	unsigned char *OutputContent(unsigned char *target, uint32 &remaining_len, BOOL &done);

	virtual uint32 GetNextContentBlock(unsigned char *buf, uint32 max_len, BOOL &more);

	virtual OpFileLength PayloadLength();

private:
	void *databuf;
	int total_datasize;
	int head; ///< Amount of data received by data provider
	int tail; ///< Amount of data forwarded to the Comm object
};
#endif // _URL_EXTERNAL_LOAD_ENABLED_

#endif // __UPLOAD2_H__
