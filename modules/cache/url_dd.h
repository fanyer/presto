/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */

// URL Data Descriptor

#ifndef URL_DD_H_
#define URL_DD_H_

#include "modules/util/simset.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/cache/cache_utils.h"

class URL;
class URL_Rep;
class MessageHandler;
class Data_Decoder;
class Cache_Storage;
class DataStream;
class OpFileDescriptor;

/** Helper class to keep track of MSG_URL_DATA_LOADED messages */
class MessageTracker {
	bool nothing_consumed_or_added;			/**< If TRUE, nothing has been consumed or added since last StartBlocking */
	bool block_msg_data_loaded;				/**< If TRUE, MSG_URL_DATA_LOADED will be blocked because because nothing_consumed_or_added was TRUE */
	bool msg_data_loaded_was_blocked;		/**< If TRUE, we would have posted MSG_URL_DATA_LOADED, but didn't because block_msg_data_loaded was TRUE */
	unsigned long msg_data_loaded_delay;	/**< The delay we would have posted MSG_URL_DATA_LOADED with */
public:
	MessageTracker();
	/** Start blocking MSG_URL_DATA_LOADED if nothing has been consumed or added since last time, and storage doesn't have more either */
	void MaybeStartBlocking(BOOL storage_has_more);
	/** Stop blocking MSG_URL_DATA_LOADED */
	void StopBlocking();
	/** Stop blocking and forget if a message was blocked */
	void ClearBlockingState();
	/** Return TRUE if the given message should be blocked */
	BOOL BlockMessage(OpMessage Msg, unsigned long delay = 0);
	/** Tell MessageTracker if any data was consumed or added. Returns TRUE if amount != 0 */
	BOOL DataConsumedOrAdded(unsigned long amount);
	/** If a message was blocked, post it now */
	void PostMessageIfBlocked(class URL_DataDescriptor *dd);
};


/** Used to retrieve data from a URL's cache storage element.
 *
 *	This class retrives a block of data from a URL, if necessary decompressing it and/or 
 *	converting it UTF-16 encoding, and lets the owner of the object access the buffer, 
 *	process it, and then continue to the next block of data.
 *
 *	Objects of this class is owned by the caller of URL::GetDescriptor, and it is the owner 
 *	that must delete them by deallocating them.
 *
 *	If initialized with a message handler the object can send MSG_URL_DATA_LOADED messages 
 *	to the owner, but only if the URL is no longer actively loading.
 *
 *	Primary API functions:
 *
 *		RetrieveData	Fills the buffer to capacity if enough data are available
 *		GetBuffer		Access the data buffer
 *		GetBufSize		How long is the buffer?
 *		ConsumeData		Mark the specified number of bytes as read, and remove them from the buffer
 *
 *		GetCharacterSet	Returns the guessed character set (if any) determined by the character converter
 *
 *	The datadescriptor is part of a linked list in the cache storage element as long as one of 
 *	them is alive
 *
 *	If no message handler is provided during construction the owner will not be informed about more data,
 *	but must poll the descriptor manually.
 *
 *	The decriptor can post the following messages:
 *
 *		MSG_URL_DATA_LOADED		:	When more data are available
 *		MSG_HEADER_LOADED		:	After MSG_MULTIPART_RELOAD
 *		MSG_MULTIPART_RELOAD	:	In case the charset guesser changed its mind, restart the document.
 *		MSG_URL_LOADING_FAILED	:	In case of errors.
 */
class URL_DataDescriptor : public Link
#ifdef DEBUG_LOAD_STATUS
	, public MessageObject
#endif
{
	friend class Cache_Storage;
	private:
		/** The URL for which this descriptor is created */
		URL url;

		/** protects the url and the url cache object from being deleted */
		URL_InUse		url_protector;

		/** The actual Cache Storage object accessed by this descriptor */
		Cache_Storage	*storage;
		
		/** The message handler used to send data */
		TwoWayPointer<MessageHandler>	mh;
		
		/** List of decoding elements. If present the conversion to UTF-16 is the 
		 *	first element, the remaining are decompression elements.
		 *	The this element retrieves daat from the first element, which again 
		 *	retrieves it from the second. The last element in the list retrieves 
		 *	it from the sub_desc datadescriptor.
		 *
		 *	If this list is empty the data are retrieved directly from the cache-storage element
		 */
		AutoDeleteHead	CE_decoding;

		/** If present, this object retrieves the raw data used by the CE_decoding chain of data decoders */
		URL_DataDescriptor *sub_desc;
		
		/** If TRUE this descriptor is receiving data that are retrieved from an open file in the cache storage, 
		 *	and when it is deleted a reference count must be decremented 
		 */
		bool is_using_file:1;

		/** Document need to be refreshed because assumptions on character
		  * set on associated document has changed.
		  */
		bool need_refresh:1;

		/** If TRUE, a message has already been posted, no need to post another */
		bool posted_message:1;

		/** If TRUE, a message has already been posted, no need to post another */
		bool posted_delayed_message:1;

		/** Of TRUE, the multipart iteration call can been used. */
		bool mulpart_iterable:1;

		/** Helper class to keep track of MSG_URL_DATA_LOADED messages */
		MessageTracker messageTracker;

		/** position in a file or memory buffer */
		OpFileLength	position;

		/** Tell this descriptor which cache storage object it belongs to */
		void			SetStorage(Cache_Storage *strage);

		unsigned long	RetrieveDataInternalL(BOOL &more);

#ifdef _DEBUG
		BOOL is_utf_16_content;
#endif
		URLContentType	content_type;
		unsigned short	charset_id;
#ifdef _CACHE_DESCRIPTOR_MIMETYPE
		OpString8 content_type_string;
#endif

		/** The encoding of the parent document from which we are loaded, if any */
		unsigned short	parent_charset;

    protected: // these need to be protected so that url_i18n.cpp can access

		/** Buffer */
		char*			buffer;

		/** Maximum length of buffer */
		unsigned long	buffer_len;

		/** Current length of buffer */
		unsigned long	buffer_used;        

	public:

		/** Policy used when requesting a Grow() */
		enum GrowPolicy
		{
			// The buffer will grow to the requested size, but it will be at least as big as DATADESC_BUFFER_SIZE (min = DATADESC_BUFFER_SIZE)
			GROW_BIG,
			// The buffer will grow to the requested size, but it will not exceed DATADESC_BUFFER_SIZE (max = DATADESC_BUFFER_SIZE)
			GROW_SMALL,
		};

#ifdef DEBUG_LOAD_STATUS
		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
#endif

		/** Constructor
		 *
		 *	@param	url_rep		Pointer to the URL this descriptor is retrieving data from
		 *	@param	mh			Optional message handler used to post the MSG_URL_DATA_LOADED messages
		 *	@param	override_content_type
		 *						Content-Type to load this resource as, or
		 *						URL_UNDETERMINED_CONTENT to not override.
		 *	@param	override_charset_id
		 *						The ID of the character encoding to load this
		 *						resource with, or 0 to not override.
		 *	@param	parent_charset_id
		 *						Character encoding (charset) passed from the
		 *						parent resource. Should either be the actual
		 *						encoding of the enclosing resource, or an
		 *						encoding recommended by it, for instance as
		 *						set by a CHARSET attribute in a SCRIPT tag.
		 *						Pass 0 if no such contextual information
		 *						is available.
		 */
						URL_DataDescriptor(URL_Rep *url_rep, MessageHandler *mh, URLContentType override_content_type = URL_UNDETERMINED_CONTENT, unsigned short override_charset_id = 0, unsigned short parent_charset_id = 0);

		/** Initialize the descriptor
		 *
		 *	@param	get_raw_data	If TRUE, no conversion of the data read from the Cache_Storage is made	
		 *	@param	window			pointer to Window object used to determine the forced character set to be used (if necessary)
		 *
		 *	@eturn	OP_STATUS		status of operation
		 */
		OP_STATUS		Init(BOOL get_raw_data = FALSE, Window *window = NULL);

		/** Destructor */
		virtual 		~URL_DataDescriptor();

		/** Fill the vector with the coverage of the requested part of the file; the segments are sorted by start
		 *  segments can be "merged" (0->100 + 100->300  ==>  0==>300)
		 *  Remember: this function is really useful for Multimedia files, when a Multimedia_Storage is used
		 *
		 *  @param 	out_segments	Output list of the segments available
		 *  @param 	start			Starting byte of the range
		 *  @param 	len				Length of the range
		 *  @param 	merge 			TRUE to merge contigous segments in a single segment
		*/
		OP_STATUS GetSortedCoverage(OpAutoVector<StorageSegment> &out_segments, OpFileLength start=0, OpFileLength len=INT_MAX, BOOL merge=TRUE);


#ifdef _HTTP_COMPRESS
		/** Use the string argument to set up a sequence of content decoders (gunzip, etc) to decode the raw data */
		void			SetupContentDecodingL(const char *);
		/** Like SetupContentDecodingL(), but exceptions are trapped and silently ignored. */
		void			SetupContentDecoding(const char *enc) {TRAPD(op_err, SetupContentDecodingL(enc));};
#endif // _HTTP_COMPRESS

		/** Retrieve as much data as possible from the URL
		 *
		 *	@param	more	Set to TRUE if there is more data available, and FALSE if there is no
		 *					more data at the present time. FALSE only indicates end of loading if
		 *					URL::KLoadStatus of the URL is != URL_LOADING, otherwise the owner
		 *					should try again later.
		 *
		 *					If a message handler has been specified, this function may cause
		 *					messages to be posted. No further messages will be posted until
		 *					either this function or ClearPostedMessage() is called next.
		 *
		 *					If there is data present, a new (possibly spurious) MSG_URL_DATA_LOADED
		 *					will be posted as long as something was added to the data descriptor or
		 *					the client consumed something since the last call to RetrieveData. If
		 *					MSG_URL_DATA_LOADED was not posted due to lack of activity, it will be
		 *					posted later on ConsumeData if a nonzero amount is consumed.
		 *
		 *	@return	unsigned long	Number of bytes in the buffer.
		 */
		unsigned long	RetrieveDataL(BOOL &more);
		/** Like RetrieveDataL(), but exceptions are trapped and silently ignored. */
		unsigned long	RetrieveData(BOOL &more) {OP_MEMORY_VAR unsigned long retval = 0; TRAPD(op_err, retval = RetrieveDataL(more)); return retval;};

		/** Remove the specified number of bytes from the buffer */
		void			ConsumeData(unsigned len);

		/** Clear the posted message flags, so that messages will keep
		 *  being posted even if RetrieveData() is not called again. */
		void			ClearPostedMessage() { posted_message = posted_delayed_message = FALSE; if (sub_desc) sub_desc->ClearPostedMessage(); messageTracker.ClearBlockingState(); }

		/** Increase the buffer to the specified size or enlarge it with a 
		 *	hardcoded number of bytes compared to its present maximum size.
		 *	
		 *	This should only be used in cases where the owner cannot proceed without 
		 *	having all the necessary data available in the buffer.
		 */
        unsigned int    Grow(unsigned long requested_size=0, GrowPolicy policy = GROW_BIG);

		/** Get the URL currently accessed */
		URL				GetURL() { return url; }

		/** returns TRUE if the URL has finished loading and there are no more data waiting to be read */
		BOOL			FinishedLoading();

		/** Used by Cache_Storage to add data  to the buffer */
		unsigned long	AddContentL(DataStream *src, const unsigned char *buf, unsigned long len
#if LOCAL_STORAGE_CACHE_LIMIT>0
			, BOOL more = TRUE
#endif
			);

		/** Used by Cache_Storage to add data  to the buffer */
		OpFileLength	AddContentL(OpFileDescriptor *file, OpFileLength len, BOOL &more);

		/** Return TRUE if there is still room for data in the buffer */
		BOOL			RoomForContent() const;

		/** Get the pointer to the buffer */
		const char*		GetBuffer() { return buffer; }

		/** Get the number of bytes currently in the buffer */
		unsigned long	GetBufSize() const { return buffer_used; }

		/** Get the maximum length of the buffer */
		unsigned long	GetBufMaxLength() const { return buffer_len; }

		/** Get the current position in the document */
		OpFileLength	GetPosition() const { return position; }

		/** Set the position, start reading from the beginning of the document */
		OP_STATUS		SetPosition(OpFileLength pos);

		/** Reset the position, start reading from the beginning of the document */
		void			ResetPosition() { position = 0; buffer_used = 0; }

		/** Post a message, if possible to the owner */
		void			PostMessage(OpMessage Msg, MH_PARAM_1 par1, MH_PARAM_2 par2) {
							OP_ASSERT(Msg != MSG_URL_DATA_LOADED || ((URL_ID)par1 == url.Id(FALSE) && par2 == 0)); // The message should be for our own url
							if (mh.get() != NULL && !messageTracker.BlockMessage(Msg))
							{
								if(PostedDelayedMessage())
								{
									if(sub_desc)
										sub_desc->posted_delayed_message = FALSE;	
									mh->RemoveDelayedMessage(MSG_URL_DATA_LOADED, par1, 0); 
								}
								mh->PostMessage(Msg, par1, par2); 
								posted_message= TRUE;
								posted_delayed_message = FALSE;
							}
						}

		/** Post a delayed message, if possible to the owner */
		void			PostDelayedMessage(OpMessage Msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay) {
								if (mh.get() != NULL && !messageTracker.BlockMessage(Msg, delay))
								{
									mh->PostDelayedMessage(Msg, par1, par2, delay); 
									posted_message= FALSE; 
									posted_delayed_message = TRUE;
								}
							}
		
		/** Has a message been posted? */
		BOOL			PostedMessage() const{return posted_message || (sub_desc && sub_desc->PostedMessage());}
		
		/** Has a delayed message been posted? */
		BOOL			PostedDelayedMessage() const{return posted_delayed_message || (sub_desc && sub_desc->PostedDelayedMessage());}

		/** Do we need to reread the document because the initial charset guess was wrong */
		BOOL			NeedRefresh() const{return need_refresh;}

		/** Does this object have a message handler? */
		BOOL			HasMessageHandler(){return mh.get() != NULL;}

		/** Update the messagehandler */
		void			SetMessageHandler(MessageHandler *pmh){mh = pmh;};

		/** Flag this object as using a file */
		void			SetUsingFile(BOOL val){is_using_file = !!val;}

		/** Is this object using a file? */
		BOOL			GetUsingFile() const{return is_using_file;}

		/** Get the pointer to the first DataDecoder */
		Data_Decoder*	GetFirstDecoder();

		void SetSubDescriptor(URL_DataDescriptor *desc){OP_DELETE(sub_desc); sub_desc = desc;};

		URL_DataDescriptor *Suc(){ return (URL_DataDescriptor *) Link::Suc();};
		URL_DataDescriptor *Pred(){ return (URL_DataDescriptor *) Link::Pred();};

		/** Get window associated with this data descriptor, if possible.
		  * @return Associated window, or NULL if none is available.
		  */
		Window			*GetDocumentWindow() const
						{ return mh.get() != NULL ? mh->GetWindow() : NULL; };
		MessageHandler	*GetMessageHandler() const{return mh;}

		/** Retrieve the character set used in the character converter
		  * associated with this descriptor.
		  * @return Name of character set if found, otherwise NULL.
		  */
		const char		*GetCharacterSet();
		/** Stop guessing character set for this document.
		  * Character set detection wastes cycles, and avoiding it if possible
		  * is nice.
		  */
		void			StopGuessingCharset();

		URLContentType	GetContentType() const {return content_type;};
		unsigned short	GetCharsetID() const {return charset_id;};
		void			SetContentType(URLContentType typ){content_type=typ;}
		void			SetCharsetID(unsigned short id);

		void			SetMultipartIterable(){mulpart_iterable = TRUE;}

		/** Set the content type string of this descriptor
		 *	Requires the caller module to import API_CACHE_DESCRIPTOR_MIME_TYPE
		 */
		void			SetContentTypeL(const OpStringC8 &typ)
#ifdef _CACHE_DESCRIPTOR_MIMETYPE
						{content_type_string.SetL(typ);}
#endif
						;

		/** Get the content type string of this descriptor
		 *	Requires the caller module to import API_CACHE_DESCRIPTOR_MIME_TYPE
		 */
		const OpStringC8 GetMIME_Type() const;

		/** Returns the character offset of the first invalid character
		 *  (according to the charset decoder,) or -1 if not known or if no
		 *  invalid characters have been found. */
		int GetFirstInvalidCharacterOffset();
};

#endif // !URL_DD_H_
