// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Yngve Pettersen
//
//

#ifndef _MIME_CS_H
#define _MIME_CS_H

#ifdef _MIME_SUPPORT_

#include "modules/datastream/fl_lib.h"
#include "modules/cache/url_cs.h"

class MIME_Decoder;

class MIME_attach_element_url : public ListElement<MIME_attach_element_url>
{
public:
	URL_InUse attachment;
	BOOL displayed;
	BOOL is_icon;

	MIME_attach_element_url(URL &url, BOOL disp, BOOL isicon);
	virtual ~MIME_attach_element_url();
};

#ifndef RAMCACHE_ONLY
class Decode_Storage
  : public Persistent_Storage
{
protected:
	MIME_ScriptEmbedSourcePolicyType	script_embed_policy;
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	BOOL valid_mhtml_archive;
#endif

public:
	Decode_Storage(URL_Rep *url_rep);

	/**
	 * Second phase constructor. You must call this method prior to using the 
	 * Decode_Storage object, unless it was created using the Create() method.
	 *
	 * @return OP_STATUS Status of the construction, always check this.
	 */
	
	OP_STATUS Construct(URL_Rep *url_rep);
	
	/**
	 * OOM safe creation of a Decode_Storage object.
	 *
	 * @return Decode_Storage* The created object if successful and NULL otherwise.
	 */

	static Decode_Storage* Create(URL_Rep *url_rep);
	virtual BOOL IsProcessed(){return TRUE;}

	CACHE_STORAGE_DESC("Decode");
};
#else
class Decode_Storage
 : public Memory_Only_Storage
{
protected:
	MIME_ScriptEmbedSourcePolicyType	script_embed_policy;
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	BOOL valid_mhtml_archive;
#endif

public:
	Decode_Storage(URL_Rep *url_rep);
	virtual BOOL IsProcessed(){return TRUE;}
	CACHE_STORAGE_DESC("Decode")
};
#endif


class DecodedMIME_Storage 
 : public Decode_Storage
{
private:

	AutoDeleteList<MIME_attach_element_url> attached_urls;
	MIME_Decoder *decoder;
	BOOL	writing_to_self;
	BOOL	prefer_plain;
	BOOL	ignore_warnings;
	int		body_part_count;

	DataStream_FIFO_Stream data;
	BOOL	decode_only;
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
	URL_CONTEXT_ID context_id;
#endif

protected:
	BOOL GetWritingToSelf(){return writing_to_self;}
	virtual void SetBigAttachmentIcons(BOOL isbig);
	virtual void SetPreferPlaintext(BOOL plain) {prefer_plain=plain;}
	virtual void SetIgnoreWarnings(BOOL ignore) {ignore_warnings=ignore;}
	void WriteToDecoder(const unsigned char *src, unsigned long src_len);
	virtual void ProcessData()=0;

private:
	void CreateDecoder(const unsigned char *src, unsigned long src_len);

public:
	DecodedMIME_Storage(URL_Rep *url_rep);
	virtual ~DecodedMIME_Storage();

	OP_STATUS Construct(URL_Rep *url_rep);
	//static DecodedMIME_Storage* Create(URL_Rep *url_rep);

	void AddMIMEAttachment(URL &associate_url, BOOL displayed, BOOL isicon=FALSE, BOOL isbodypart=FALSE);
	virtual BOOL GetAttachment(int i, URL &url);
	virtual BOOL IsMHTML() const;
	virtual URL GetMHTMLRootPart();

	//virtual void	StoreData(const char *buffer, unsigned long buf_len); // Unused
	virtual unsigned long RetrieveData(URL_DataDescriptor *desc,BOOL &more);
	virtual void	SetFinished(BOOL force);
	virtual BOOL	Purge();
	virtual BOOL	Flush();
	virtual URLCacheType	GetCacheType() const { return URL_CACHE_MHTML;}
	//virtual const uni_char *FileName() const;
	//virtual const uni_char *FilePathName() const;
	virtual URL_DataDescriptor *GetDescriptor(MessageHandler *mh=NULL, BOOL get_raw_data = FALSE, BOOL get_decoded_data=TRUE, Window *window = NULL, 
					URLContentType override_content_type = URL_UNDETERMINED_CONTENT, unsigned short override_charset_id = 0, BOOL get_original_content=FALSE, unsigned short parent_charset_id = 0);

	BOOL GetDecodeOnly() {return decode_only;}
	void SetDecodeOnly(BOOL dec_only) {decode_only=dec_only;}
	int GetBodyPartCount() {return body_part_count;}
	CACHE_STORAGE_DESC("DecodeMIME");
};

/**
 * @brief Created by URL for MIME documents
 *
 * Creates MIME_Decoders during a data loading, which in turn create MIME_Decoders for each MIME part.
 * MIME part contains data will create a new temporary URL.
 */
class MIME_DecodeCache_Storage
 : public DecodedMIME_Storage
{
private:
	Cache_Storage *source;
	URL_DataDescriptor *desc;

protected:
	virtual void ProcessData();


public:
	
	MIME_DecodeCache_Storage(URL_Rep *url_rep, Cache_Storage *src);
	virtual ~MIME_DecodeCache_Storage();

	OP_STATUS Construct(URL_Rep *url_rep, Cache_Storage *src);
	//static MIME_DecodeCache_Storage* Create(URL_Rep *url_rep, Cache_Storage *src);

	
	virtual OP_STATUS	StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position = FILE_LENGTH_NONE);
	//virtual unsigned long RetrieveData(URL_DataDescriptor *desc,BOOL &more);
	virtual void	SetFinished(BOOL force);
	virtual BOOL	Purge();
	virtual BOOL	Flush();
	virtual const OpStringC FileName(OpFileFolder &folder, BOOL get_original_body = TRUE) const;
	virtual OP_STATUS FilePathName(OpString &name, BOOL get_original_body = TRUE) const;
	//virtual URL_DataDescriptor *GetDescriptor(MessageHandler *mh=NULL, BOOL get_raw_data = FALSE, BOOL get_decoded_data=TRUE, Window *window = NULL);
	virtual unsigned long SaveToFile(const OpStringC &);
	virtual URL_DataDescriptor *GetDescriptor(MessageHandler *mh=NULL, BOOL get_raw_data = FALSE, BOOL get_decoded_data=TRUE, Window *window = NULL, URLContentType override_content_type = URL_UNDETERMINED_CONTENT, unsigned short override_charset_id = 0, BOOL get_original_content=FALSE, unsigned short parent_charset_id = 0);
	virtual void TakeOverContentEncoding(OpStringS8 &enc){if(source) source->TakeOverContentEncoding(enc);}
	CACHE_STORAGE_DESC("MIME_DecodeCache");
};


#endif

#endif // !_MIME_CS_H_
