/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef URL_CS_H_
#define URL_CS_H_

#include "modules/util/opfile/opfile.h"
#include "modules/datastream/fl_lib.h"
#ifdef CACHE_ENABLE_LOCAL_ZIP
#include "modules/datastream/fl_zip.h"
#endif

#if CACHE_CONTAINERS_ENTRIES>0
	#include "modules/cache/cache_container.h"
#endif

#include "modules/cache/cache_utils.h"
#include "modules/cache/context_man.h"
#include "modules/cache/multimedia_cache.h"

#ifdef URL_ENABLE_ASSOCIATED_FILES
#include "modules/cache/AssociatedFile.h"
#endif

class DiskCacheEntry;
class CacheFile;

#ifdef PI_ASYNC_FILE_OP
class AsyncExporterListener;
#endif // PI_ASYNC_FILE_OP

// URL Cache Storage

enum ResourceID {
	MEMORY_RESOURCE,
	TEMP_DISK_RESOURCE,
	DISK_RESOURCE,
	SHARED_RESOURCE
};

enum Multipart_Status{
	Multipart_NotAMultipart,
	Multipart_NotStarted,
	Multipart_BoundaryRead,
	Multipart_HeaderLoaded,
	Multipart_BodyContent,
	Multipart_NextBodypart
};

/// Macro used to add a description to each type of cache storage
#ifdef SELFTEST
	#define CACHE_STORAGE_DESC(A) virtual const uni_char *GetCacheDescription() {  const uni_char * desc = UNI_L(A); return desc; }
#else
	#define CACHE_STORAGE_DESC(A)
#endif


class FileName_Store;
class OpFile;

/**
	Interface used for using the storage "asynchronously" 
*/
class StorageListener
{
public:
	/**
		Event sent when some bytes have been read and are available
		@param buf Buffer
		@param result Result of the operation
		@param length Number of bytes available
		@param more TRUE if there are more bytes to read
	*/
	virtual void OnDataRead(void *buf, OP_STATUS result, UINT32 len, BOOL more) = 0;
	virtual ~StorageListener() { }
};

class Context_Manager_Disk;
class Context_Manager;
class Cache_Storage;
class URL_Rep;
class CacheAsyncExporter;

/** Cache storage types, used with Cache_Storage::IsA. */
enum Cache_StorageType
{
	STORAGE_TYPE_GENERIC = 0,
	STORAGE_TYPE_FILE,
	STORAGE_TYPE_MEMORY,
	STORAGE_TYPE_EMBEDDED,
	STORAGE_TYPE_STREAM
};

class Cache_Storage : public Head
{
	friend class Context_Manager;
	friend class Context_Manager_Disk;
	friend class CacheTester;
	friend class CacheAsyncExporter;
	protected:
		UINT32			storage_id; // unique id of the storage - used for value of attribute KStorageId (needed in the fix for CORE-34824)
		URL_Rep*		url; // Does NOT update reference count
		DataStream_ByteArray_Base cache_content;
		OpFileLength	content_size;
		OpStringS8		content_encoding;
		BOOL			read_only;
		unsigned short	http_response_code;
		OpStringS8		content_type_string;
		URLContentType	content_type;
		unsigned short	charset_id;
		OpFileLength	save_position;  // Where to save the content (FILE_LENGTH_NONE means current position)
		BOOL			in_setfinished; // True if inside SetFinished() to avoid recursion.
		
		#ifdef CACHE_STATS
			// TRUE when the last file has been retrieved from the disk
			BOOL retrieved_from_disk;
			// Number of times that the storage has been flushed
			UINT32 stats_flushed;
		#endif
		
		#if (CACHE_SMALL_FILES_SIZE>0 || CACHE_CONTAINERS_ENTRIES>0)
		    BOOL plain_file;	// True if the file must remain "plain": not embedded, not in a container (or in the future, not compressed, not encripted...)
		#endif
		
		#if CACHE_SMALL_FILES_SIZE>0
			// True if the files has been embedded in the index
			BOOL embedded;
			
			// Check for embedding, and if required set all the variable to embed the file; return TRUE if the file is (now) embedded
			BOOL ManageEmbedding();
			// Rollback the embedding, restoring the memory counted in the manger; use carefully... it is tought for associated files, that need this functionality
			void RollBackEmbedding();
		#endif
		
		#if CACHE_CONTAINERS_ENTRIES>0
			/// ID used in the container; if 0, the file is not in a container
			UINT8 container_id;
			/// TRUE to prevent the delete of the file; this is done to avoid a performance bug in the cache,
			/// where multiple URLs tried to delete the same container
			BOOL prevent_delete;

			#ifdef SELFTEST
				/// TRUE if the container is under scrutiny to see if it is ok...
				BOOL checking_container;
			#endif // SELFTEST
		#endif // CACHE_CONTAINERS_ENTRIES>0
		#ifdef SELFTEST
			/// TRUE if this test is supposed to bypass some asserts
			BOOL bypass_asserts;
			/// TRUE to simulate an error during StoreData()
			BOOL debug_simulate_store_error;
			/// Number of physical disk read performed (so read from memory covered by LOCAL_STORAGE_CACHE_LIMIT are explicitely not checked)
			UINT32 num_disk_read;
		#endif // SELFTEST

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		BOOL			never_flush;
#endif

		/// Function used to give the possibility to the Multimedia Cache to change the position.
		virtual OP_STATUS SetWritePosition(OpFileDescriptor *file, OpFileLength start_position) { return OpStatus::OK; }

		/** Return TRUE if a circular dependency has been detected (this could generate a crash in Cache_Storage::StoreData(); see JOZO86-1593 )
		 * @param ptr Pointer that if part of this storage can generate a circular dependency
		 */
		virtual BOOL CircularReferenceDetected(Cache_Storage *ptr) { return FALSE; }

		/**
		  Function used to add the size of the URL to the size computed by the cache
		  @param url URL that will be added (it's primarily used for calling GetURLObjectSize())
		  @param ram TRUE to update the RAM cache size
		  @param disk_oem TRUE to update the Disk cache size
		  @param add_url_size_estimantion TRUE if the estimation has to be added (it has to be skipped for partial store operations)
		*/
		virtual void AddToCacheUsage(URL_Rep *url, BOOL ram, BOOL disk_oem, BOOL add_url_size_estimantion);
		/**
		  Function used to add the size of the URL to the size computed by the cache
		  @param url URL that will be added (used to call GetURLObjectSize())
		  @param ram TRUE to add the value to the RAM size
		  @param disk_oem TRUE to add the value to the Disk and OEM size
		*/
		virtual void SubFromCacheUsage(URL_Rep *url, BOOL ram, BOOL disk_oem);

		/**
		  Increment the size of the disk or of the Operator Cache of a given value, hiding the logic to choose between the two.
		  @param size_to_add size of the content of the URL
		  @param param url URL, used to call GetURLLength() and compute the estimated size of the URL
		  @param add_url_size_estimantion TRUE if we want the estimation of the URL size to be added; this is only a hint
		*/
		void AddToDiskOrOEMSize(OpFileLength size_to_add, URL_Rep *url, BOOL add_url_size_estimantion);

		/**
		  Decrement the size of the disk or of the Operator Cache of a given value, hiding the logic to choose between the two;
		  The URL size estimation will also be removed
		  @param size_to_sub size of the content of the URL
		  @param param url URL, used to call GetURLLength() and compute the estimated size of the URL
		  @enable_checks if FALSE, it disables consistency checks; basically it should always been TRUE, except for UnsetFinished()
			     (this only affect the code if SEFLTEST is on)
		*/
		void SubFromDiskOrOEMSize(OpFileLength size_to_sub, URL_Rep *url, BOOL enable_checks);

		/// TRUE if the content of the RAM has been saved
		virtual BOOL IsRAMContentComplete() { return read_only; }
		/// TRUE if the content of the Disk has been saved; it's meant to be used with IsDiskContentAvailable()
		virtual BOOL HasContentBeenSavedToDisk() { return FALSE; }
		/// TRUE if the content of the Disk is available; it's meant to be used with HasContentBeenSavedToDisk()
		virtual BOOL IsDiskContentAvailable() { return FALSE; }


		/** Return the additional content stored on disk but not counted by content_size.
			This situation is typically associated with big files, where part of the file is saved, then the memory is flushed.
		*/
		virtual OpFileLength GetContentAlreadyStoredOnDisk() { return 0; }
		/** Set the amount of additional content stored on disk but not counted by content_size
			This situation is typically associated with big files, where part of the file is saved, then the memory is flushed.
		*/
		virtual void SetContentAlreadyStoredOnDisk(OpFileLength size) { OP_ASSERT(!"This function should not be called"); }
		/** TRUE if some content has really been computed in the disk cache size, which is important to not decrease the disk size, unbalancing the calls.
		    The main problem is that the storage can become completed with a 0 bytes of content both if the URL has already been
			counted in the disk size or not.
			So we need a flag to decide if we have to subtract the dize (which will be bigger than 0, becasue fo the call to GetURLObjectSize()).
		*/
		virtual BOOL GetDiskContentComputed() { return FALSE; }
		/** Set if some content has really been computed in the disk cache size,  which is important to not decrease the disk size, unbalancing the calls.
		    The main problem is that the storage can become completed with a 0 bytes of content both if the URL has already been
			counted in the disk size or not.
			So we need a flag to decide if we have to subtract the dize (which will be bigger than 0, becasue fo the call to GetURLObjectSize()). */
		virtual void SetDiskContentComputed(BOOL value) { OP_ASSERT(!"This function should not be called"); }

#ifdef CACHE_ENABLE_LOCAL_ZIP
private:
	class InternalEncoder
	{
	private:
		DataStream_ByteArray_Base compression_fifo;
		DataStream_Compression compressor;

	public:
		InternalEncoder(DataStream_Compress_alg alg);
		~InternalEncoder(){};

		OP_STATUS Construct();
		OP_STATUS StoreData(Cache_Storage *target, const unsigned char *buffer, unsigned long buf_len);
		OP_STATUS FinishStorage(Cache_Storage *target); 
	protected:
		OP_STATUS WriteToStorage(Cache_Storage *target); 

	};

	InternalEncoder *encode_storage;
#endif // CACHE_ENABLE_LOCAL_ZIP

	private:
		void InternalInit(URL_Rep *url_rep, Cache_Storage *old);
		void InternalDestruct();
		#if CACHE_CONTAINERS_ENTRIES>0
			#ifdef SELFTEST
				// Check the coherency of the container; usefull to use with OP_ASSERT()
				BOOL CheckContainer();
			#else
				// Dummy
				BOOL CheckContainer() { return TRUE; }
			#endif
		#endif

			/** Check if ExportAsFile() can succeed, opposed to IsExportAllowed() that barely check if it is allowed,
			    for the Multimedia Cache.
			    NOTE: this function is called internally by ExportAsFile() and ExportAsFileAsync() and ExportAsFileAsync() methods.

			    @param allow_embedded_and_containers if FALSE, containers and embedded files will not be allowed
			    */
			OP_STATUS CheckExportAsFile(BOOL allow_embedded_and_containers);
			
			/**
			  Open the cache file in read only mode, with the constraints required by the ExportAsFile()
			*/
			OpFile *OpenCacheFileForExportAsReadOnly(OP_STATUS &ops);

	public:
						Cache_Storage(Cache_Storage *old);
						Cache_Storage(URL_Rep *url_rep);
		virtual			~Cache_Storage();

		/* This function should be overridden by all subclasses that ever need to be identified. */
		virtual BOOL IsA(Cache_StorageType type) { return type == STORAGE_TYPE_GENERIC; }
		
		// Set the position to use during saving (for Multimedia cache)
		void SetSavePosition(OpFileLength position) { save_position=position; } 
		/// Fill the vector with the coverage of the requested part of the file; the segments are unordered. Default implementation: one single segment with all the file.
		/// Remember: this function is really useful for Multimedia files, when a Multimedia_Storage is used
		virtual OP_STATUS GetUnsortedCoverage(OpAutoVector<StorageSegment> &out_segments, OpFileLength start=0, OpFileLength len=INT_MAX);
		/** Fill the vector with the coverage of the requested part of the file; the segments are sorted by start
			segments can be "merged" (0->100 + 100->300  ==>  0==>300)
			Remember: this function is really useful only for Multimedia files, when a Multimedia_Storage is used

			@param 	out_segments	Output list of the segments available
			@param 	start			Starting byte of the range
			@param 	len				Length of the range
			@param 	merge 			TRUE to merge contigous segments in a single segment
		*/
		virtual OP_STATUS GetSortedCoverage(OpAutoVector<StorageSegment> &out_segments, OpFileLength start=0, OpFileLength len=INT_MAX, BOOL merge=TRUE);

		/**
			Fill a vector with the list of segments that needs to be downloaded to fully cover the stream in the range requested
			@param start First byte of the range to cover
			@param len Length of the range
			@param missing Vector that will be populated with the ranges requested.
		*/
		virtual OP_STATUS GetMissingCoverage(OpAutoVector<StorageSegment> &missing, OpFileLength start=0, OpFileLength len=FILE_LENGTH_NONE) { return OpStatus::ERR_NOT_SUPPORTED; }

		/**	If the requested byte is in the segment, this method says how many bytes are available, else it returns
			the number of bytes NOT available (and that needs to be downloaded before reaching a segment).
			This bad behaviour has been chosen to avoid traversing the segments chain two times.

			So:
			- available==TRUE  ==> length is the number of bytes available on the current segment
			- available==FALSE ==> length is the number of bytes to skip or download before something is available.
								   if no other segments are available, length will be 0 (this is because the Multimedia
								   cache does not know the length of the file)

			@param position Start of the range requested
			@param available TRUE if the bytes are available, FALSE if they need to be skipped
			@param length bytes available / to skip
			@param multiple_segments TRUE if it is ok to merge different segments to get a bigger coverage (this operation is relatively slow)
		*/
		virtual void GetPartialCoverage(OpFileLength position, BOOL &available, OpFileLength &length, BOOL multiple_segments);

		/** Create a read only OpFileDescriptor that the caller must delete when it has finished using it
			Will switch cache_content to direct access
		*/
		virtual OpFileDescriptor* AccessReadOnly(OP_STATUS &op_err);

		/** Synchronously save the document as the named file. Does not decode the content. The cache object is unchanged (it uses AccesReadOnly()).
		 *  This is the method of choice for Multimedia Cache, but it enable only complete export (so if something is missing, an error is retrieved)
		 *
		 *	@param	file_name	Location to save the document
		 *	@return In case of OpStatus::ERR_NOT_SUPPORTED, URL::SaveAsFile() can be attempted.
		 */
		OP_STATUS ExportAsFile(const OpStringC &file_name);

	#ifdef PI_ASYNC_FILE_OP
		/** Asynchronously save the document as the named file. Does not decode the content. The cache object is unchanged (it uses AccesReadOnly()).
		 *  This is the method of choice for Multimedia Cache, but it enable only complete export (so if something is missing, an error is retrieved).
		 *  Note that in case of Unsupported operation, URL::SaveAsFile() will be attempted, if requested.
		 *
		 *	@param file_name File where to save the document
		 *	@param listener Listener that will be notified of the progress (it cannot be NULL)
		 *	@param param optional user parameter that will be passed to the listener
		 *	@param delete_if_error TRUE to delete the target file in case of error
		 *  @param safe_fall_back If TRUE, in case of problems, URL::SaveAsFile() will be attempted.
		 *                        This should really be TRUE, or containers and embedded files will fail... and encoded files... and more...
		 *	@return In case of OpStatus::ERR_NOT_SUPPORTED, URL::SaveAsFile() can be attempted.
		 */
		OP_STATUS ExportAsFileAsync(const OpStringC &file_name, AsyncExporterListener *listener, UINT32 param=0, BOOL delete_if_error=TRUE, BOOL safe_fall_back=TRUE);
	#endif // PI_ASYNC_FILE_OP
		/**
			Function called to verify that the export operation is possible (this is related to the Multimedia Cache, that blocks the export for incomplete files)
			@return TRUE if the call to ExportAsFile() is considered safe
		*/
		virtual BOOL IsExportAllowed() { return TRUE; }

// TODO: Incomplete, see CORE-27799
#ifdef CACHE_URL_RANGE_INTEGRATION
		/** Function Used to articulate the download in a different way, in particular the Multimedia cache can break the download in more pieces based on the
		 * content available in the cache.
		 *
		 * Note: this function MUST absolutely call URL::LoadDocument() to execute the real download. The intent is just to make possible to change the call
		 * in a transparent way, to simplify the use of the Multimedia Cache
		 *
		 *  @param	mh				MessageHandler that will be used to post update messages to the caller
		 *	@param	referer_url		The URL (if any) of the document that pointed to this URL
		 *	@param	loadinfo		Specification of cache policy, inline loading, user interaction, etc. used to determine how to load the document
		 *	@param comm_state		The status of the load action. with the following values returned
		 *
		 *	@return TRUE if the call has been customised (so LoadDocument() will stop, FALSE to not customize)
		 */
		virtual BOOL CustomizeLoadDocument(MessageHandler* mh, const URL& referer_url, const URL_LoadPolicy &loadpolicy, CommState &comm_state) { return FALSE; }
#endif// CACHE_URL_RANGE_INTEGRATION

	#ifdef CACHE_FAST_INDEX
		/// Get a reader able to retrieve the content of the file (both if it is in memory or on a file)
		/// This is a very particular function, that should be used only for SELFTESTS, because its behaviour is undefined
		/// in a lot of situations.
		/// The caller must delete the object
		/// WARNING: This method is mainly intended for testing. On Containers it can be quite slow...
		virtual SimpleStreamReader *CreateStreamReader();
	#endif

		/**
		 * Returns unique id of the storage which is used as value of KStorageId attribute (needed in the fix for CORE-34824)
		 */
		UINT32			GetStorageId() { return storage_id; }
		virtual BOOL	GetFinished() { return read_only; }
		/** Function called when the cache object is finished loading, to finalize the object for use.
		 * @param force if set to TRUE, skip activities that can prevent the operation from completing
		 *    (e.g. do not synchronously notify URL, to avoid potential race conditions)
		 */
		virtual void	SetFinished(BOOL force = FALSE);
		virtual void	UnsetFinished();
		void			ResetForLoading();

		void			SetHTTPResponseCode(unsigned short code) { http_response_code = code; }
		unsigned short	GetHTTPResponseCode() const { return http_response_code; }
		virtual URLContentType	GetContentType() const {return content_type;};
		unsigned short	GetCharsetID() const {return charset_id;};
		void			SetContentType(URLContentType typ){content_type=typ;}
		void			SetCharsetID(unsigned short id);
		void			SetMIME_TypeL(const OpStringC8 &typ){content_type_string.SetL(typ);}
		virtual const OpStringC8 GetMIME_Type() const{return content_type_string;}

		virtual void	ForceKeepOpen(){};

#ifdef CACHE_ENABLE_LOCAL_ZIP
		OP_STATUS	ConfigureEncode();
#endif
		virtual OP_STATUS	StoreDataEncode(const unsigned char *buffer, unsigned long buf_len);

		/// Add the buffer content to cache storage; a position is supplied (FILE_LENGTH_NONE means append), to support Multimedia Cache (that can store out of order content)
		virtual OP_STATUS	StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position=FILE_LENGTH_NONE);
		virtual unsigned long RetrieveData(URL_DataDescriptor *desc,BOOL &more);

#ifdef CACHE_RESOURCE_USED
		virtual OpFileLength ResourcesUsed(ResourceID resource);
#endif
		virtual unsigned long SaveToFile(const OpStringC &);
		virtual void	CloseFile();
		virtual BOOL	Flush()
		{
			#ifdef CACHE_STATS
				stats_flushed++;
			#endif
			
			#if CACHE_SMALL_FILES_SIZE>0
				if(embedded)
					return TRUE;
			#endif
			
			return FALSE;
		}
		/// Purge the cache storage, deleting files and memory if necessary
		/// @return TRUE if some operation has been done
		virtual BOOL	Purge();
		virtual void	TruncateAndReset();

		virtual URLCacheType	GetCacheType() const { return URL_CACHE_MEMORY;}
		virtual void SetCacheType(URLCacheType /*ct*/){}
		/** Get some more internal Informations
		@param streaming TRUE if this storage is streaming
		@param ram TRUE if the content is kept in RAM
		@param embedded_storage TRUE if this is an Embedded_Files_Storage (so it can be FALSE with the file still embedded)
		*/
		virtual void GetCacheInfo(BOOL &streaming, BOOL &ram, BOOL &embedded_storage)=0;
#ifdef SELFTEST
		/// Retrieve a textual description of the cache storage. Implemented using the macro CACHE_STORAGE_DESC()
		virtual const uni_char *GetCacheDescription()=0;
#endif


		virtual const OpStringC FileName(OpFileFolder &folder, BOOL get_original_body = TRUE) const{OpStringC ret; return ret;}
		virtual OP_STATUS		FilePathName	(OpString &name, BOOL get_original_body = TRUE) const;

		URL_DataDescriptor *First(){ return (URL_DataDescriptor *) Head::First();};
		URL_DataDescriptor *Last(){ return (URL_DataDescriptor *) Head::Last();};

		virtual URL_DataDescriptor *GetDescriptor(MessageHandler *mh=NULL, BOOL get_raw_data = FALSE, BOOL get_decoded_data=TRUE, Window *window = NULL, URLContentType override_content_type = URL_UNDETERMINED_CONTENT, unsigned short override_charset_id = 0, BOOL get_original_content=FALSE, unsigned short parent_charset_id = 0);
		void			AddDescriptor(URL_DataDescriptor *desc);

		virtual OpFileLength ContentLoaded(BOOL force=FALSE){return content_size ? content_size : cache_content.GetLength();};
		virtual BOOL UsingMemory(){return TRUE;}
		virtual BOOL DataPresent(){return (cache_content.GetLength() > 0 ? TRUE : FALSE);}
		/** @return TRUE if this Cache_Storage has more data available. */
		virtual BOOL MoreData(){return cache_content.MoreData();}

		virtual void DecFileCount();

		BOOL	IsPersistent(); 
		//virtual BOOL	WriteCacheData(DataFile_Record *target) {return FALSE;}; 
		// The writing Object shall append its data with appropriate tags into target
		// Returns TRUE if the item is a permanent cache storage element
		// If so, the element should be deleted immediately.
#ifdef _OPERA_DEBUG_DOC_
		virtual void GetMemUsed(DebugUrlMemory &);
#endif
#ifdef _MIME_SUPPORT_
		virtual BOOL IsProcessed(){return FALSE;}
		/** Forwarded call from identical function in URL class */
		virtual BOOL GetAttachment(int i, URL &url);
		/** Forwarded call from identical function in URL class */
		virtual BOOL IsMHTML() const {return FALSE;}
		/** Forwarded call from identical function in URL class */
		virtual URL GetMHTMLRootPart(){OP_ASSERT(!"Don't call this when IsMHTML() returns FALSE");return URL();}
 		virtual void SetBigAttachmentIcons(BOOL isbig){};
		virtual void SetIgnoreWarnings(BOOL ignore){};
		virtual void SetPreferPlaintext(BOOL plain){};
#endif
		URL_Rep *Url(){return url;};

		virtual void SetMemoryOnlyStorage(BOOL flag);
		virtual BOOL GetMemoryOnlyStorage();

		OpStringS8 &GetContentEncoding(){return content_encoding;}
		virtual void TakeOverContentEncoding(OpStringS8 &enc){content_encoding.TakeOver(enc);}

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	void SetNeverFlush(){never_flush = TRUE;}
#endif

	virtual BOOL GetIsMultipart() const{return FALSE;}
	virtual void SetMultipartStatus(Multipart_Status status){}
	virtual Multipart_Status GetMultipartStatus(){return Multipart_NotAMultipart;}

#ifdef URL_ENABLE_ASSOCIATED_FILES
	virtual AssociatedFile *CreateAssociatedFile(URL::AssociatedFileType type);
	virtual AssociatedFile *OpenAssociatedFile(URL::AssociatedFileType type);

	virtual void SetPurgeAssociatedFiles(UINT32 types) {}

	/**
		Retrive the associated file name for the resource
		@param fname File name
		@param type Type of the associated file
		@param allow_storage_change if TRUE, embedded files and container are stored as plain files, flush Flush() is called. This should be avoided during startup, as in any case we would get problems...
	*/
	virtual OP_STATUS AssocFileName(OpString &fname, URL::AssociatedFileType type, OpFileFolder &folder_assoc, BOOL allow_storage_change) {fname.Empty(); return OpStatus::OK;}
#endif

#if CACHE_SMALL_FILES_SIZE>0
	/// TRUE if the file is so small that it could be embedded in the index
	inline BOOL IsEmbeddable()
	{
		return cache_content.GetLength()>0 &&
			   cache_content.GetLength()<=CACHE_SMALL_FILES_SIZE &&
			   (
					GetCacheType()==URL_CACHE_DISK
				#ifdef SELFTEST
					|| GetContextManager()->GetEmbedAllCacheTypes()  /* Used to embed HTTPS */
				#endif
			   ) && !url->GetAttribute(URL::KMultimedia);
	}
	/// TRUE if the file has been embedded
	inline BOOL IsEmbedded() const { return embedded; }
	/// Get the content of the file that has to be embedded
	OP_STATUS RetrieveEmbeddedContent(DiskCacheEntry *entry);
	/// Store the content of the file that has to be embedded in cache_content
	OP_STATUS StoreEmbeddedContent(DiskCacheEntry *entry);
#else
	/// TRUE if the file has been embedded;
	/// This function has been defined also when CACHE_SMALL_FILES_SIZE is disabled just to "clean" a bit the code
	BOOL IsEmbedded() const { return FALSE; }
	/// TRUE if the file is so small that it could be embedded in the index
	/// This function has been defined also when CACHE_SMALL_FILES_SIZE is disabled just to "clean" a bit the code
	BOOL IsEmbeddable() { return FALSE; }
	// Rollback the embedding, restoring the memory counted in the manger; use carefully... it is tought for associated files, that need this functionality
	/// This function has been defined also when CACHE_SMALL_FILES_SIZE is disabled just to "clean" a bit the code
	void RollBackEmbedding() { OP_ASSERT(!"NOT SUPPOSED TO BE USED! IsEmbedded() should be checked beforehand."); }
#endif

#ifdef SELFTEST
	/// Returns the number of time that a physical disk read has been performed to retrieve data; when this happen in memory, the number is not increased
	/// This is intended only to test the local files cache covered by LOCAL_STORAGE_CACHE_LIMIT
	UINT32 DebugGetNumberOfDiskRead() { return num_disk_read; }
#endif

#if (CACHE_SMALL_FILES_SIZE>0 || CACHE_CONTAINERS_ENTRIES>0)
    /// Set if the file must remain "plain": not embedded, not in a container (or in the future, not compressed, not encripted...)
    void SetPlainFile(BOOL plain) { plain_file=plain; }
    /// Get if the file is "plain": not embedded, not in a container (or in the future, not compressed, not encripted...)
    BOOL IsPlainFile() { return plain_file; }
#endif

#if CACHE_CONTAINERS_ENTRIES>0
	/// Return the ID used in the container; if 0, the file is not in a container
	UINT8 GetContainerID() { return container_id; }
	/// Set the Container ID (0 means that the file is not in a container)
	void SetContainerID(UINT8 id) { container_id=id; }
	/// Prevent the container file from being deleted, actually because it already was
	void PreventContainerFileDelete() { OP_ASSERT(!prevent_delete); prevent_delete=TRUE; }
	/**
		Store the content of the cache in a container.
		The container is supposed to have been already allocated witht he right size. In case of error,
		the calle is responsible for deleting the ID
		@param cont Container
		@param id ID
	*/
	OP_STATUS StoreInContainer(CacheContainer *cont, UINT id);
#else
	/// Return the ID used in the container; if 0, the file is not in a container
	/// This function has been defined also when CACHE_CONTAINERS_ENTRIES is 0 just to "clean" a bit the code
	UINT8 GetContainerID() { return 0; }
#endif // CACHE_CONTAINERS_ENTRIES>0

	/// Return the Context_Manager associated to the Cache storage. This is a temporary implementation
	Context_Manager *GetContextManager();

#ifdef CACHE_STATS
	// Return TRUE if the content has been retrieved from the cache
	BOOL RetrievedFromDisk() { return retrieved_from_disk; }
	// Return the number of calls to Flush()
	UINT32 GetFlushCalls() { return stats_flushed; }
#endif

	protected:
		/// Reset the DataStream object
		/// @return TRUE if some operation has been done, FALSE if nothin happened
		BOOL			FlushMemory(BOOL force = FALSE);
		BOOL			OOMForceFlush();

		virtual OpFileDescriptor* OpenFile(OpFileOpenMode mode, OP_STATUS &op_err, int flags = OPFILE_FLAGS_NONE);
		virtual OpFileDescriptor* CreateAndOpenFile(const OpStringC &filename, OpFileFolder folder, OpFileOpenMode mode, OP_STATUS &op_err, int flags = OPFILE_FLAGS_NONE);
		
#if !defined NO_SAVE_SUPPORT || !defined RAMCACHE_ONLY
		virtual DataStream_GenericFile*	OpenDataFile(const OpStringC &name, OpFileFolder folder,OpFileOpenMode mode, OP_STATUS &op_err, OpFileLength start_position=FILE_LENGTH_NONE, int flags = OPFILE_FLAGS_NONE);
#endif // !defined NO_SAVE_SUPPORT || !defined RAMCACHE_ONLY

#if !defined NO_SAVE_SUPPORT || !defined RAMCACHE_ONLY
		/** Copies content from source file (or memory) to target file (or memory) 
		 *
		 *	@param	source_name		Name of source file (in specified folder). If this
		 *							parameter is empty either the cache file is used or 
		 *							the memory storage is used (if source_is_memory is TRUE)
		 *	@param	source_folder	Folder where the source file resides (if specified)
		 *	@param	target_name		Name of target file (in specified folder). If this
		 *							parameter is empty either the cache file is used or 
		 *							the memory storage is used (if target_is_memory is TRUE)
		 *	@param	target_folder	Folder where the target file resides (if specified)
		 *	@param	source_is_memory	If this is TRUE, and source_name is empty, the memory 
		 *							storage is used as a contentsource
		 *	@param	target_is_memory	If this is TRUE, and target_name is empty, the memory 
		 *							storage is used as a target
		 *  @param  start_position  HTTP Range start during a range download. This is used by the Multimedia Cache
		 */
		OP_STATUS		CopyCacheFile(const OpStringC &source_name, OpFileFolder source_folder, 
										const OpStringC &target_name,  OpFileFolder target_folder, 
										BOOL source_is_memory=FALSE, BOOL target_is_memory=FALSE, OpFileLength start_position=FILE_LENGTH_NONE);
#endif
};

class Memory_Only_Storage
  : public Cache_Storage
{
	public:

		Memory_Only_Storage(URL_Rep *url_rep) : Cache_Storage(url_rep) {cache_content.SetIsSensitive(TRUE);};
		virtual URLCacheType	GetCacheType() const { return URL_CACHE_MEMORY;}
		virtual void GetCacheInfo(BOOL &streaming, BOOL &ram, BOOL &embedded_storage) { streaming=FALSE; ram=TRUE; embedded_storage=FALSE; }
		CACHE_STORAGE_DESC("MemoryOnly")

		/* This function should be overridden by all subclasses that ever need to be identified. */
		virtual BOOL IsA(Cache_StorageType type) { return type == STORAGE_TYPE_MEMORY || Cache_Storage::IsA(type); }
};

#if CACHE_SMALL_FILES_SIZE>0
// Memory cache that is saved in the index file; the files are in memory, but saved on the disk...
class Embedded_Files_Storage
  : public Cache_Storage
{
	private:
		URLCacheType cache_type;   // Cache Type; it is supposed to be DISK or TEMP
	public:

		Embedded_Files_Storage(URL_Rep *url_rep) : Cache_Storage(url_rep), cache_type(URL_CACHE_DISK) { };
		virtual URLCacheType	GetCacheType() const { OP_ASSERT(cache_type==URL_CACHE_DISK || cache_type==URL_CACHE_TEMP); return cache_type; }
		virtual void SetCacheType(URLCacheType ct)
		{
			if(ct==URL_CACHE_DISK || ct==URL_CACHE_TEMP)
				cache_type=ct;
		}
		virtual BOOL	Flush()
		{
			#ifdef CACHE_STATS
				stats_flushed++;
			#endif
			
			OP_ASSERT(embedded);
			
			return TRUE;
		}
		virtual BOOL UsingMemory(){ return FALSE; }
		virtual void GetCacheInfo(BOOL &streaming, BOOL &ram, BOOL &embedded_storage) { streaming=FALSE; ram=TRUE; embedded_storage=TRUE; }
		CACHE_STORAGE_DESC("Embedded_Files")

		/* This function should be overridden by all subclasses that ever need to be identified. */
		virtual BOOL IsA(Cache_StorageType type) { return type == STORAGE_TYPE_EMBEDDED || Cache_Storage::IsA(type); }
};

#endif

//#define SEEK_OPTIMIZATION

/// Class used to write on the disk and keep track of the operations done in the cache, to call SetFilePos() when appropriate
class CacheFile: public OpFile
{
private:
	// Possible values of the last operations
	enum LastOperation
	{
		// No operations
		OPERATION_NONE,
		// Last operation was a read
		OPERATION_READ,
		// Last operation was a write
		OPERATION_WRITE,
		// Last operation was a seek
		OPERATION_SEEK
	};
	
	// Last operation perferomed
	LastOperation last_operation;
#ifdef SEEK_OPTIMIZATION
	// Last position set
	OpFileLength last_pos;
#endif	
	OP_STATUS SyncOperation() { return OpFile::SetFilePos(0, SEEK_FROM_CURRENT); }
	
public:
	CacheFile()
	{
		last_operation=OPERATION_NONE;
		#ifdef SEEK_OPTIMIZATION
			last_pos=0; 
		#endif
	}

	virtual ~CacheFile() { Close(); }
	
	virtual OP_STATUS Read(void* data, OpFileLength len, OpFileLength* bytes_read=NULL)
	{
		if(last_operation==OPERATION_WRITE)
			RETURN_IF_ERROR(SyncOperation());
			
		last_operation=OPERATION_READ;
		
		OpFileLength bytes = 0;
		OP_STATUS ops=OpFile::Read(data, len, &bytes);
		
		#ifdef SEEK_OPTIMIZATION
			if(bytes>0)
				last_pos+=bytes;
		#endif
			
		if(bytes_read)
			*bytes_read=bytes;
			
		#ifdef SEEK_OPTIMIZATION
			#ifdef _DEBUG
				OpFileLength true_pos;
				
				OpStatus::Ignore(GetFilePos(true_pos));
				OP_ASSERT(true_pos==last_pos);
			#endif
		#endif
		
		return ops;
	}
	
	virtual OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START)
	{
		#ifdef SEEK_OPTIMIZATION
			OP_ASSERT(seek_mode==SEEK_FROM_START);
			
			if(last_operation==OPERATION_WRITE || last_pos!=pos || seek_mode!=SEEK_FROM_START)
			{
				last_operation=OPERATION_SEEK;
				
				OP_STATUS ops=OpFile::SetFilePos(pos, seek_mode);

				if(seek_mode==SEEK_FROM_START && OpStatus::IsSuccess(ops))
					last_pos=pos;
				else
					RETURN_IF_ERROR(GetFilePos(last_pos));
					
				return ops;
			}
			
			return OpStatus::OK;
		#else
			last_operation=OPERATION_SEEK;
		
			return OpFile::SetFilePos(pos, seek_mode);
		#endif
	}
	
	virtual OP_STATUS Write(const void* data, OpFileLength len)
	{
		if(last_operation==OPERATION_READ)
			RETURN_IF_ERROR(SyncOperation());
			
		last_operation=OPERATION_WRITE;
		
		return OpFile::Write(data, len);
	}
	
	virtual OP_STATUS Close() { last_operation=OPERATION_NONE; return OpFile::Close(); }
	virtual OP_STATUS ReadLine(OpString8& str) { last_operation=OPERATION_READ; return OpFile::ReadLine(str); } 
	OP_STATUS Flush() { last_operation=OPERATION_WRITE; return OpFile::Flush(); }
	virtual OP_STATUS SetFileLength(OpFileLength len) { last_operation=OPERATION_WRITE; return OpFile::SetFileLength(len); }
};

/**
Storage that save the content in a file
*/
class File_Storage
  : public Cache_Storage
{
	friend class Context_Manager_Disk;
	protected:
		/// Directory of the file
		OpFileFolder folder;
		/// File name
		OpStringS filename;
		/// Object that store the file. Don't expect this to be a normal OpFile.
		OpFileDescriptor *cache_file;
		/// Part of the content stored on disk and counted
		OpFileLength stored_size;

	private:
		struct fs_info_st
		{
			BYTE		dual:1;  /**< data are stored both in memory and on disk */
			BYTE		force_save:1;  /**< force store the data on disk */
			BYTE		keep_open_file:1;  /**< do not close the file when StoreData finishes, the reason for this is a bit unclear */

			BYTE		completed:1;  /**< data received are complete */
			BYTE		computed:1;  /**< TRUE if some data has been computed in the cache size (this could be true also for 0-byte URLs, and it's used to balance AddToCacheUsage() and SubToCacheUsage() */

			BYTE		memory_only_storage:1;  /**< data mustn't be saved to disk; it's quite strange to have File_Storage not saving to disk */

#ifdef SEARCH_ENGINE_CACHE
			BYTE		modified:1;  /**< filename is not empty, but the file doesn't contain valid data yet */
#endif
		}info;

		int				file_count;
		URLCacheType	cachetype;

#ifdef URL_ENABLE_ASSOCIATED_FILES
		UINT32          purge_assoc_files;

		/// Delete the associated files
		OP_STATUS PurgeAssociatedFiles(BOOL also_temporary);
		/// Delete only the temporary associated files
		OP_STATUS PurgeTemporaryAssociatedFiles();
#endif

	protected:
		/// Get the value of force_save
		BOOL GetForceSave() { return info.force_save; }
		/// Set the value of force_save
		void SetForceSave(BOOL b) { info.force_save=b; }
		/// Return an estimation of the content that can be retrieved
		virtual OpFileLength EstimateContentAvailable();
		/// Set the value of info.completed
		void SetInfoCompleted(BOOL b) { info.completed=b; }
		/// TRUE if the content destined to the Disk has been saved; it's meant to be used with IsDiskContentAvailable()
		virtual BOOL HasContentBeenSavedToDisk() { return info.completed; }
		/// TRUE if the content destined to the Disk is available; it's meant to be used with HasContentBeenSavedToDisk()
		virtual BOOL IsDiskContentAvailable();
		/** Return the additional content stored on disk but not counted by content_size
			This situation is typically associated with big files, where part of the file is saved, then the memory is flushed.
		*/
		virtual OpFileLength GetContentAlreadyStoredOnDisk() { return stored_size; }
		/** Set the amount of additional content stored on disk but not counted by content_size
			This situation is typically associated with big files, where part of the file is saved, then the memory is flushed.
		*/
		virtual void SetContentAlreadyStoredOnDisk(OpFileLength size) { stored_size = size; }
		/** TRUE if some content has really been computed in the disk cache size, which is important to not decrease the disk size, unbalancing the calls.
		    THe main problem is that the storage can become completed with a 0 bytes of content both if the URL has already been
			counted in the disk size or not.
			So we need a flag to decide if we have to subtract the dize (which will be bigger than 0, becasue fo the call to GetURLObjectSize()).
		*/
		virtual BOOL GetDiskContentComputed() { return info.computed; }
		/** Set if some content has really been computed in the disk cache size */
		virtual void SetDiskContentComputed(BOOL value) { info.computed = value; }

	public:

	                    File_Storage(Cache_Storage *source, URLCacheType ctyp = URL_CACHE_DISK, BOOL frc_save= FALSE, BOOL kp_open_file = FALSE);
	                    File_Storage(URL_Rep *url_rep, URLCacheType ctyp = URL_CACHE_DISK, BOOL frc_save = FALSE, BOOL kp_open_file= FALSE);

		virtual			~File_Storage();
		virtual void GetCacheInfo(BOOL &streaming, BOOL &ram, BOOL &embedded_storage) { streaming=FALSE; ram=FALSE; embedded_storage=FALSE; }
		CACHE_STORAGE_DESC("File")

		/* This function should be overridden by all subclasses that ever need to be identified. */
		virtual BOOL IsA(Cache_StorageType type) { return type == STORAGE_TYPE_FILE || Cache_Storage::IsA(type); }

/**
* Second phase constructor. You must call this method prior to using the 
* File_Storage object, unless it was created using the Create() method.
*
* @return OP_STATUS Status of the construction, always check this.
*/
	    OP_STATUS       Construct(Cache_Storage *source, const OpStringC &filename);
	
/**
* Second phase constructor. You must call this method prior to using the 
* File_Storage object, unless it was created using the Create() method.
*
* @return OP_STATUS Status of the construction, always check this.
*/
        OP_STATUS       Construct(const OpStringC &file_name, FileName_Store *filenames, OpFileFolder afolder = OPFILE_ABSOLUTE_FOLDER, OpFileLength file_len=0);

/**
* OOM safe creation of a File_Storage object.
*
* @return File_Storage* The created object if successful and NULL otherwise.
*/
     	static File_Storage* Create(Cache_Storage *source, const OpStringC &filename, URLCacheType ctyp = URL_CACHE_DISK, BOOL frc_save = FALSE, BOOL kp_open_file = FALSE);

	#ifdef CACHE_FAST_INDEX
		/// Get a reader able to retrieve the content of the file (both if it is in memory or on a file)
		/// This is a very particular function, that should be used only for SELFTESTS, because its behaviour is undefined
		/// in a lot of situations.
		/// The caller must delete the object
		/// WARNING: This method is mainly intended for testing. On Containers it can be quite slow...
		virtual SimpleStreamReader *CreateStreamReader();
	#endif

/**
* OOM safe creation of a File_Storage object.
*
* @return File_Storage* The created object if successful and NULL otherwise.
*/
		static File_Storage* Create(URL_Rep *url_rep, const OpStringC &file_name, FileName_Store *filenames, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER, URLCacheType ctyp = URL_CACHE_DISK, BOOL frc_save = FALSE, BOOL kp_open_file = FALSE);
		
		/** Create a read only OpFileDescriptor that the caller must delete when it has finished using it */
		virtual OpFileDescriptor* AccessReadOnly(OP_STATUS &op_err)
		{
		#if (CACHE_SMALL_FILES_SIZE>0 || CACHE_CONTAINERS_ENTRIES>0)
			if(embedded)
				return Cache_Storage::AccessReadOnly(op_err);
		#endif
			
			return OpenFile(OPFILE_READ, op_err); 
		}
		
		OP_STATUS SetFileName(const uni_char *file_name, OpFileFolder afolder)
		{
			#ifdef SEARCH_ENGINE_CACHE
				info.modified = TRUE;
			#endif
			folder = afolder;
			return filename.Set(file_name);
		}
		
		/*OP_STATUS SetFileName(const uni_char *file_name, OpFileFolder afolder)
		{
			info.modified = TRUE;
			folder = afolder;
			return filename.Set(file_name);
		}*/
		const uni_char *GetFileName(void) const {return filename.CStr();}
		OpFileFolder GetFolder(void) const {return folder;}

		virtual void	SetFinished(BOOL force = FALSE);
		virtual BOOL	GetFinished() { return info.completed; }
		virtual void	UnsetFinished();

		virtual OP_STATUS	StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position=FILE_LENGTH_NONE);
		virtual unsigned long RetrieveData(URL_DataDescriptor *desc,BOOL &more);

		BOOL			CopyFromFile(OpFile *inSrcFile);
		virtual void	CloseFile();

		virtual BOOL	Flush();
#ifdef CACHE_RESOURCE_USED
		virtual OpFileLength	ResourcesUsed(ResourceID resource);
#endif
		OpFileLength	FileLength();

#ifdef URL_ENABLE_ASSOCIATED_FILES
		virtual void SetPurgeAssociatedFiles(UINT32 types) {purge_assoc_files |= types;}

		virtual OP_STATUS AssocFileName(OpString &fname, URL::AssociatedFileType type, OpFileFolder &folder_assoc, BOOL allow_storage_change);
#endif

		virtual BOOL	Purge();
		virtual void	TruncateAndReset();
		virtual URLCacheType	GetCacheType() const { return cachetype;}
		virtual void SetCacheType(URLCacheType ct);
		virtual const OpStringC FileName(OpFileFolder &folder, BOOL get_original_body = TRUE) const;
		virtual OpFileLength ContentLoaded(BOOL force=FALSE);
		virtual BOOL UsingMemory(){return info.dual || filename.IsEmpty();}

		virtual void SetMemoryOnlyStorage(BOOL flag);
		virtual BOOL GetMemoryOnlyStorage();

		virtual void	ForceKeepOpen(){info.keep_open_file = TRUE;};
		void			ResetForLoading();

		virtual BOOL DataPresent();

		virtual void DecFileCount();
#ifdef _OPERA_DEBUG_DOC_
		virtual void GetMemUsed(DebugUrlMemory &);
#endif
	protected:
		virtual OP_STATUS CheckFilename();
};

class Download_Storage
  : public File_Storage
{
private:
	 friend class CacheTester;

	BOOL decided_decoding;
	BOOL need_to_decode_all;
	BOOL constructed;
	Cache_Storage *temp_storage;
	URL_DataDescriptor *decoder;
	
	void SetDescriptor(OP_STATUS &op_err);
	
protected:
	virtual BOOL CircularReferenceDetected(Cache_Storage *ptr) { return ptr==this || ptr==temp_storage; }

public:

	Download_Storage(Cache_Storage *storage);
	Download_Storage(URL_Rep *url);
	virtual ~Download_Storage();
	virtual void GetCacheInfo(BOOL &streaming, BOOL &ram, BOOL &embedded_storage) { streaming=FALSE; ram=FALSE; embedded_storage=FALSE; }
	CACHE_STORAGE_DESC("Download")

/**
* Second phase constructor. You must call this method prior to using the 
* Download_Storage object, unless it was created using the Create() method.
*
* @return OP_STATUS Status of the construction, always check this.
*/
	OP_STATUS       Construct(Cache_Storage *source, const OpStringC &target);
	
/**
* Second phase constructor. You must call this method prior to using the 
* Download_Storage object, unless it was created using the Create() method.
*
* @return OP_STATUS Status of the construction, always check this.
*/
	OP_STATUS       Construct(URL_Rep *url, const OpStringC &filename, BOOL finished=TRUE);

/**
* OOM safe creation of a Download_Storage object.
*
* @return Download_Storage* The created object if successful and NULL otherwise.
*/
    static Download_Storage* Create(Cache_Storage *source, const OpStringC &target);

/**
* OOM safe creation of a Download_Storage object.
*
* @return Download_Storage* The created object if successful and NULL otherwise.
*/
    static Download_Storage* Create(URL_Rep *url, const OpStringC &filename, BOOL finished=TRUE);

    virtual OP_STATUS	StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position=FILE_LENGTH_NONE);
	virtual void	SetFinished(BOOL force = FALSE);
	virtual void    TruncateAndReset();
};

class Local_File_Storage
  : public File_Storage
{
	int m_flags;

	public:
	                Local_File_Storage(URL_Rep *url_rep, int flags = OPFILE_FLAGS_NONE);

			~Local_File_Storage();
/**
* Second phase constructor. You must call this method prior to using the 
* Local_File_Storage object, unless it was created using the Create() method.
*
* @return OP_STATUS Status of the construction, always check this.
*/
	
	OP_STATUS Construct(const uni_char *filename, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER);

/**
* OOM safe creation of a Local_File_Storage object.
*
* @return Local_File_Storage* The created object if successful and NULL otherwise.
*/
	static Local_File_Storage* Create(URL_Rep *url_rep, const OpStringC &filename, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER, int flags = OPFILE_FLAGS_NONE);

	virtual OP_STATUS	StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position=FILE_LENGTH_NONE);
	virtual BOOL	Flush();

	virtual OpFileDescriptor* OpenFile(OpFileOpenMode mode, OP_STATUS &op_err, int flags = OPFILE_FLAGS_NONE);
	virtual OpFileDescriptor* CreateAndOpenFile(const OpStringC &filename, OpFileFolder folder, OpFileOpenMode mode, OP_STATUS &op_err, int flags = OPFILE_FLAGS_NONE);

	virtual void GetCacheInfo(BOOL &streaming, BOOL &ram, BOOL &embedded_storage) { streaming=FALSE; ram=FALSE; embedded_storage=FALSE; }
	CACHE_STORAGE_DESC("Local_File")
		
#if !defined NO_SAVE_SUPPORT || !defined RAMCACHE_ONLY
	virtual DataStream_GenericFile*	OpenDataFile(const OpStringC &name, OpFileFolder folder,OpFileOpenMode mode, OP_STATUS &op_err, OpFileLength start_position=FILE_LENGTH_NONE, int flags = OPFILE_FLAGS_NONE);
#endif // !defined NO_SAVE_SUPPORT || !defined RAMCACHE_ONLY
	virtual unsigned long RetrieveData(URL_DataDescriptor *desc,BOOL &more);
#if LOCAL_STORAGE_CACHE_LIMIT>0
	virtual OpFileLength ContentLoaded(BOOL force =FALSE);

	private:
	unsigned long RetrieveDataAndCache(URL_DataDescriptor *desc,BOOL &more,BOOL cache_file_content);

	/// Buffer used to store the file in memory
	char unsigned *buffer;
	/// Length of the buffer
	unsigned int buffer_len;
	/// Time of the file when it has been cached (to check if it has changed)
	time_t time_cached;
#endif
};

#ifndef RAMCACHE_ONLY
class CacheFile_Storage
  : public File_Storage
{
private:
public:
	CacheFile_Storage(URL_Rep *url_rep, URLCacheType ctyp = URL_CACHE_DISK);
	
	/**
	* Second phase constructor. You must call this method prior to using the 
	* CacheFile_Storage object, unless it was created using the Create() method.
	*
	* @return OP_STATUS Status of the construction, always check this.
	*/
	OP_STATUS Construct(URL_Rep *url_rep, const OpStringC &filename=NULL);
	
	/**
	* Second phase constructor. You must call this method prior to using the 
	* CacheFile_Storage object, unless it was created using the Create() method.
	*
	* @return OP_STATUS Status of the construction, always check this.
	*/
#ifndef SEARCH_ENGINE_CACHE
	OP_STATUS Construct(FileName_Store &filenames, OpFileFolder folder, const OpStringC &filename=NULL, OpFileLength file_len=0);
#else
	OP_STATUS Construct(OpFileFolder folder, const OpStringC &filename=NULL);
#endif
	
	/**
	* OOM safe creation of a CacheFile_Storage object.
	*
	* @return CacheFile_Storage* The created object if successful and NULL otherwise.
	*/
	static CacheFile_Storage* Create(URL_Rep *url_rep, const OpStringC &filename=NULL, URLCacheType ctyp = URL_CACHE_DISK);
	
	/**
	* OOM safe creation of a CacheFile_Storage object.
	*
	* @return CacheFile_Storage* The created object if successful and NULL otherwise.
	*/
#ifndef SEARCH_ENGINE_CACHE
	static CacheFile_Storage* Create(URL_Rep *url_rep, FileName_Store &filenames, OpFileFolder folder, const OpStringC &filename=NULL, URLCacheType ctyp = URL_CACHE_DISK);
#else
	static CacheFile_Storage* Create(URL_Rep *url_rep, OpFileFolder folder, const OpStringC &filename=NULL, URLCacheType ctyp = URL_CACHE_DISK);
#endif
	
#ifdef CACHE_RESOURCE_USED
	virtual OpFileLength ResourcesUsed(ResourceID resource);
#endif
	
	//virtual BOOL	CacheFile() { return TRUE; }  // true if this is a part of the cache ; Default FALSE
	CACHE_STORAGE_DESC("CacheFile")
};


class Session_Only_Storage
: public CacheFile_Storage
{
public:
	
	Session_Only_Storage(URL_Rep *url_rep);
	
	/**
	* Second phase constructor. You must call this method prior to using the 
	* Session_Only_Storage object, unless it was created using the Create() method.
	*
	* @return OP_STATUS Status of the construction, always check this.
	*/
	OP_STATUS Construct(URL_Rep *url_rep);
	
	/**
	* OOM safe creation of a Session_Only_Storage object.
	*
	* @return Session_Only_Storage* The created object if successful and NULL otherwise.
	*/
	static Session_Only_Storage* Create(URL_Rep *url_rep);
	
#ifdef CACHE_RESOURCE_USED
	virtual OpFileLength ResourcesUsed(ResourceID resource);
#endif
	//virtual BOOL	Persistent() { return FALSE; } // Only used by cache files. True if it is to be preserved for the next session; DEFAULT TRUE

	CACHE_STORAGE_DESC("Session_Only")
};

class Persistent_Storage
  : public CacheFile_Storage
{
public:
	Persistent_Storage(URL_Rep *url_rep);
	virtual ~Persistent_Storage();
	
	/**
	* Second phase constructor. You must call this method prior to using the 
	* Persistent_Storage object, unless it was created using the Create() method.
	*
	* @return OP_STATUS Status of the construction, always check this.
	*/
	OP_STATUS Construct(const OpStringC &filename=NULL);
	
	/**
	* Second phase constructor. You must call this method prior to using the 
	* Persistent_Storage object, unless it was created using the Create() method.
	*
	* @return OP_STATUS Status of the construction, always check this.
	*/
#ifndef SEARCH_ENGINE_CACHE
	OP_STATUS Construct(FileName_Store &filenames, OpFileFolder folder, const OpStringC &filename=NULL, OpFileLength file_len=0);
#else
	OP_STATUS Construct(OpFileFolder folder, const OpStringC &filename=NULL);
#endif
	
	/**
	* OOM safe creation of a Persistent_Storage object.
	*
	* @return Persistent_Storage* The created object if successful and NULL otherwise.
	*/
	static Persistent_Storage* Create(URL_Rep *url_rep,const OpStringC &filename=NULL);
	
	/**
	* OOM safe creation of a Persistent_Storage object.
	*
	* @return Persistent_Storage* The created object if successful and NULL otherwise.
	*/
#ifndef SEARCH_ENGINE_CACHE
	static Persistent_Storage* Create(URL_Rep *url_rep, FileName_Store &filenames, OpFileFolder folder, const OpStringC &filename=NULL, OpFileLength file_len=0);
#else
	static Persistent_Storage* Create(URL_Rep *url_rep, OpFileFolder folder, const OpStringC &filename=NULL);
#endif
	
	
#ifdef CACHE_RESOURCE_USED
	virtual OpFileLength ResourcesUsed(ResourceID resource);
#endif
	//virtual BOOL	WriteCacheData(DataFile_Record *target); 
	// Returns TRUE if the item is a permanent cache storage element
	// If so, the element should be deleted immediately.

	CACHE_STORAGE_DESC("Persistent")
};

class MultimediaSegment;
class MultimediaCacheFile;
class Multimedia_Storage;

#ifdef CACHE_URL_RANGE_INTEGRATION
/// Class used to load a Multimedia file avoiding to download a part of the file twice
class MultimediaLoader: public MessageHandler
{
private:
	/// Requested range start
	OpFileLength start;
	/// Requested range end
	OpFileLength end;
	/// Current position
	//OpFileLength pos;
	/*====== Parameters used for calling LoadDocument() =====*/
	/// URL that will load the document
	URL load_url;
	/// Message handler that will receive the simulated messages
	MessageHandler* load_mh;
	/// Referer
	URL load_referer_url;
	/// Policy
	URL_LoadPolicy load_policy;

public:
	/// Default constructor
	MultimediaLoader(): MessageHandler(NULL), start(0), end(0), load_mh(NULL) { }

	/// Start the loading process
	CommState StartLoadDocument(OpFileLength range_start, OpFileLength range_end, const URL_Rep *rep, MessageHandler* mh, const URL& referer_url, const URL_LoadPolicy &policy);
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
};
#endif// CACHE_URL_RANGE_INTEGRATION

#ifdef MULTIMEDIA_CACHE_SUPPORT
/**
	Storage optimized for Multimedia content, for the Audio / Video tag.
	The low-level implementation is provided by the class MultimediaCacheFile
	
	Reference task: CORE-17129.
	
*/
class Multimedia_Storage
  : public Persistent_Storage
{
private:
	/// Low level component that manage the Multimedia Cache
	MultimediaCacheFile *mcf;
#ifdef CACHE_URL_RANGE_INTEGRATION
	/// Manage the loading of a URL
	MultimediaLoader loader;
	/// Used to avoid recursion
	BOOL enable_loader;
#endif// CACHE_URL_RANGE_INTEGRATION

	/// Convert an array of MultimediaSegment in an array of StorageSegment
	/// If present, the elements of out_segments will be recycled, empty spots will be filled and exceeding segments deleted
	OP_STATUS ConvertSegments(OpAutoVector<StorageSegment> &out_segments, OpAutoVector<MultimediaSegment> &in_segments);
	
protected:
	virtual OpFileDescriptor* CreateAndOpenFile(const OpStringC &filename, OpFileFolder folder, OpFileOpenMode mode, OP_STATUS &op_err, int flags=OPFILE_FLAGS_NONE);
	virtual OP_STATUS SetWritePosition(OpFileDescriptor *file, OpFileLength start_position);
	virtual OP_STATUS CheckFilename();
	virtual OpFileLength EstimateContentAvailable();
	
public:
	Multimedia_Storage(URL_Rep *url_rep);
	virtual ~Multimedia_Storage();
	/**
	* Second phase constructor. You must call this method prior to using the 
	* Multimedia_Storage object, unless it was created using the Create() method. filename must already have been set
	* @param afilename file name
	* @param force_ram_stream TRUE to force streaming in RAM. This bypass all the other settings
	*
	* @return OP_STATUS Status of the construction, always check this.
	*/
	OP_STATUS Construct(const OpStringC &afilename, BOOL force_ram_stream=FALSE);
	
	/**
	* Second phase constructor. You must call this method prior to using the 
	* Multimedia_Storage object, unless it was created using the Create() method.
	*
	* @return OP_STATUS Status of the construction, always check this.
	*/
	OP_STATUS Construct(FileName_Store &filenames, OpFileFolder folder, const OpStringC &filename=NULL, OpFileLength file_len=0);

	/// Returns TRUE if streaming is required for this URL
	static BOOL IsStreamRequired(URL_Rep *rep, BOOL &ram_stream);
	
	/**
	* OOM safe creation of a Persistent_Storage object.
	*
	* @return Persistent_Storage* The created object if successful and NULL otherwise.
	*/
	static Multimedia_Storage* Create(URL_Rep *url_rep,const OpStringC &filename=NULL, BOOL force_ram_stream=FALSE);
	
	/**
	* OOM safe creation of a Persistent_Storage object.
	*
	* @return Persistent_Storage* The created object if successful and NULL otherwise.
	*/
	static Multimedia_Storage* Create(URL_Rep *url_rep, FileName_Store &filenames, OpFileFolder folder, const OpStringC &filename=NULL, OpFileLength file_len=0);
	
	virtual OP_STATUS GetUnsortedCoverage(OpAutoVector<StorageSegment> &out_segments, OpFileLength start=0, OpFileLength len=INT_MAX);
	virtual OP_STATUS GetSortedCoverage(OpAutoVector<StorageSegment> &out_segments, OpFileLength start=0, OpFileLength len=INT_MAX, BOOL merge=TRUE);
	virtual OP_STATUS GetMissingCoverage(OpAutoVector<StorageSegment> &missing, OpFileLength start=0, OpFileLength len=FILE_LENGTH_NONE);
	virtual void GetPartialCoverage(OpFileLength position, BOOL &available, OpFileLength &length, BOOL multiple_segments);
#ifdef CACHE_URL_RANGE_INTEGRATION
	virtual BOOL CustomizeLoadDocument(MessageHandler* mh, const URL& referer_url, const URL_LoadPolicy &loadpolicy, CommState &comm_state);
#endif// CACHE_URL_RANGE_INTEGRATION

	OpFileLength GetCacheSize() { return (mcf) ? mcf->GetMaxSize() : FILE_LENGTH_NONE; }
	virtual void GetCacheInfo(BOOL &streaming, BOOL &ram, BOOL &embedded_storage);
	CACHE_STORAGE_DESC("Multimedia")

	virtual BOOL Flush();

	// For Multimedia, the file should not be deleted
	virtual void SetFinished(BOOL force);

		virtual BOOL IsExportAllowed();
};
#endif // MULTIMEDIA_CACHE_SUPPORT

#endif // !RAMCACHE_ONLY

/** Cache storage to temporarily store data for plugins, only used for multiparts 
 * There is only a single, preallocated Data descriptor, all data is inserted directly into the descriptor
 * Maximum limit on buffer is Document RAM cache size (but minimum 256KB)
 * Loading will be stopped if the size of the buffer exceeds this size
 */
class StreamCache_Storage : public Cache_Storage
{
private:
	URL_DataDescriptor *local_desc; // Temporary storage
	BOOL descriptor_created;
	BOOL expecting_subdescriptor;
	unsigned long max_buffersize;
	unsigned long content_loaded;

	OP_STATUS AddDataFromCacheStorage(Cache_Storage * newdata);


public:
	StreamCache_Storage(URL_Rep *);
	virtual ~StreamCache_Storage();

	/* This function should be overridden by all subclasses that ever need to be identified. */
	virtual BOOL IsA(Cache_StorageType type) { return type == STORAGE_TYPE_STREAM || Cache_Storage::IsA(type); }

    void ConstructL(Cache_Storage * initial_storage, OpStringS8 &content_encoding);

	virtual OP_STATUS	StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position=FILE_LENGTH_NONE);
	virtual unsigned long RetrieveData(URL_DataDescriptor *desc,BOOL &more);
	virtual URL_DataDescriptor *GetDescriptor(MessageHandler *mh=NULL, BOOL get_raw_data = FALSE, BOOL get_decoded_data=TRUE, Window *window = NULL, URLContentType override_content_type = URL_UNDETERMINED_CONTENT, unsigned short override_charset_id=0, BOOL get_original_content=FALSE, unsigned short parent_charset_id=0);
	virtual OpFileLength ContentLoaded(BOOL force=FALSE) { return content_loaded; };
	virtual void GetCacheInfo(BOOL &streaming, BOOL &ram, BOOL &embedded_storage) { streaming=TRUE; ram=TRUE; embedded_storage=FALSE; }
	CACHE_STORAGE_DESC("Stream")
	virtual URLCacheType	GetCacheType() const { return URL_CACHE_STREAM;}

};

#endif // !URL_CS_H_
