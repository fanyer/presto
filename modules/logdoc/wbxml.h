/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifdef WBXML_SUPPORT

#ifndef WBXML_H
#define WBXML_H

class CharConverter;
class HLDocProfile;

typedef OP_STATUS OP_WXP_STATUS;

/// Dummy class for constants used as return values from the WBXML_Parser
class WXParseStatus : public OpStatus
{
public:
    enum
    {
        OK          = 0, ///< guess: everything OK, no special actions required
		BUFFER_FULL	= USER_SUCCESS + 1, ///< the output buffer is full
        NEED_GROW		=	USER_SUCCESS + 2,	///< no end tag found, need a larger buffer
		WRONG_VERSION	=	USER_ERROR + 1,	///< wrong version given
		UNEXPECTED_END	=	USER_ERROR + 2,	///< end of buffer reached unexpected
		NOT_WELL_FORMED	=	USER_ERROR + 3	///< the code is not well formed
    };
};

/// Elements for the tagstack in the WBXML_Parser
class WBXML_StackElm
{
private:
	WBXML_StackElm*	m_next; ///< next entry in the stack

public:
				WBXML_StackElm() : m_next(NULL) {}
	virtual		~WBXML_StackElm() {};

	void		Precede(WBXML_StackElm *elm) { m_next = elm; }

	WBXML_StackElm*	GetNext() { return m_next; }
};

class WBXML_TagElm : public WBXML_StackElm
{
private:
	uni_char*	m_tag; ///< tag string
	BOOL		m_quote; ///< does the tag need WML style '$' quoting

public:
				/// Constructor. \param[in] need_quoting Tells if '$'
				/// should be quoted with '$$' WML style
				WBXML_TagElm(BOOL need_quoting)
					: WBXML_StackElm(), 
					m_tag(NULL),
					m_quote(need_quoting) {};
	virtual		~WBXML_TagElm() { OP_DELETEA(m_tag); }

	void		SetTagNameL(const uni_char *tag);
	const uni_char*	GetTagName() { return m_tag; }
	BOOL		GetQuoting() { return m_quote; }
};

class WBXML_AttrElm : public WBXML_StackElm
{
private:
	UINT32		m_tok; ///< The token for the current element we are parsing attributes for
	BOOL		m_start_found; ///< Is the start of a attribute value found
	BOOL		m_end_found; ///< Is the end of a attribute list found

public:
	WBXML_AttrElm() : m_tok(0), m_start_found(FALSE), m_end_found(FALSE) {}

	void		SetToken(UINT32 token) { m_tok = token; }
	UINT32		GetToken() { return m_tok; }

	void		SetStartFound(BOOL found) { m_start_found = found; }
	BOOL		GetStartFound() { return m_start_found; }

	void		SetEndFound(BOOL found) { m_end_found = found; }
	BOOL		GetEndFound() { return m_end_found; }
};

class WBXML_Parser;

/// Content handler interface used for content that is specific for the
/// different document types. This interface must be implemented for all
/// the document types Opera should handle specifically (like WML).
/// Returns a element tag or attribute for the WBXML tokens in its namespace.
class WBXML_ContentHandler
{
private:
	WBXML_Parser *m_parser; ///< pointer back to the parser that uses it

public:
			WBXML_ContentHandler(WBXML_Parser *parser) : m_parser(parser) {}
	virtual ~WBXML_ContentHandler() {}

	/// Secondary constructor. Use if you need to allocate data during construction.
	virtual OP_WXP_STATUS	Init() { return OpStatus::OK; };

	/// Returns a tag string corresponding to the given WBXML element token
	virtual const uni_char* TagHandler(UINT32 tok) = 0;
	/// Returns an attribute string corresponding to the given WBXML attribute token
	virtual const uni_char* AttrStartHandler(UINT32 tok) = 0;
	/// Returns an attribute value string corresponding to the given WBXML attribute value token
	virtual const uni_char* AttrValueHandler(UINT32 tok) = 0;
	/// Returns a string corresponding to the WBXML EXT_* tokens
	virtual const uni_char* ExtHandlerL(UINT32 tok, const char **buf) { return 0; }
	/// Returns a string corresponding to the WBXML OPAQUE element token
	virtual const uni_char* OpaqueTextNodeHandlerL(const char **buf) { return 0; }
	/// Returns a string corresponding to the WBXML OPAQUE attribute token
	virtual const uni_char* OpaqueAttrHandlerL(const char **buf) { return 0; }

	WBXML_Parser*	GetParser() { return m_parser; }
};

/// The WBXML parser. Used for parsing data coded as Wireless Binary 
/// coded XML (WBXML) into a textual XML document.
class WBXML_Parser
{
public:

	/// Numeric type constants indicating the type of document
	typedef enum
	{
		WX_TYPE_UNKNOWN = 0
#ifdef WML_WBXML_SUPPORT
		, WX_TYPE_WML ///< A binary coded WML document
#endif // WML_WBXML_SUPPORT
#ifdef SI_WBXML_SUPPORT
		, WX_TYPE_SI ///< OMA Service Indication
#endif // SI_WBXML_SUPPORT
#ifdef SL_WBXML_SUPPORT
		, WX_TYPE_SL ///< OMA Service Loading
#endif // SL_WBXML_SUPPORT
#ifdef PROV_WBXML_SUPPORT
		, WX_TYPE_PROV ///< OMA Provisioning
#endif // PROV_WBXML_SUPPORT
#ifdef DRMREL_WBXML_SUPPORT
		, WX_TYPE_DRMREL ///< OMA DRM Rights Expression Language
#endif // DRMREL_WBXML_SUPPORT
#ifdef CO_WBXML_SUPPORT
		, WX_TYPE_CO ///< OMA Cache Operation
#endif // CO_WBXML_SUPPORT
#ifdef EMN_WBXML_SUPPORT
		, WX_TYPE_EMN ///< OMA E-mail notification
#endif // EMN_WBXML_SUPPORT
	} Content_t;
	
private:

	BOOL		m_header_parsed; ///< TRUE if the WBXML header is fully parsed
	BOOL		m_more; ///< TRUE if there is more data waiting in the stream
	UINT32		m_code_page; ///< The current token codepage
	UINT32		m_version; ///< The WBXML version of the current document
	UINT32		m_doc_type; ///< The WBXML doctype used
	Content_t	m_content_type; ///< Internal type of document

	UINT32		m_strtbl_len; ///< Length of the document string table
	char		*m_strtbl; ///< Pointer to the document string table. Consecutive, nul terminated, strings
	const char	*m_in_buf_end; ///< Pointer to the first character after the end of the current buffer
	const char	*m_in_buf_committed; ///< Pointer to the last character committed from the in buffer

	uni_char	*m_tmp_buf; ///< Temporary buffer, used internally
	int			m_tmp_buf_len; ///< Length of the temporary buffer

	uni_char	*m_out_buf; ///< Pointer to the output buffer to append the generated XML code
	unsigned	m_out_buf_idx; ///< The current position into the output buffer
	unsigned	m_out_buf_len; ///< Max length of the ouput buffer
	unsigned	m_out_buf_committed; ///< Position of the last committed out data

	uni_char*	m_overflow_buf; ///< Buffer used when the outbuffer is full
	int			m_overflow_buf_len; ///< Length of the overflow buffer
	int			m_overflow_buf_idx; ///< Current index into the overflow buffer

	InputConverter	*m_char_conv; ///< Character converter to use for decoding strings
	WBXML_StackElm*	m_tag_stack; ///< Top of the tag stack used to keep track of open elements
	WBXML_StackElm*	m_tag_stack_committed; ///< The last committed element, back
	WBXML_AttrElm*	m_attr_parsing_indicator; ///< Pushed on the stack to indicate that we are parsing attributes

	WBXML_ContentHandler	**m_content_handlers; ///< Array of content handlers for the different content types

	UINT32	SetDocType(UINT32 type); ///< Set the doctype and calculate internal content type

	/// Commits the data generated since the last commit. Only committed data will be returned from
	/// the parser. The next round of parsing will start at the last commit point.
	///\param[in] buf_p The point in the input buffer to commit
	void	Commit2Buffer(const char *buf_p) { m_in_buf_committed = buf_p; m_out_buf_committed = m_out_buf_idx; m_tag_stack_committed = m_tag_stack; }

	/// Will convert a string using the document character decoder and put it in the output buffer
	///\param[in] str String to enque, need not be nul terminated
	///\param[in] len Length of str
	///\param[in] commit_pos 
	void	ConvertAndEnqueueStrL(const char *str, int len, const char* commit_pos);
	/// Will look up a string from the string table and convert it using the document character 
	/// decoder and put it in the output buffer.
	///\param[in] index Index into the string table
	void	LookupAndEnqueueL(UINT32 index, const char* commit_pos);

	void	ParseHeaderL(const char **buf); ///< Parse the WBXML header
	void	ParseTreeL(const char **buf); ///< Parse the body of the WBXML document
	void	ParseAttrsL(const char **buf); ///< Parse the attributes of an element

	/// Called for application specific element tokens. \param[in] tok Token
	void	AppSpecialTagL(UINT32 tok, const char* commit_pos);
	/// Called for application specific attribute start tokens. \param[in] tok Token
	void	AppSpecialAttrStartL(UINT32 tok, BOOL already_in_attr, const char* commit_pos);
	/// Called for application specific attribute value tokens. \param[in] tok Token
	void	AppSpecialAttrValueL(UINT32 tok, const char* commit_pos);
	/// Called for application specific extension tokens.
	///\param[in] tok Token
	///\param[out] buf Pointer to buffer where application specific data can be appended.
	void	AppSpecialExtL(UINT32 tok, const char **buf);

	/// Push a new tag on the stack. Called when start tag parsed
	void	PushTagL(const uni_char *tag, BOOL need_quoting);
	/// Pop the last tag from the stack and put the corresponding endtag in the output buffer
	void	PopTagAndEnqueueL(const char* commit_pos);
	/// Push the current state onto the stack to indicate that we are parsing attributes
	///\param[in] tok The current element token
	void	PushAttrState(UINT32 tok);
	/// Pop the current state off the stack when attribute parsing is done
	void	PopAttrState();
	/// Returns TRUE if we are currently parsing attributes
	BOOL	IsInAttrState() { return m_tag_stack == m_attr_parsing_indicator; }

	/// Grows the overflow if it is smaller than wanted_index
	///\param[in] wanted_index The position in the overflow buffer you want to store things to
	void	GrowOverflowBufferIfNeededL(int wanted_index);

public:

			WBXML_Parser();
			~WBXML_Parser();

	/// Secondary constructor. Will allocate memory.
	OP_WXP_STATUS	Init();

	/// Will parse a stream of WBXML data one buffer at a time. The buffer pointer must 
	/// point to the character after the last parsed on subsequent calls.
	///\param[in] out_buf Buffer to put the generated XML in.
	///\param[out] out_buf_len Specifies the length of the output buffer in. After the
	/// method returns it contains the length of the generated data.
	///\param[out] buf Buffer containing raw WBXML data. After the method
	/// returns it will point to the first character after the last character parsed.
	///\param[in] buf_end First character after the last character in the input buffer
	///\param[in] more_buffers TRUE if more data is available from the stream
	OP_WXP_STATUS	Parse(char *out_buf, int &out_buf_len, const char **buf, const char *buf_end, BOOL more_buffers);

	/// Put a string in the output buffer. Will not be returned before a called to Commit2Buffer()
	///\param[in] str String to enqueue, need not be nul terminated
	///\param[in] len Length of str
	///\param[in] need_quoting Will quote non-XML characters as entity references (and '$' in WML)
	///\param[in] may_clear_overflow_buffer Never clear overflow buffer if this is false
	void		EnqueueL(const uni_char *str, int len, const char* commit_pos, BOOL need_quoting = TRUE, BOOL may_clear_overflow_buffer = TRUE);

	/// Will look up a string from the string table and return a pointer to it.
	///\param[in] index Index into the string table
	const char*	LookupStr(UINT32 index);

	/// Reads the next token from the input buffer.
	///\param[out] buf The current buffer pointer, will be incremented
	///\param[in] is_mb_value TRUE if a WBXML multibyte value is to be read
	UINT32		GetNextTokenL(const char **buf, BOOL is_mb_value = FALSE);
	/// Will skip over the next nul terminated string in the input buffer.
	///\param[out] buf The current buffer pointer, will be incremented
	void		PassNextStringL(const char **buf);

	/// Checks if there are enough bytes available in the temporary buffer.
	/// Will leave if there isn't enough space available.
	void		EnsureTmpBufLenL(int len);
	uni_char*	GetTmpBuf() { return m_tmp_buf; }
	int			GetTmpBufLen() { return m_tmp_buf_len; }

	BOOL		IsWml();
	Content_t	GetContentType() { return m_content_type; }

	UINT32		GetCodePage() { return m_code_page; } ///< Returns the current codepage
};

#endif // WBXML_H

#endif // WBXML_SUPPORT
