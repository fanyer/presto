/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef __FAVICON_MANAGER_H__
#define __FAVICON_MANAGER_H__

#include "modules/hardcore/timer/optimer.h"
#include "modules/url/url_man.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/adt/oplisteners.h"
#include "adjunct/desktop_util/datastructures/StreamBuffer.h"
#include "adjunct/desktop_util/image_utils/fileimagecontentprovider.h"
#include "adjunct/quick/managers/DesktopManager.h"

/********************************************************************************
 *
 * FavIconManager and helper classes
 *
 * g_favicon_manager->Add called from Sync and OpBrowserview::SaveDocumentIcon
 * (the last from DocumentDesktopWindow::UpdateWindowImage (OnDocumentIconAdded/
 *                OnStart/FinishLoading) adds/writes icons.
 *
 *
 *
 ***************************************************************************/

#define g_favicon_manager (FavIconManager::GetInstance())

/******************************************************************
 *
 * class FileImage
 *
 * Holds string url and corresponding ImageEntry (Filename and Image)
 *
 * FileImages are stored in the m_icon_table of FavIconManager,
 * which maps document_url to FileImage
 *
 * FaviconManager.cpp m_image_table holds the ImageEntries
 * (mapping filename of ImageEntry -> ImageEntry)
 *
 ******************************************************************/

class FileImage
{
public:
	struct ImageEntry
	{
		ImageEntry()
		{
			num_users = 0;
		}

		~ImageEntry()
		{
			image.DecVisible(null_image_listener);
		}

		OpString filename;
		int num_users;
		Image image;
		FileImageContentProvider m_image_content_provider;
	};

public:
	FileImage(const uni_char* filename, const uni_char* url);
	~FileImage();

	void ReloadImage( const uni_char* filename );

	Image GetImage() const { return m_entry ? m_entry->image : m_empty_image; }
	OpString& GetFilename() { return m_entry ? m_entry->filename : m_empty_string; }

	const uni_char* GetURL() {return m_url.CStr();}

	static BOOL IsICOFileFormat(const uni_char* filename);
	static BOOL CheckICOHeader(UINT8* data);

private:
	static Image m_empty_image;
	static OpString m_empty_string;
	OpString m_url;
	ImageEntry* m_entry;
};



/*************************************************************************
 *
 * class FavIconLoader
 *
 *
 *
 *************************************************************************/

class FavIconLoader : public MessageObject
{
public:
	class Listener
	{
	public:
		virtual ~Listener() {}
		virtual void OnFavIconLoaded(const uni_char* document_url, const uni_char* image_path, URL_CONTEXT_ID id) = 0;
		virtual void OnFavIconFinished() = 0;
	};

public:
	FavIconLoader();
	~FavIconLoader();

	/**
	 * Requests the specified favicon and associates it with the document url
	 *
	 * @param document_url The document url associated with the favicon
	 * @param favicon_url Full url to the favicon
	 * @param id Specifies where the icon will be cached. 0 means Opera's normal cache.
	 *           Note that we use our own cache as well, but this "outer" cache will reduce
	 *           network traffic
	 *
	 * @return TRUE if a request has been made, otherwise FALSE
	 */
	BOOL Load(const OpString& document_url, const OpString& favicon_url, URL_CONTEXT_ID id);

	/**
	 * Returns url to document that requests download
	 */
	const OpString& DocumentUrl() const { return m_document_url; }

	/**
	 * Returns url to favicon that is downloaded
	 */
	const OpString& FaviconUrl() const { return m_favicon_url; }

	/**
	 * Returns TRUE if object is downloading an icon
	 */
	BOOL IsLoading();

	/**
	 * Adds a listener.
	 *
	 * @param listener listener to be added
	 *
	 * @return OpStatus::OK if the add was successful
	 */
	OP_STATUS AddListener(Listener* listener) { return m_listeners.Add(listener); }

	/**
	 * Removes a listener.
	 *
	 * @param listener listener to be removed
	 *
	 * @return OpStatus::OK if the removal was successful
	 */
	OP_STATUS RemoveListener(Listener* listener) { return m_listeners.Remove(listener); }

	/**
	 * For callback handling when loading url loading
	 */
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	OpListeners<Listener> m_listeners;
	OpString m_document_url;
	OpString m_favicon_url;
	URL m_url;
	URL_CONTEXT_ID m_id;
	BOOL m_finished;
};

class FavIconProvider
{
public:
	virtual ~FavIconProvider() {}

	/**
	 * Returns the fav icon image that is associated with the document. The image
	 * is empty if there is no such icon.
	 *
	 * @param document_url The document url that requests the icon
	 * @param allow_near_match If TRUE, the filename will be the first in the index if it
	 *        does not match anything else. To be used carefully.
	 *
	 * @return An Image holding the icon. The image can be empty
	 */
	virtual Image Get(const uni_char* document_url, BOOL allow_near_match = FALSE) = 0;
};



/*************************************************************************
 *
 * class FavIconManager
 *
 *
 *
 *************************************************************************/

class FavIconManager
	: public DesktopManager<FavIconManager>,
	  public FavIconLoader::Listener,
	  public OpTimerListener,
	  public FavIconProvider
{

public:
	class FavIconListener
	{
	public:
		virtual ~FavIconListener() {}
		virtual void OnFavIconAdded(const uni_char* document_url, const uni_char* image_path) = 0;
		virtual void OnFavIconsRemoved() = 0;
	};

private:
	struct DocAndFavIcon
	{
		OpString url;
		OpString icon_url;
	};

public:
	FavIconManager();
	~FavIconManager();
	virtual OP_STATUS Init();

	/**
	 * Loads icons for package customizations (search engines, bookmarks, etc.) into cache if necessary.
	 * Should be called after package customizations are loaded (#OpBootManager::LoadCustomizations).
	 */
	OP_STATUS InitSpecialIcons();

	/**
	 * Saves a fav icon image to the OPFILE_ICON_FOLDER directory and updates
	 * the index file.
	 *
	 * @param document_url The document url that requested the icon
	 * @param icon_url Address to location where icon is located
	 * @param on_demand Set to TRUE when downloading from favicon loader
	 */
	void Add(const uni_char* document_url, const uni_char* icon_url, BOOL on_demand = FALSE, URL_CONTEXT_ID id = 0);

	/**
	* Saves a fav icon image data to the OPFILE_ICON_FOLDER directory and updates
	* the index file.
	*
	* @param document_url The document url that requested the icon
	* @param icon_data Fav icon data
	* @param len Length of data
	*/
	void Add(const uni_char* document_url, const unsigned char* icon_data, UINT32 len);

	/**
	 * Returns the fav icon image that is associated with the document. The image
	 * is empty if there is no such icon.
	 *
	 * @param document_url The document url that requests the icon
	 * @param allow_near_match If TRUE, the filename will be the first in the index if it
	 *        does not match anything else. To be used carefully.
	 *
	 * @return An Image holding the icon. The image can be empty
	 */
	Image Get(const uni_char* document_url, BOOL allow_near_match = FALSE) { return LookupImage(document_url, FALSE, allow_near_match); }

	/**
	 * Attempts to load special icons from the net if they are noe already saved in cache
	 * Loads default search and bookmark icons
	 *
	 */
	void LoadSpecialIcons();

	/**
	 * Requests favicon associated with the document url. If the icon url
	 * is NULL we will attempt to load an icon from the list of predefined
	 * bookmarks.
	 *
	 * @document_url The document url that requested the icon
	 * @icon_url icon to load if any
	 */
	void LoadBookmarkIcon(const uni_char* document_url, const uni_char* icon_url);

	/**
	 * Attempts to load icon for search with guid search_guid
	 *
	 */
	void LoadSearchIcon(const OpStringC& search_guid);

	/**
	 * Returns the fav icon image file name that best matches the document url
	 *
	 * @param document_url The document url that requested the icon
	 * @param image_filename On return: Filename with best match. Can be empty
	 * @param full_path If TRUE, the returned 'image_filename' will contain a full path
	 * @param allow_near_match If TRUE, the filename will be the first in the index if it
	 *        does not match anything else
	 */
	void GetImageFilename(const uni_char* document_url, OpString& image_filename, BOOL full_path, BOOL allow_near_match);

	/**
	 * Clears internal data structures and removes image file. Index file is removed if
	 * empty after the image reference has been removed.
	 *
	 * @param document_url The document url that requested the icon
	 * @param image_filename Name of file as stored in index file
	 */
	void EraseDataFile(const uni_char* document_url, const uni_char *image_filename);

	/**
	 * Clears internal data structures and removes all images and index files
	 */
	void EraseDataFiles();

	/**
	 * Create icon filename from the icon url
	 *
	 * @param icon_url Icon url to make the filename out of
	 * @param extension Forced extension appended to filename. Can be NULL.
	 * @param filename Constructed filename is saved here
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR on empty filename, OpStatus::ERR_NULL_POINTER
	 *         on illegal argument or OpStatus::ERR_NO_MEMORY on insufficient memory available
	 */
	OP_STATUS MakeImageFilename(const uni_char* icon_url, const char* extension, OpString& filename);

	/**
	 * Creates an index filename from the url
	 *
	 * @param icon_url The icon url to make the filename out of
	 * @param filename Constructed filename is saved here
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR on empty filename or
	 *         OpStatus::ERR_NO_MEMORY on insufficient memory available
	 */
	OP_STATUS MakeIndexFilename(const uni_char* document_url, OpString& filename);

	/**
	 * Adds a listener.
	 *
	 * @param listener listener to be added
	 *
	 * @return OpStatus::OK if the add was successful
	 */
	OP_STATUS AddListener(FavIconListener* listener) { return m_listeners.Add(listener); }

	/**
	 * Removes a listener.
	 *
	 * @param listener listener to be removed
	 *
	 * @return OpStatus::OK if the removal was successful
	 */
	OP_STATUS RemoveListener(FavIconListener* listener) { return m_listeners.Remove(listener); }
#ifdef SELFTEST
public:
#else
private:
#endif
	/**
	 * Loads the predefined bookmark list if any
	 *
	 * @return The list if found, otherwise NULL
	 */
	OpVector<DocAndFavIcon>* LoadBookmarkList();

	/**
	 * Adds an item to the list of icons that shall never be deleted
	 *
	 * @param document_url Page url
	 * @param escaped_icon_filename Escaped version of icon filename
	 *
	 * @return OpStatus::OK if all went fine, otherwise an error
	 */
	OP_STATUS AddPersistentItem(const uni_char* document_url, const uni_char* escaped_icon_filename);

	/**
	 * Returns the list of icons that shall never be deleted
	 */
	OpVector<OpString8>* GetPersistentList();

	/**
	 * Returns true if the file is specified in the list of icons that shall never be delected
	 */
	BOOL IsPersistentFile(const uni_char* filename);

	/**
	 * Makes new index files for all items in persistent list
	 */
	void RecreatePersistentIndex();


	BOOL CreateIconTable();

	/**
	 * Returns the url context to be used for the particual favicon when downloading
	 * it from the net. The context is associated with a separate opera cache if not 0
	 */
	URL_CONTEXT_ID GetURLContext(const OpString& favicon_url);

	/**
	 * Requests the fav icon using the favicon url and associates it with the
	 * applied document url
	 *
	 * @param document_url The document url associated with the favicon
	 * @param favicon_url Full url to the favicon
	 *
	 * @return TRUE if image was present or a download was started
	 */
	BOOL GetInternal(const OpString& document_url, const OpString& favicon_url);

	/**
	 * Behaves like @ref Get but updates the internal cache as well.
	 *
	 * @param document_url The document url that requests the icon
	 *
	 * @return An Image holding the icon. The image can be empty
	 */
	Image UpdateCache(const uni_char* document_url) { return LookupImage(document_url, TRUE, FALSE); }

	/**
	 * Returns the fav icon that is associated with the document. The image
	 * is empty if there is no such icon.
	 *
	 * @param document_url The document url that requests the icon
	 * @param update_cache Forces a cache update
	 * @param allow_near_match If TRUE, the image will represent the first entry in the
	 *        index for the 'document_url' if there were no exact matches.
	 *
	 * @return An Image holding the icon. The image can be empty
	 */
	Image LookupImage(const uni_char* document_url, BOOL update_cache, BOOL allow_near_match);

	/**
	 * Reads all entries in the index file and return them as a list.
	 *
	 * @param document_url The document url is used to form the filename to open.
	 *
	 * @return The index as a vector
	 */
	OpVector<OpString8>* ReadIndex(const uni_char* document_url);

	/**
	 * Reads all entries in the index file and return them as a list.
	 *
	 * @param document_url The document url is used to form the filename to open.
	 */
	void ReadIndex(const uni_char* document_url, OpVector<OpString8>& list);

	/**
	 * Adds icon filename to index file associated with the document if it
	 * not already listed in the file
	 *
	 * @param document_url The document url
	 * @param icon_filename The filename to add to the index file.
	 * @param saved_data_changed Set to TRUE if an image and/or index was modified, otherwise FALSE
	 *
	 * @return TRUE if the path was added, otherwise FALSE.
	 */
	BOOL AppendToIndex(const uni_char* document_url, const OpString& icon_filename, BOOL& saved_data_changed);

	/**
	 * Replace all references to 'old_icon_filename' with 'new_icon_filename' in the
	 * given icon index.
	 *
	 * @param document_url Url that select index to examine
	 * @param old_icon_filename Filename that will be removed
	 * @param new_icon_filename Filename that will replace old filename
	 *
	 * @return OpStatus::Ok on success, otherwise OpStatus::ERR or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS ReplaceIndex(const uni_char* document_url, const OpString& old_icon_filename, const OpString& new_icon_filename);

	/**
	 * Write index list to file. The filename is constructed using the document_url.
	 * If a file with the same name already exists it will be replaced.
	 *
	 * @param document_url The document url
	 * @param list List of strings to write to the file
	 *
	 * @return OpStatus::Ok on success, otherwise OpStatus::ERR or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS WriteIndexToFile(const uni_char* document_url, OpVector<OpString8>& list);

	/**
	 * Called by favicon downloader when an icon has been downloaded.
	 *
	 * @param document_url The document url that requested the icon
	 * @param icon_url Address to location where icon is located
	 */
	void OnFavIconLoaded(const uni_char* document_url, const uni_char* image_path, URL_CONTEXT_ID id) { Add(document_url,image_path, TRUE, id); }

	/**
	 * Called by favicon downloader when download is complete or has failed
	 */
	void OnFavIconFinished();

	/**
	 * Cleanup routine. Deletes idle download objects
	 */
	void OnTimeOut(OpTimer* timer);

	void LoadSearchIcon(class SearchTemplate* search);

	/**
	 * Determine servername from url.
	 *
	 * Purpose is to prevent using urlManager->GetURL() because that will
	 * update access times [espen 2007-09-19]
	 *
	 * @param url Url string to examine
	 * @param server_name On return, the server name if any
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY
	 *         Note: OpStatus::OK can be returned for an empty string so
	 *         string size should always be examined
	 */
	OP_STATUS GetServerName(const uni_char* url, OpString& server_name);

	/**
	 * Save URL content to buffer.
	 *
	 * @param url URL to save
	 * @param buffer On return, the content of the URL
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on insufficient
	 *         memory available or OpStatus::ERR_NULL_POINTER on internal error
	 */
	OP_STATUS UrlToBuffer(URL& url, StreamBuffer<UINT8>& buffer);

	/**
	 * Save URL content to buffer as PNG image with size 16x16. The URL must refer
	 * to an image.
	 *
	 * @param url Url to save
	 * @param buffer On return, the PNG content of the url
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on insufficient
	 *         memory available or OpStatus::ERR on a decoding error
	 */
	OP_STATUS UrlToPNGBuffer(URL url, StreamBuffer<UINT8>& buffer);

	/**
	 * Save file content to buffer.
	 *
	 * @param filename File to save. The name is relative to OPFILE_ICON_FOLDER
	 * @param buffer On return, the content of the file
	 *
	 * @return OpStatus::OK on success or OpStatus::ERR_NO_MEMORY on insufficient memory available
	 */
	OP_STATUS FileToBuffer(const OpString& filename, StreamBuffer<UINT8>& buffer);

	/**
	 * Save file content to buffer as PNG image with size 16x16. The file must refer
	 * to an image.
	 *
	 * @param filename File to save. The name is relative to OPFILE_ICON_FOLDER
	 * @param buffer On return, the content of the file
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on insufficient
	 *         memory available or OpStatus::ERR on a decoding error
	 */
	OP_STATUS FileToPNGBuffer(const OpString& filename, StreamBuffer<UINT8>& buffer);

	/**
	 * Convert incoming buffer to a PNG buffer image with size 16x16.
	 *
	 * @param src Buffer to convert
	 * @param buffer On return, the converted content
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on insufficient
	 *         memory available or OpStatus::ERR on a decoding error
	 *
	 */
	OP_STATUS BufferToPNGBuffer(StreamBuffer<UINT8>& src, StreamBuffer<UINT8>& buffer);

	/**
	 * Write buffer to file. An existing file is overwritten
	 *
	 * @param buffer buffer to write
	 * @param filename Filename of new or existing file. The name is relative to OPFILE_ICON_FOLDER
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on insufficient
	 *         memory available an error code from @ref OpFile::Write
	 */
	static OP_STATUS BufferToFile(const StreamBuffer<UINT8>& buffer, const OpString& filename);

	/**
	 * Change file extension to ".png"
	 *
	 * @param filename The filename to examine
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS ReplaceExtensionToPNG(OpString& filename);

	/**
	 * Returns TRUE if filename ends with ".png" (case insensitive)
	 *
	 * @param filename The filename to examine
	 *
	 * @return TRUE on a match, otherwise FALSE
	 */
	BOOL IsPNGFile(const OpString& filename);

	/**
	 * Return value of given ini file entry. The entry format is expected
	 * to be KEY=VALUE, but the caller must append a '=' to the 'key' argument
	 * to get a successful match.
	 *
	 * @param line Ini file entry line
	 * @param key Key to match
	 * @param value On return the value of a matched key is returned here
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR on no match, or
	 *         OpStatus::ERR_NO_MEMORY on insufficient memory available
	 */
	OP_STATUS GetIniFileValue(const OpString8& line, const char* key, OpString8& value)
	{
		return line.Find(key) == 0 ? value.Set(line.SubString(strlen(key))) : OpStatus::ERR;
	}

private:

	BOOL m_disable_get_set; // For debug purposes.

    // m_icon_table maps document_url to FileImage
	OpAutoStringHashTable<FileImage>* m_icon_table;

	// m_index_cache holds <index_filename, list of entries in index with index_filename> - in-memory repr of idx files on disk
	OpAutoStringHashTable< OpAutoVector<OpString8> > m_index_cache;

	// Holds the index_filenames (Is this really used?, looks like they are only inserted and removed)
	OpAutoVector<OpString> m_index_key_list;
	OpListeners<FavIconListener> m_listeners;
	OpVector<FavIconLoader> m_loader_list;
	OpAutoVector<OpString8>* m_persistent_list;
	OpAutoVector<DocAndFavIcon>* m_bookmark_list;
	OpTimer* m_timer;
	URL_CONTEXT_ID  m_cache_id;
	class FileName_Store* m_cache;
	BOOL m_cache_init;
};

#endif
