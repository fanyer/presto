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


#ifndef _MUL_PAR_CS_H
#define _MUL_PAR_CS_H

#include "modules/cache/url_cs.h"

class MultipartStorage_Item : public ListElement<MultipartStorage_Item>
{
public:
	Cache_Storage *item;
	BOOL header_loaded_sent;
	
public:
	MultipartStorage_Item(Cache_Storage *_item) : item(_item), header_loaded_sent(FALSE) {}
	virtual ~MultipartStorage_Item(){OP_DELETE(item); if(InList()) Out();}
};

class Multipart_CacheStorage : public StreamCache_Storage
{
private:
	AutoDeleteList<MultipartStorage_Item> used_cache_items;
	AutoDeleteList<MultipartStorage_Item> unused_cache_items;

	MultipartStorage_Item *loading_item;

#if defined(MIME_ALLOW_MULTIPART_CACHE_FILL) || defined(WBMULTIPART_MIXED_SUPPORT)
	URL_InUse current_target;
#endif

	BOOL first_bodypart_created;
	Multipart_Status bodypart_status;

protected:
	URL_DataDescriptor *desc;

public:

	Multipart_CacheStorage(URL_Rep *url);
	virtual ~Multipart_CacheStorage();

    void ConstructL(Cache_Storage * initial_storage, OpStringS8 &content_encoding);

protected:

	void CreateNextElementL(URL new_target, const OpStringC8 &content_type, OpStringS8 &content_encoding, BOOL no_store);
	void AddAliasL(URL alias);

	void WriteDocumentDataL(const unsigned char *buffer, unsigned long buf_len, BOOL more);

	virtual void ProcessDataL()=0;

public:
	virtual OP_STATUS	StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position=FILE_LENGTH_NONE);
	virtual URL_DataDescriptor *GetDescriptor(MessageHandler *mh=NULL, BOOL get_raw_data = FALSE, BOOL get_decoded_data=TRUE, Window *window = NULL, URLContentType override_content_type = URL_UNDETERMINED_CONTENT, unsigned short override_charset_id = 0, BOOL get_original_content = FALSE, unsigned short parent_charset_id = 0);
	virtual OpFileLength ContentLoaded(BOOL force=FALSE);

	virtual BOOL	Flush();
	virtual BOOL	Purge();
	virtual void	TruncateAndReset();
	virtual void	SetFinished(BOOL force = FALSE);
	virtual void	MultipartSetFinished(BOOL force=FALSE );
	virtual BOOL	GetIsMultipart() const{return TRUE;}
	virtual BOOL	IsLoadingPart();
	virtual BOOL	FirstBodypartCreated() const {return first_bodypart_created; }
	virtual void SetMultipartStatus(Multipart_Status status);
	virtual Multipart_Status GetMultipartStatus();

	virtual URLContentType	GetContentType() const;
	virtual const OpStringC8 GetMIME_Type() const;
	CACHE_STORAGE_DESC("Multipart_Cache")
};

#endif // _MUL_PAR_CS_H
