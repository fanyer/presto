/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Espen Sand (espen)
 * @author Co-owner: Karianne Ekern (karie)
 *
 */
#include "core/pch.h"
#include "modules/util/opfile/opfile.h"
#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/pi/OpSystemInfo.h"

#ifdef _XML_SUPPORT_
# include "modules/xmlutils/xmlnames.h"
# include "modules/xmlutils/xmlparser.h"
# include "modules/xmlutils/xmltoken.h"
# include "modules/url/url2.h"
#endif // _XML_SUPPORT_

#include "adjunct/desktop_util/opfile/desktop_opfile.h"
#include "modules/util/opfile/opsafefile.h"

#include "hotlistfileio.h"

#include <ctype.h>
#if !defined(MSWIN)
# include <dirent.h>
#endif
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>  // for stat()
#if !defined(_MACINTOSH_)
#include <sys/types.h> // for stat()
#endif
#if defined (MSWIN)
#include "platforms/Windows/win_handy.h"
#endif


HotlistFileIO::Encoding HotlistFileIO::ProbeEncoding(const OpStringC &filename)
{
	OpFile file;

	if(OpStatus::IsError(file.Construct(filename.CStr())))
	{
		return Unknown;
	}

	if(OpStatus::IsError(file.Open(OPFILE_READ)))
	{
		return Unknown;
	}

	OpString8 buf;
	if(OpStatus::IsError(file.ReadLine(buf)))
	{
		return Unknown;
	}

	// Scan start of file for the header:
	//
	// Opera Hotlist version 2.0
	// Options: encoding = utf8, version=3

	if( !buf.CStr() || !strstr(buf.CStr(),"Opera Hotlist version 2.0") )
	{
		return Unknown;
	}

	if(OpStatus::IsError(file.ReadLine(buf)))
	{
		return Unknown;
	}

	int len = buf.Length();
	for( int i=0; i<len; i++ )
	{
		if( !isspace(buf.CStr()[i]) )
		{
			tolower(BYTE(buf.CStr()[i]));
		}
	}

	if( strstr( buf.CStr(), "options:encoding=") == buf.CStr() )
	{
		const char *start = &buf.CStr()[17];
		if( strstr( start, "utf8") == start )
		{
			return UTF8;
		}
		if( strstr( start, "utf16") == start )
		{
			return UTF16;
		}
		if( strstr( start, "ascii") == start )
		{
			return Ascii;
		}
	}

	return Unknown;

#if 0
	rewind(fp);

	int utf8_length = 0;
	int utf8_invalid = 0;
	int utf8_valid = 0;

	for( int ch; (ch=fgetc(fp)) != EOF;  )
	{
		if (utf8_length)
		{
			// We are inside an UTF-8 sequence
			if((ch & 0xC0) == 0x80)
			{
				// Update state
				utf8_length --;

				// Count if we are done with this sequence
				utf8_valid += !utf8_length;
			}
			else
			{
				// Invalid UTF-8 data
				utf8_length = 0;
				utf8_invalid ++;
			}
		}
		else if ((ch & 0xE0) == 0xC0)
		{
			// Two byte UTF-8 sequence; seed state machine
			utf8_length = 1;
		}
		else if ((ch & 0xF0) == 0xE0)
		{
			// Three byte UTF-8 sequence; seed state machine
			utf8_length = 2;
		}
		else if ((ch & 0xF8) == 0xF0)
		{
			// Four byte UTF-8 sequence; seed state machine
			utf8_length = 3;
		}
		else if ((ch & 0xFC) == 0xF8)
		{
			// Five byte UTF-8 sequence; seed state machine
			utf8_length = 4;
		}
		else if ((ch & 0xFE) == 0xFC)
		{
			// Six byte UTF-8 sequence; seed state machine
			utf8_length = 5;
		}
	}

	fclose( fp );

	return utf8_valid > utf8_invalid ? UTF8 : Unknown;
#endif
}


BOOL HotlistFileIO::ProbeCharset(const OpStringC& filename, OpString8& charset)
{
	charset.Empty();

	OpFile file;

	if(OpStatus::IsError(file.Construct(filename.CStr())))
	{
		return FALSE;
	}

	if(OpStatus::IsError(file.Open(OPFILE_READ)))
	{
		return FALSE;
	}

	BOOL success = FALSE;

	while( !file.Eof() )
	{
		OpString8 buf;
		if(OpStatus::IsError(file.ReadLine(buf)))
		{
			break;
		}

		if (buf.HasContent())
		{
			const char* p1 = strstr(buf.CStr(), "<META");
			if (p1)
			{
				const char* p2 = strstr(p1, "CONTENT");
				if (p2)
				{
					const char* p3 = strstr(p2, "charset=");
					if (p3)
					{
						const char* p4 = strstr(p3, "\">");
						charset.Set(p3 + 8, p4 - (p3 + 8));
						success = TRUE;
					}
				}
				break;
			}
		}
	}

	return success;
}



BOOL HotlistFileReader::Init(const OpStringC& filename, HotlistFileIO::Encoding encoding)
{
	SetEncoding(encoding);
	RETURN_VALUE_IF_ERROR(m_file.Construct(filename.CStr()), FALSE);
	return OpStatus::IsSuccess(m_file.Open(OPFILE_READ));
}


void HotlistFileReader::SetEncoding( HotlistFileIO::Encoding encoding )
{
	if( m_encoding != encoding || !m_char_converter )
	{
		m_encoding = encoding;

		OP_DELETE(m_char_converter);
		m_char_converter = 0;

		if( m_encoding == HotlistFileIO::Unknown || m_encoding == HotlistFileIO::Ascii )
		{
#if defined (MSWIN)
			InputConverter::CreateCharConverter(m_charset.CStr() ? m_charset.CStr() : GetSystemCharsetName(), &m_char_converter);
#else
		 	InputConverter::CreateCharConverter(m_charset.CStr() ? m_charset.CStr() : "iso-8859-1", &m_char_converter );
#endif
		}
	}
}


uni_char* HotlistFileReader::Readline(int& length)
{
	if(!m_file.IsOpen())
	{
		return NULL;
	}
	if( m_file.Eof() )
	{
		return NULL;
	}

	if( m_char_converter )
	{
		OpString8 buf;
		if(OpStatus::IsError(m_file.ReadLine(buf)))
		{
			return NULL;
		}
		buf.Strip(TRUE,FALSE);

		length = buf.Length();
		while( length > 0 && (buf.CStr()[length-1] == '\n' || buf.CStr()[length-1] == '\r') )
		{
			buf.CStr()[length-1] = 0;
			length--;
		}

		if( !m_buffer.Reserve(length+1) )
		{
			return NULL;
		}

		int bytes_read, bytes_written;
		bytes_written = m_char_converter->Convert(buf.CStr(), length, (char*)m_buffer.CStr(), m_buffer.Capacity()*sizeof(uni_char), &bytes_read);
		if( bytes_written >= 0 )
		{
			length = UNICODE_DOWNSIZE(bytes_written);
			m_buffer.CStr()[length]=0;
			return m_buffer.CStr();
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		m_file.ReadUTF8Line(m_buffer);
		m_buffer.Strip(TRUE,FALSE);

		length = m_buffer.Length();
		while( length > 0 && (m_buffer.CStr()[length-1] == '\n' || m_buffer.CStr()[length-1] == '\r') )
		{
			m_buffer.CStr()[length-1] = 0;
			length--;
		}

		return m_buffer.CStr();
	}
}


HotlistFileWriter::HotlistFileWriter()
{
	m_fp = 0;
	m_write_fail_occured = FALSE;
}


HotlistFileWriter::~HotlistFileWriter()
{
	Close();
}


OP_STATUS HotlistFileWriter::Open( const OpString& filename, BOOL safe )
{
	Close();

	if( safe )
	{
		m_fp = OP_NEW(OpSafeFile, ());
	}
	else
	{
		m_fp = OP_NEW(OpFile, ());
	}

	if( !m_fp )
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = m_fp->Construct(filename.CStr());
	if (OpStatus::IsError(status))
	{
		OP_DELETE(m_fp);
		m_fp = 0;
		return status;	
	}
	return m_fp->Open(OPFILE_WRITE | OPFILE_TEXT);
}


OP_STATUS HotlistFileWriter::Close()
{
	OP_STATUS ret = OpStatus::OK;

	if( m_fp )
	{
		if( m_fp->Type() == OPSAFEFILE )
		{
			if( !m_write_fail_occured )
			{
				ret = ((OpSafeFile*)m_fp)->SafeClose();
			}
		}
		else
		{
			ret = m_fp->Close();
		}
		OP_DELETE(m_fp);
		m_fp = 0;
	}

	m_write_fail_occured = FALSE;

	return ret;
}

OP_STATUS HotlistFileWriter::WriteHeader(BOOL is_syncable /*, INT32 model_type */)
{
	OP_STATUS ret;

	if (is_syncable) // default= TRUE
	{
		if ( OpStatus::IsError(ret=WriteAscii("Opera Hotlist version 2.0\n")) ||
			 OpStatus::IsError(ret=WriteAscii("Options: encoding = utf8, version=3\n\n")))
		{
			m_write_fail_occured = TRUE;
			return ret;
		}
	}
	else 
	{
		if ( OpStatus::IsError(ret=WriteAscii("Opera Hotlist version 2.0\n")) ||
			 OpStatus::IsError(ret=WriteAscii("Options: encoding = utf8, version=3, sync=NO\n\n")))
		{
			m_write_fail_occured = TRUE;
			return ret;
		}
	}

   return OpStatus::OK;
}


OP_STATUS HotlistFileWriter::WriteFormat( const char *fmt, ... )
{
	if( !fmt || !m_fp)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	// Some systems will change ap. We use ap1 and ap2 here to make that visible.

	va_list ap1;
	va_start( ap1, fmt );
	OpString fmt_str;
	fmt_str.Set(fmt);
	int num = uni_vsnprintf(NULL, 0, fmt_str.CStr(), ap1);
	va_end(ap1);

	if( num <= 0 )
	{
		// Return error flag on a write error only
		return OpStatus::OK;
	}

	if( m_buffer.Capacity() <= num )
	{
		const uni_char* p = m_buffer.Reserve(num+10000);
		if( !p )
		{
			m_write_fail_occured = TRUE;
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	va_list ap2;
	va_start( ap2, fmt );
	uni_vsnprintf(m_buffer.CStr(), m_buffer.Capacity(), fmt_str.CStr(), ap2);
	va_end(ap2);

	return WriteUni(m_buffer.CStr(), num);
}

OP_STATUS HotlistFileWriter::WriteUni( const uni_char *str, int count )
{
	if (count == 0)
		count = uni_strlen(str);

	int len = count * sizeof(uni_char);
	if( len*4 > m_convert_buffer.Capacity() )
	{
		const char* p = m_convert_buffer.Reserve(len*4);
		if( !p )
		{
			m_write_fail_occured = TRUE;
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	int readLen = 0;
	int n;

	n = m_encoder.Convert( (char*)str, len, m_convert_buffer.CStr(), m_convert_buffer.Capacity(), &readLen );

	return WriteAscii(m_convert_buffer.CStr(), n);
}


OP_STATUS HotlistFileWriter::WriteAscii( const char *str, int count )
{
	if (count == 0)
		count = op_strlen(str);

	OP_STATUS ret = m_fp->Write(str, count);
	if (OpStatus::IsError(ret))
	{
		m_write_fail_occured = TRUE;
	}
	return ret;
}


HotlistDirectoryReader::HotlistDirectoryReader()
{
	m_folder_lister = 0;
}


HotlistDirectoryReader::~HotlistDirectoryReader()
{
	Close();
}


void HotlistDirectoryReader::Construct( const OpStringC &name, const OpStringC &extensions )
{
	Close();
	m_folder_lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, extensions.CStr(), name.CStr());
}


void HotlistDirectoryReader::Close()
{
	OP_DELETE(m_folder_lister);
	m_folder_lister = 0;
}


HotlistDirectoryReader::Node *HotlistDirectoryReader::GetFirstEntry()
{
	return GetNextNode(TRUE);
}


HotlistDirectoryReader::Node *HotlistDirectoryReader::GetNextEntry()
{
	return GetNextNode(FALSE);
}


HotlistDirectoryReader::Node *HotlistDirectoryReader::GetNextNode( BOOL first )
{
	while( m_folder_lister && m_folder_lister->Next() )
	{
		first = FALSE;

		const uni_char *filename = m_folder_lister->GetFileName();
		if( !filename )
		{
			continue;
		}

		if( uni_strcmp( filename, UNI_L(".") ) == 0 ||
			uni_strcmp( filename, UNI_L("..") ) == 0 )
		{
			continue;
		}

		const uni_char *path_filename = m_folder_lister->GetFullPath();
		if( !path_filename )
		{
			continue;
		}

		OpFile file;
		OpFileInfo::Mode mode;

		if( OpStatus::IsError(file.Construct(path_filename)) )
		{
			continue;
		}
		if( OpStatus::IsError(file.GetMode(mode)) )
		{
			continue;
		}
		if( mode != OpFileInfo::FILE && mode != OpFileInfo::DIRECTORY )
		{
			continue;
		}

		if( mode != OpFileInfo::DIRECTORY )
		{
			// This was added after Folderlister pattern was changed from "*.url" to "*" 
			// for bug #281778 [espen 2007-09-07]
			if( uni_stristr( filename, UNI_L(".url") ) == 0 )
			{
				continue;
			}

			if (OpStatus::IsError(file.Open(OPFILE_READ)))
			{
				continue;
			}

			m_current_node.m_url.Empty();

			OpString base_url, orig_url;

			while( !file.Eof() )
			{
				OpString buf;
				if (OpStatus::IsError(file.ReadUTF8Line(buf)))
				{
					break;
				}

				// Read until we find a 'URL=' entry. If no 'URL=' prefer
				// 'BASEURL=' and then 'ORIGURL='
				if( buf.HasContent() )
				{
					uni_char* b = uni_strstr( buf.CStr(), UNI_L("BASEURL=") );
					if( b )
					{
						base_url.Set( b+8 );
						base_url.Strip();
					}
					else
					{
						b = uni_strstr( buf.CStr(), UNI_L("ORIGURL=") );
						if( b )
						{
							orig_url.Set( b+8 );
							orig_url.Strip();
						}
						else
						{
							b = uni_strstr( buf.CStr(), UNI_L("URL=") );
							if( b )
							{
								m_current_node.m_url.Set( b+4 );
								m_current_node.m_url.Strip();
								break;
							}
						}
					}
				}
			}

			if( m_current_node.m_url.IsEmpty() )
			{
				if( base_url.Length() > 0 )
				{
					m_current_node.m_url.Set( base_url.CStr() );
				}
				else if( orig_url.Length() > 0 )
				{
					m_current_node.m_url.Set( orig_url.CStr() );
				}
			}
		}

		OpString name;
		name.Set( filename );
		m_current_node.m_is_directory = mode == OpFileInfo::DIRECTORY;

		// Remove file extension from name of bookmark item (not folder)
		if (!m_current_node.m_is_directory)
		{
			int pos = name.FindLastOf('.'); 
			if( pos != KNotFound )
			{
				name.Set( filename, pos );
			}
		}

		struct stat buf;
		if( OpStatus::IsSuccess(DesktopOpFileUtils::Stat(&file, &buf)) )
		{
			m_current_node.m_created = (int)buf.st_ctime;
		}
		else
		{
			m_current_node.m_created = (int)g_op_time_info->GetTimeUTC(); // Now, since we can't get the stat
		}
		m_current_node.m_visited = 0; // Not yet visited as its recently imported
		m_current_node.m_name.Set(name);
		m_current_node.m_path.Set(path_filename);
		return &m_current_node;
	}
	return 0;
}


BOOL HotlistDirectoryReader::IsDirectory( Node *node ) const
{
	return node && node == &m_current_node ? m_current_node.m_is_directory : FALSE;
}


BOOL HotlistDirectoryReader::IsFile( Node *node ) const
{
	return node && node == &m_current_node ? !m_current_node.m_is_directory : FALSE;
}


const OpStringC &HotlistDirectoryReader::GetName( Node *node ) const
{
	return  node && node == &m_current_node ? m_current_node.m_name : m_dummy;
}


const OpStringC &HotlistDirectoryReader::GetUrl( Node *node ) const
{
	return  node && node == &m_current_node ? m_current_node.m_url : m_dummy;
}


const OpStringC &HotlistDirectoryReader::GetPath( Node *node ) const
{
	return  node && node == &m_current_node ? m_current_node.m_path : m_dummy;
}


int HotlistDirectoryReader::GetCreated( Node *node ) const
{
	return node && node == &m_current_node ? m_current_node.m_created :0;
}


int HotlistDirectoryReader::GetVisited( Node *node ) const
{
	return node && node == &m_current_node ? m_current_node.m_visited :0;
}



HotlistXBELReader::HotlistXBELReader()
{
	Reset();
}


HotlistXBELReader::~HotlistXBELReader()
{
	Close();
}


void HotlistXBELReader::SetFilename( const OpStringC &name )
{
	Close();
	m_filename.Set(name);

}


void HotlistXBELReader::Close()
{
	m_xml_model.DeleteAll();
	m_filename.Empty();
	Reset();
}


void HotlistXBELReader::Reset()
{
	m_xml_bookmark_item = 0;
	m_xml_folder_item = 0;
	m_xml_title_item = 0;

	m_xml_folder_index = -1;
	m_xml_id = 0;

	m_valid_format = FALSE;
}

void HotlistXBELReader::Parse()
{
	char *buffer = 0;
	int length = 0;

	if( m_filename.HasContent() )
	{
		OpFile file;

		if(OpStatus::IsError(file.Construct(m_filename.CStr())))
		{
			return;
		}

		if(OpStatus::IsError(file.Open(OPFILE_READ)))
		{
			return;
		}

		OpFileLength len;
		if (OpStatus::IsError(file.GetFileLength(len)) || len <= 0)
		{
			return;
		}

		buffer = OP_NEWA(char, (int)len);
		if( !buffer )
		{
			return;
		}
		length = (int)len;

		OpFileLength bytes_read;
		if(OpStatus::IsError(file.Read(buffer,len,&bytes_read)))
		{
			OP_DELETEA(buffer);
			return;
		}

		if( bytes_read != len )
		{
			OP_DELETEA(buffer);
			return;
		}
	}


#if defined(_XML_SUPPORT_)

	OpString buffer16;

	if (OpStatus::IsError(buffer16.SetFromUTF8(buffer, length)))
		return;

	XMLParser *parser;
	URL empty;

	if (OpStatus::IsError(XMLParser::Make(parser, NULL, g_main_message_handler, this, empty)))
		return;

	XMLParser::Configuration configuration;
	configuration.load_external_entities = XMLParser::LOADEXTERNALENTITIES_NO;
	parser->SetConfiguration(configuration);

	OpStatus::Ignore(parser->Parse(buffer16.CStr(), buffer16.Length(), FALSE));

	OP_DELETE(parser);

#endif

	OP_DELETEA(buffer);

	//int index = 0;
	//Dump( index, -1 );
}


void HotlistXBELReader::Dump( int& index, int folder_count )
{
	printf("-->\n");

	for( ; index < m_xml_model.GetCount(); )
	{
		if( folder_count == 0 )
		{
			break;
		}

		XBELReaderModelItem* item = (XBELReaderModelItem*)m_xml_model.GetItemByPosition(index);
		if( item )
		{
			OpString8 name;
			name.Set( item->m_name.CStr() );
			printf("NAME: %s\n", name.CStr() );

			if( item->m_is_folder )
			{
				printf("FOLDER\n" );
				int child_count = m_xml_model.GetChildCount(index);
				index++;
				Dump( index, child_count );
			}
			else
			{
				OpString8 url;
				url.Set( item->m_url.CStr() );
				printf("URL : %s\n", url.CStr() );
				index ++;
			}
		}

		folder_count --;
	}

	printf("<--\n");
}


XMLTokenHandler::Result HotlistXBELReader::HandleToken(XMLToken &token)
{
	switch (token.GetType())
	{
	case XMLToken::TYPE_CDATA:
	case XMLToken::TYPE_Text:
		CharacterDataHandler(token);
		break;

	case XMLToken::TYPE_STag:
	case XMLToken::TYPE_ETag:
	case XMLToken::TYPE_EmptyElemTag:
		if (token.GetType() != XMLToken::TYPE_ETag)
			StartElementHandler(token);
		if (token.GetType() != XMLToken::TYPE_STag)
			EndElementHandler(token);
		break;
	}

	return XMLTokenHandler::RESULT_OK;
}


void HotlistXBELReader::StartElementHandler(XMLToken &token)
{
	if( !m_valid_format )
	{
		if( token.GetName() == XMLExpandedName((const uni_char *) NULL, UNI_L("xbel")) )
		{
			m_valid_format = true;
		}
	}
	else
	{
		if( token.GetName() == XMLExpandedName((const uni_char *) NULL, UNI_L("bookmark")) )
		{
			if (!(m_xml_bookmark_item = OP_NEW(XBELReaderModelItem, (false))))
				return;
			m_xml_bookmark_item->m_id = m_xml_id++;
			m_xml_model.AddLast(m_xml_bookmark_item, m_xml_folder_index);

			XMLToken::Attribute *attributes = token.GetAttributes();

			for( unsigned index = 0; index < token.GetAttributesCount(); ++index )
			{
				if( attributes[index].GetName() == XMLExpandedName((const uni_char *) NULL, UNI_L("href")) )
				{
					m_xml_bookmark_item->m_url.Set(attributes[index].GetValue(), attributes[index].GetValueLength());
				}
				if( attributes[index].GetName() == XMLExpandedName((const uni_char *) NULL, UNI_L("netscapeinfo")) )
				{
					OpString value;
					value.Set(attributes[index].GetValue(), attributes[index].GetValueLength());
					ParseNetscapeInfo( value.CStr(), m_xml_bookmark_item->m_created, m_xml_bookmark_item->m_visited );
				}
			}
		}
		if( token.GetName() == XMLExpandedName((const uni_char *) NULL, UNI_L("folder")) )
		{
			if (!(m_xml_folder_item = OP_NEW(XBELReaderModelItem, (true))))
				return;
			m_xml_folder_item->m_id = m_xml_id++;
			m_xml_folder_index = m_xml_model.AddLast(m_xml_folder_item, m_xml_folder_index);
		}
		else if( token.GetName() == XMLExpandedName((const uni_char *) NULL, UNI_L("title")) )
		{
			m_xml_title_item = m_xml_bookmark_item ? m_xml_bookmark_item : m_xml_folder_item;
		}
	}
}


void HotlistXBELReader::EndElementHandler(XMLToken &token)
{
	if( m_valid_format)
	{
		if( token.GetName() == XMLExpandedName((const uni_char *) NULL, UNI_L("bookmark")) )
		{
			if( m_xml_title_item == m_xml_bookmark_item )
				m_xml_title_item = 0;
			m_xml_bookmark_item = 0;
		}
		else if( token.GetName() == XMLExpandedName((const uni_char *) NULL, UNI_L("title")) )
		{
			m_xml_title_item = 0;
		}
		else if( token.GetName() == XMLExpandedName((const uni_char *) NULL, UNI_L("folder")) )
		{
			if( m_xml_title_item == m_xml_folder_item )
				m_xml_title_item = 0;
			m_xml_folder_index = m_xml_model.GetParentIndex(m_xml_model.GetIndexByItem(m_xml_folder_item));
			m_xml_folder_item = 0;
		}
	}
}


void HotlistXBELReader::CharacterDataHandler(XMLToken &token)
{
	if( m_xml_title_item )
	{
		uni_char *s = token.GetLiteralAllocatedValue();
		m_xml_title_item->m_name.Set(s,token.GetLiteralLength());
		OP_DELETEA(s);
	}
}


void HotlistXBELReader::ParseNetscapeInfo( const uni_char* src, int &add_date, int &last_visit ) const
{
	add_date = last_visit = 0;

	if( !src )
	{
		return;
	}

	const uni_char* p = uni_strstr( src, UNI_L("ADD_DATE=") );
	if( p )
	{
		uni_sscanf( p+9, UNI_L("\"%d\""), &add_date );
	}
	p = uni_strstr( src, UNI_L("LAST_VISIT=") );
	if( p )
	{
		uni_sscanf( p+11, UNI_L("\"%d\""), &last_visit );
	}
}


INT32 HotlistXBELReader::GetCount()
{
	return m_xml_model.GetCount();
}

XBELReaderModelItem* HotlistXBELReader::GetItem(INT32 index)
{
	return (XBELReaderModelItem*)m_xml_model.GetItemByPosition(index);
}


INT32 HotlistXBELReader::GetChildCount(INT32 index)
{
	return m_xml_model.GetChildCount(index);
}

BOOL HotlistXBELReader::IsFolder(const XBELReaderModelItem* item) const
{
	return item->m_is_folder;
}

const OpStringC& HotlistXBELReader::GetName(const XBELReaderModelItem* item) const
{
	return item->m_name;
}

const OpStringC& HotlistXBELReader::GetUrl(const XBELReaderModelItem* item) const
{
	return item->m_url;
}

INT32 HotlistXBELReader::GetCreated(const XBELReaderModelItem* item) const
{
	return item->m_created;
}

INT32 HotlistXBELReader::GetVisited(const XBELReaderModelItem* item) const
{
	return item->m_visited;
}

