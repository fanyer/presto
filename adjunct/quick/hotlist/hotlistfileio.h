/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Espen Sand (espen)
 * @author Co-owner: Karianne Ekern (karie)
 */
#ifndef __HOTLIST_FILEIO_H__
#define __HOTLIST_FILEIO_H__

#include "modules/util/opfile/opfile.h"
#include "modules/util/opstring.h"
#include "modules/util/opstrlst.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/decoders/inputconverter.h"

class OpFolderLister;

#include <stdio.h>

#ifdef _XML_SUPPORT_
# include "modules/xmlutils/xmltokenhandler.h"
#endif // _XML_SUPPORT_


namespace HotlistFileIO
{
	enum Encoding
	{
		Unknown = 0,
		Ascii,
		UTF8,
		UTF16
	};

	Encoding ProbeEncoding( const OpStringC &filename );
	BOOL ProbeCharset(const OpStringC& filename, OpString8& charset);
};

class HotlistFileReader
{
public:
	HotlistFileReader()
	{
		m_char_converter = 0;
		m_encoding = HotlistFileIO::Unknown;
	}

	~HotlistFileReader()
	{
		OP_DELETE(m_char_converter);
	}


	BOOL Init(const OpStringC &filename, HotlistFileIO::Encoding encoding);

	void SetCharset( const OpStringC8 &charset ) { m_charset.Set(charset.CStr()); }
	void SetEncoding( HotlistFileIO::Encoding encoding );

	uni_char* Readline(int& length);

private:
	OpFile m_file;
	OpString m_buffer;
	OpString8 m_charset;
	InputConverter* m_char_converter;
	HotlistFileIO::Encoding m_encoding;
};

class HotlistFileWriter
{
public:
	HotlistFileWriter();
	~HotlistFileWriter();

	OP_STATUS Open( const OpString& filename, BOOL safe );
	OP_STATUS Close();

	OP_STATUS WriteHeader( BOOL is_syncable = TRUE/*, INT32 model_type */ );
	OP_STATUS WriteFormat( const char *fmt, ... );
	OP_STATUS WriteUni( const uni_char *str, int count = 0);
	OP_STATUS WriteAscii( const char *str, int count = 0);

private:
	OpFile* m_fp;
	BOOL m_write_fail_occured;
	OpString m_buffer;
	OpString8 m_convert_buffer;
	UTF16toUTF8Converter m_encoder;
};


class HotlistDirectoryReader
{
public:
	class Node
	{
	public:
		OpString m_name;
		OpString m_url;
		OpString m_path;
		BOOL m_is_directory;
		int m_created;
		int m_visited;
	};

public:
	/**
	 * Constructor. Initializes the internal state
	 */
	HotlistDirectoryReader();

	/**
	 * Desctructor. Releases any allocated resources
	 */
	virtual ~HotlistDirectoryReader();

	/**
	 * Sets the directory path and extenstions to match.
	 *
	 * @param name The path
	 * @param extensions Comma separated list of extentions
	 *        example: "*.url,*.ini"
	 */
	void Construct( const OpStringC &name, const OpStringC &extensions );

	/**
	 * Returns the first node that match the extension string
	 * or NULL if there is no node to match.
	 */
	Node *GetFirstEntry();

	/**
	 * Returns the next node that match the extension string
	 * or NULL if there is no more nodes to match. You must use
	 * @ref GetFirstEntry() once first.
	 */
	virtual Node *GetNextEntry();

	/**
	 * Returns TRUE if the boomark node represents a folder
	 *
	 * @param node The node object returned by @ref GetFirstEntry()
	 *        or @ref GetNextEntry()
	 */
	BOOL IsDirectory( Node *node ) const;

	/**
	 * Returns TRUE if the boomark node represents a regular file
	 *
	 * @param node The node object returned by @ref GetFirstEntry()
	 *        or @ref GetNextEntry()
	 */
	BOOL IsFile( Node *node ) const;

	/**
	 * Returns the name of the bookmark node
	 *
	 * @param node The node object returned by @ref GetFirstEntry()
	 *        or @ref GetNextEntry()
	 */
	const OpStringC &GetName( Node *node ) const;

	/**
	 * Returns the url of the bookmark node
	 *
	 * @param node The node object returned by @ref GetFirstEntry()
	 *        or @ref GetNextEntry()
	 */
	const OpStringC &GetUrl( Node *node ) const;

	/**
	 * Returns the full path to the file or directory the
	 * bookmark node represents
	 *
	 * @param node The node object returned by @ref GetFirstEntry()
	 *        or @ref GetNextEntry()
	 */
	const OpStringC &GetPath( Node *node)  const;

	/**
	 * Returns the created time of the bookmark node
	 *
	 * @param node The node object returned by @ref GetFirstEntry()
	 *        or @ref GetNextEntry()
	 */
	int GetCreated( Node *node ) const;

	/**
	 * Returns the visited time of the bookmark node
	 *
	 * @param node The node object returned by @ref GetFirstEntry()
	 *        or @ref GetNextEntry()
	 */
	int GetVisited( Node *node ) const;

private:
	/**
	 * Resets the internal state
	 */
	void Close();
	Node* GetNextNode( BOOL first );

private:
	OpString m_directory_name;
	OpString_list m_extensions;
	OpString m_dummy;
	Node m_current_node;
	OpFolderLister* m_folder_lister;
};



class XBELReaderModel;
class XBELReaderModelItem : public TreeModelItem<XBELReaderModelItem, XBELReaderModel>
{
public:
	XBELReaderModelItem( bool is_folder ) { m_is_folder = is_folder; m_created=0; m_visited=0; }

	Type GetType() { return UNKNOWN_TYPE; }
	INT32 GetID() { return m_id; }
	OP_STATUS GetItemData( ItemData* item_data ) { return OpStatus::OK; }

	OpString m_name;
	OpString m_url;
	int m_is_folder;
	int m_created;
	int m_visited;
	INT32 m_id;
};


class XBELReaderModel : public TreeModel<XBELReaderModelItem>
{
public:
	INT32 GetColumnCount() { return 1; }
	OP_STATUS GetColumnData(ColumnData* column_data) { return OpStatus::OK; }
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	OP_STATUS GetTypeString(OpString& type_string) { return OpStatus::ERR; }
#endif
};


class HotlistXBELReader : public XMLTokenHandler
{
public:
	/**
	 * Constructor. Initializes the internal state
	 */
	HotlistXBELReader();

	/**
	 * Desctructor. Releases any allocated resources
	 */
	~HotlistXBELReader();

	/**
	 * Sets the filename to be used.
	 *
	 * @param name The filename
	 */
	void SetFilename( const OpStringC &name );

	/**
	 * Parses XML file and places result in internal model
	 */
	void Parse();

	/**
	 * Returns the number of nodes in internal model
	 */
	INT32 GetCount();

	/**
	 * Returns the node at index in internal model
	 */
	XBELReaderModelItem* GetItem(INT32 index);

	/**
	 * Returns the number pf children a node at index of internal model
	 */
	INT32 GetChildCount(INT32 index);

	/**
	 * Returns TRUE if the node represents a folder
	 *
	 * @param item The node object returned by @ref GetNextEntry()
	 */
	BOOL IsFolder(const XBELReaderModelItem* item) const;

	/**
	 * Returns the name of the node
	 *
	 * @param item The node object returned by @ref GetNextEntry()
	 */
	const OpStringC& GetName(const XBELReaderModelItem* item) const;

	/**
	 * Returns the url of the node
	 *
	 * @param item The node object returned by @ref GetNextEntry()
	 */
	const OpStringC& GetUrl(const XBELReaderModelItem* item) const;

	/**
	 * Returns the created time of the node (as read from file)
	 *
	 * @param item The node object returned by @ref GetNextEntry()
	 */
	INT32 GetCreated(const XBELReaderModelItem* item) const;

	/**
	 * Returns the visited time of the node (as read from file)
	 *
	 * @param item The node object returned by @ref GetNextEntry()
	 */
	INT32 GetVisited(const XBELReaderModelItem* item) const;

private:
	/**
	 * Releases any allocated resources
	 */
	void Close();

	/**
	 * Resets the internal state
	 */
	void Reset();

	/**
	 * Dump internal model
	 *
	 * @param item index Start with 0
	 * @param folder_count Start with -1
	 */
	void Dump( int& index, int folder_count );

	virtual XMLTokenHandler::Result HandleToken(XMLToken &token);

	void StartElementHandler(XMLToken &token);
	void EndElementHandler(XMLToken &token);
	void CharacterDataHandler(XMLToken &token);
	void ParseNetscapeInfo( const uni_char* src, int &add_date, int &last_visit ) const;

private:
	OpString m_filename;

	XBELReaderModelItem* m_xml_bookmark_item;
	XBELReaderModelItem* m_xml_folder_item;
	XBELReaderModelItem* m_xml_title_item;
	XBELReaderModel m_xml_model;

	int m_xml_folder_index;
	int m_xml_id;

	BOOL m_valid_format;
};

#endif
