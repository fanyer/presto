/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _MIMEDEC2_H_
#define _MIMEDEC2_H_

#ifdef _MIME_SUPPORT_

//#define _SUPPORT_SMIME_

#include "modules/url/url2.h"

#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"

#include "modules/datastream/fl_lib.h"
#include "modules/mime/mimeutil.h"
#include "modules/mime/mime_enum.h"
#include "modules/mime/multipart_parser/abstract_mpp.h"
#include "modules/util/adt/opvector.h"

#ifdef _DEBUG
#ifdef YNP_WORK
#define MIME_DEBUG
#endif
#endif

class DecodedMIME_Storage;
class Header_List; // Upload
class SubElementId;
/* All input data are considered 8 bit unless specified a binary by headers
 * output data is a HTML document that may contain any character set
 */

/**
 * @brief Base class for all MIME decoders.
 *
 * A MIME decoder represents one MIME part of a document. The part can again consist of several nested MIME parts.
 */
class MIME_Decoder 
	: public ListElement<MIME_Decoder>
{
protected:
	friend class MIME_Multipart_Related_Decoder;

	struct Mime_flags_st{
		UINT header_loaded:1;
		UINT finished:1;
		UINT decode_warning:1;

		UINT uudecode_started:1;
		UINT uudecode_ended:1;

		UINT display_headers:1;
		UINT writtenheaders:1;
		UINT displayed:1;

		UINT use_no_store_flag:1;
		UINT cloc_base_is_local:1;
	} info;

	HeaderList headers;
	HeaderEntry *content_type_header; // do not delete;

	MIME_encoding encoding;
	MIME_ContentTypeID content_typeid;
	MIME_ScriptEmbedSourcePolicyType script_embed_policy;

	OpString8 default_charset;
	unsigned short forced_charset_id;

#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
	URL_CONTEXT_ID context_id;
#endif

protected:
	const MIME_Decoder *parent;
	URL_Rep*	base_url;
	URLType		base_url_type;

	DataStream_FIFO_Stream unprocessed_in;
	DataStream_FIFO_Stream unprocessed_decoded;

protected:
	URL			cloc_url;
	URL			cloc_base_url;
	BOOL		big_icons;
	BOOL		prefer_plain;
	BOOL		ignore_warnings;
	UINT		nesting_depth;
	UINT		total_number_of_parts;
	UINT*		number_of_parts_counter;

	OpString	encap_id;


private:
	void ProcessDataL(BOOL no_more);

	unsigned long DecodeQuotedPrintableL(BOOL no_more);
	unsigned long DecodeBase64L(BOOL no_more);
	unsigned long DecodeUUencodeL(BOOL no_more);

	void HandleDecodedDataL(BOOL more);

	virtual void RegisterFilenameL(OpStringC8 fname);

protected:
	virtual void HandleHeaderLoadedL();

	void WriteStartofDocumentL(URL &target, OpStringC *mimestyleFile=NULL);
	void WriteHeadersL(URL &target, DecodedMIME_Storage *attach_target);
	void WriteAttachmentListL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links=TRUE);

	virtual void WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target)=0;
	virtual void WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links=TRUE)=0;

	void WriteEndOfDocument(URL &target);

	static MIME_ContentTypeID FindContentTypeId(const char *type_string);
	virtual void ProcessDecodedDataL(BOOL more)=0;
	virtual void HandleFinishedL();

	virtual void PerformSpecialProcessingL(const unsigned char *src, unsigned long src_len);
	virtual void WriteSpecialHeaders(URL &target);

	void RaiseDecodeWarning(){if (!ignore_warnings) info.decode_warning = TRUE;}

protected:
	const unsigned char *AccessDecodedData(){return unprocessed_decoded.GetDirectPayload();}
	unsigned long GetLengthDecodedData(){return unprocessed_decoded.GetLength();}
	void CommitDecodedDataL(unsigned long len){unprocessed_decoded.CommitSampledBytesL(len);}
	BOOL MoreDecodedDataL(){return unprocessed_decoded.MoreData();}
	DataStream_FIFO_Stream &AccessDecodedStream(){return unprocessed_decoded;};

	URL	ConstructAttachmentURL_L(HeaderEntry *content_id, const OpStringC &ext, HeaderEntry *content_type, const OpStringC &filename, URL *content_id_url=NULL);
	//URL	ConstructContentIDURL_L(const OpStringC8 &content_id);

public:

	MIME_Decoder(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT); // Header elements are moved (not copied) to this element
	virtual ~MIME_Decoder();

	virtual void ConstructL();

	/**
	 * process body of the current MIME part, without headers; can be called multiple times if all the data are not available
	 */
    void LoadDataL(const unsigned char *src, unsigned long src_len);
    //void LoadDataL(const unsigned char *src);
    //void LoadDataL(const char *src, unsigned long src_len){LoadDataL((const unsigned char *) src, src_len);};
    //void LoadDataL(const char *src){LoadDataL((const unsigned char *) src);};
	//void LoadDataL(FILE *src_file);
	//void LoadDataL(URL &src_url);

	/**
	 * to be called when all the data were passed to LoadDataL
	 */
	void FinishedLoadingL();

	/**
	 * called internally, write the parsed data back to the main URL (will be held in another storage)
	 */
	void RetrieveDataL(URL &target, DecodedMIME_Storage *); // always HTML or XML 
	void RetrieveAttachementL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links){WriteDisplayAttachmentsL(target,attach_target, display_as_links);} ; // always HTML or XML 

	BOOL IsFinished(){return info.finished;}

	virtual BOOL HaveDecodeWarnings();
	virtual BOOL HaveAttachments();
	virtual BOOL IsAttachment() const { return FALSE; }
	virtual BOOL IsDisplayableInline() const { return FALSE; }
	virtual const List<MIME_Decoder>* GetSubElements() const { return NULL; }
	virtual UINT NumberOfParts() { return *number_of_parts_counter; }
	virtual URL GetRelatedStartURL() const { return URL(); } // Used by MIME_Formatted_Payload to see if it is main document 
	virtual URL GetRelatedRootPart() { return URL(); } // Equals start url or the first sub-part if there is no start url
	void SetScriptEmbed_Restrictions(MIME_ScriptEmbedSourcePolicyType policy){script_embed_policy=policy;}
	MIME_ScriptEmbedSourcePolicyType   GetScriptEmbed_Restrictions() const{return script_embed_policy;}

	virtual URL GetAttachmentURL(){return URL();}
	virtual URL GetContentIdURL(){return URL();}
	void SetBigAttachmentIcons(BOOL isbig) {big_icons=isbig;}
	void SetPreferPlaintext(BOOL plain) {prefer_plain=plain;}
	BOOL GetPreferPlaintext() const {return prefer_plain;}
	void SetIgnoreWarnings(BOOL ignore) {ignore_warnings=ignore;}
	BOOL GetIgnoreWarnings() const {return ignore_warnings;}
	void SetDisplayed(BOOL flag){info.displayed = (flag ? TRUE : FALSE);}
	void SetDisplayHeaders(BOOL flag){info.display_headers = (flag ? TRUE : FALSE);}
	void InheritAttributes(const MIME_Decoder *parent);

	/**
	 * create the appropriate MIME decoder, headers are already parsed by the caller
	 * @param parent decoder of the upper level or NULL for the topmost level
	 * @param id Content-Type header
	 * @param hdrs parsed headers
	 * @param url_type one of URL_NEWSATTACHMENT, URL_SNEWSATTACHMENT or URL_ATTACHMENT
	 * @param url The base url (for access to the Window's forceEncoding)
	 * @param ctx_id separate cache context to prevent collisions with existing cache items
	 * @return appropriate decoder derived from MIME_Decoder
	 */
	static MIME_Decoder *CreateDecoderL(const MIME_Decoder *parent, MIME_ContentTypeID id, HeaderList &hdrs, URLType url_type, URL_Rep* url
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
												, URL_CONTEXT_ID ctx_id = 0
#endif
		);

	/**
	 * create the appropriate MIME decoder from unprocessed headers
	 * @param parent decoder of the upper level or NULL for the topmost level
	 * @param src headers of the MIME part
	 * @param src_len length of the headers (body mustn't be included)
	 * @param url_type one of URL_NEWSATTACHMENT, URL_SNEWSATTACHMENT or URL_ATTACHMENT
	 * @param url The base url (for access to the Window's forceEncoding)
	 * @param ctx_id separate cache context to prevent collisions with existing cache items
	 * @return appropriate decoder derived from MIME_Decoder
	 */
	static MIME_Decoder *CreateDecoderL(const MIME_Decoder *parent, const unsigned char *src, unsigned long src_len, URLType url_type, URL_Rep* url
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
												, URL_CONTEXT_ID ctx_id=0
#endif
		);
	static MIME_Decoder *CreateNewMIME_PayloadDecoderL(const MIME_Decoder *parent, HeaderList &hdr, URLType url_type); // Header MUST be complete, Fixed creation of a MIME_Payload object

	MIME_ContentTypeID GetContentTypeID() const{return content_typeid;};

	// Only performed when no output is generated
	void RetrieveAttachementList(DecodedMIME_Storage *);

	virtual void SetUseNoStoreFlag(BOOL no_store);
	BOOL GetUseNoStoreFlag() const {return (BOOL) info.use_no_store_flag;}

	virtual void SetForceCharset(unsigned short charset_id) {forced_charset_id = charset_id;}

	void SetContentLocationBaseURL(URL &cloc){if(cloc_base_url.IsEmpty()){cloc_base_url = cloc;info.cloc_base_is_local= FALSE;}}
	URL GetContentLocationBaseURL() const{return cloc_base_url;}

	virtual void UnSetAttachmentFlag(){};

#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
	void SetContextID(URL_CONTEXT_ID id){context_id = id;};
	URL_CONTEXT_ID GetContextID() const{return context_id;};
#endif
	
	void SetParent(const MIME_Decoder* p) { parent = p; }
	const MIME_Decoder* GetParent() const { return parent; }
	void SetBaseURL(URL_Rep* url) { base_url = url; }
	virtual BOOL ReferenceAll(OpVector<SubElementId>& ids) { return FALSE; }

#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	static BOOL IsValidMHTMLArchiveURL(URL_Rep* url);
	BOOL IsValidMHTMLArchive();
#endif
};

/** Basic handling of multipart organized bodies, not encoding specific. */
class MIME_MultipartBase : public MIME_Decoder
{
protected:
	MIME_Decoder *current_element;
	AutoDeleteList<MIME_Decoder> sub_elements;

	MIME_Decoder *current_display_item;

	BOOL	display_attachment_list;

protected: // Only to be used by subclasses
	MIME_MultipartBase(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);
	virtual ~MIME_MultipartBase();

protected:
	virtual MIME_Decoder *CreateNewBodyPartL(MIME_ContentTypeID id, HeaderList &hdrs, URLType url_type);
	virtual void CreateNewBodyPartL(const unsigned char *src, unsigned long src_len); // src contains only the header. The header MUST be complete
	void CreateNewBodyPartL(HeaderList &hdr); // Header MUST be complete
	void CreateNewMIME_PayloadBodyPartL(HeaderList &hdrs);
	void CreateNewBodyPartWithNewHeaderL(const OpStringC8 &content_type, const OpStringC &filename, const OpStringC8 &content_encoding);
	void CreateNewBodyPartWithNewHeaderL(const OpStringC8 &content_type, const OpStringC8 &filename, const OpStringC8 &content_encoding);
	void CreateNewBodyPartWithNewHeaderL(/*upload*/ Header_List &headers, const OpStringC8 &p_content_type, const OpStringC8 &p_content_encoding);
	virtual void FinishSubElementL(){if(current_element){ current_element->FinishedLoadingL(); current_element = NULL;}}
	virtual void HandleFinishedL();

	void SaveDataInSubElementL(const unsigned char *src, unsigned long src_len); // ONLY payload content, not bodypart header
	void SaveDataInSubElementL(); // ONLY payload content, not bodypart header

	virtual void WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target);
	virtual void WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links = TRUE);

	virtual BOOL HaveAttachments();
	virtual BOOL HaveDecodeWarnings();
	virtual const List<MIME_Decoder>* GetSubElements() const { return &sub_elements; }

	// Only performed when no output is generated
	void RetrieveAttachementList(DecodedMIME_Storage *);

	virtual void SetUseNoStoreFlag(BOOL no_store);
	virtual void SetForceCharset(unsigned short charset_id);
	virtual BOOL ReferenceAll(OpVector<SubElementId>& ids);
};

/** Decoder of multipart/mixed and application/vnd.wap.multipart.mixed bodies */
class MIME_Multipart_Decoder : public MIME_MultipartBase, public MultiPartParser_Listener
{
public:
	MIME_Multipart_Decoder(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT, BOOL binaryMultipart=FALSE);
	virtual ~MIME_Multipart_Decoder();

protected:
	BOOL binary;
	AbstractMultiPartParser* parser;

protected:
	virtual void HandleHeaderLoadedL();
	virtual void ProcessDecodedDataL(BOOL more);
	virtual void HandleFinishedL();
	virtual void OnMultiPartParserWarning(AbstractMultiPartParser::Warning reason, unsigned int offset, unsigned int part);

	virtual void WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links = TRUE)
	{
		MIME_MultipartBase::WriteDisplayAttachmentsL(target, attach_target, TRUE);
	}
};

/** Decoder of multipart/related bodies */
class MIME_Multipart_Related_Decoder : public MIME_Multipart_Decoder
{
private:
	URL_InUse	related_start_url;

public:
	MIME_Multipart_Related_Decoder(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);
	virtual ~MIME_Multipart_Related_Decoder();

protected:
	virtual void HandleHeaderLoadedL();

	virtual void WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target);
	virtual void WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links = TRUE);

	virtual URL GetRelatedStartURL() const { return const_cast<MIME_Multipart_Related_Decoder*>(this)->related_start_url.GetURL(); }
	virtual MIME_Decoder *GetRootPart();
	virtual URL GetRelatedRootPart();
};

/** Decoder of multipart/alternative bodies */
class MIME_Multipart_Alternative_Decoder : public MIME_Multipart_Decoder
{
private:
	MIME_Decoder *FindDisplayItem();

public:
	MIME_Multipart_Alternative_Decoder(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);
	virtual void WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target);
	virtual void WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links = TRUE);
	virtual BOOL ReferenceAll(OpVector<SubElementId>& ids);
};


/** Decoder of text/plain bodies that may include UUencoded or PGP encoded bodyparts (in the future. also YEnc) */
class MIME_Unprocessed_Text : public MIME_MultipartBase
{
private:
	enum MIME_textbased_parttype {MIME_plain_text, MIME_UUencode, MIME_PGP_body};
	
	MIME_textbased_parttype encoding_type;

	//MIME_Decoder *primary_payload;

public:
	MIME_Unprocessed_Text(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);

protected:
	virtual void HandleHeaderLoadedL();
	virtual void ProcessDecodedDataL(BOOL more);

private:
	void CreateTextPayloadElementL(BOOL first);

};

/** Decoder of application/mime bodies */
class MIME_Mime_Payload : public MIME_MultipartBase
{
public:
	MIME_Mime_Payload(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);

protected:
	virtual void ProcessDecodedDataL(BOOL more);
};

/** Decoder of ignored bodies (used for nested body attacks, etc) */
class MIME_IgnoredBody : public MIME_MultipartBase
{
public:
	MIME_IgnoredBody(HeaderList &hdrs, URLType url_type) : MIME_MultipartBase(hdrs, url_type) {}
	virtual BOOL HaveDecodeWarnings() { return !ignore_warnings; }

protected:
	virtual void ProcessDecodedDataL(BOOL more) {}
};

/** General payload handler, default as attachment */
class MIME_Payload : public MIME_Decoder
{
protected:
	OpString name;
	OpString filename;
	BOOL is_attachment;

	URL			alias;
	URL			alias1;
	URL			content_id_url;
	URL			mimepart_object_url;

	URL_InUse	attachment;

public:
	MIME_Payload(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);
	virtual ~MIME_Payload();

	virtual URL GetAttachmentURL(){return attachment.GetURL();}
	virtual URL GetContentIdURL(){return content_id_url;}
	virtual BOOL HaveAttachments();
	virtual BOOL IsAttachment() const { return is_attachment; }

protected:
	virtual void HandleHeaderLoadedL();
	virtual void ProcessDecodedDataL(BOOL more);
	virtual void HandleFinishedL();
	URL	ConstructAttachmentURL_L(HeaderEntry *content_id, const uni_char *ext, HeaderEntry *content_type=NULL);

	virtual void RegisterFilenameL(OpStringC8 fname);

	virtual void WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target);
	virtual void WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links = TRUE);

	// Only performed when no output is generated
	void RetrieveAttachementList(DecodedMIME_Storage *);
	virtual void SetUseNoStoreFlag(BOOL no_store);

	virtual void UnSetAttachmentFlag(){is_attachment = FALSE;};
};

/** Plain text payload handler, will convert the textfile to HTML if it is to be displayed */
class MIME_Text_Payload : public MIME_Payload
{
private:
    int         m_plaintext_current_quotelevel;
    BOOL        m_plaintext_previous_line_flowed;
    BOOL        m_plaintext_first_line_on_level;
    BOOL        m_plaintext_has_signature;

	URL_DataDescriptor *text_desc;

public:
	MIME_Text_Payload(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);
	virtual ~MIME_Text_Payload();

	virtual void WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target);
	static BOOL PeekIsTextRTL(URL_DataDescriptor *text_desc);
};


/** Used for HTML/XML content */
class MIME_Formatted_Payload : public MIME_Payload
{
public:
	MIME_Formatted_Payload(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);

	virtual void WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target);
	virtual BOOL ReferenceAll(OpVector<SubElementId>& ids);
};

/** Used for images and audio embeds */
class MIME_Displayable_Payload : public MIME_Payload
{
public:
	MIME_Displayable_Payload(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);

	virtual void WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target);
	virtual BOOL IsDisplayableInline() const { return TRUE; }
};

/** Used for images and audio embeds */
class MIME_External_Payload : public MIME_Payload
{
public:
	MIME_External_Payload(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);

	virtual void HandleHeaderLoadedL();
};

/***************************************************************************/

class SubElementId
{
private:
	MIME_Decoder* mdec;
	const char* id[2]; /// Content-Id or Content-Location
	unsigned long idLength[2];
	BOOL found;
public:
	SubElementId(MIME_Decoder* decoder, HeaderList& headers);
	unsigned long MaxLength(unsigned long maxLength);
	BOOL FindIn(const uni_char* buffer, unsigned long length);
	BOOL Found() { return found; }
};

// _MIME_SUPPORT_
#endif
#endif
