// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef BT_P2POpFile64_H
#define BT_P2POpFile64_H

// ----------------------------------------------------

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"
# include "modules/util/opfile/opfile.h"

#include <openssl/evp.h>
#include <openssl/sha.h>


//typedef UINT64 OpFileLength;
/*
enum SeekMode
{
    SEEK_FROM_START,
    SEEK_FROM_END,
    SEEK_FROM_CURRENT
};
*/

typedef OpSeekMode SeekMode;

enum OpenMode
{
	kClosed,
	kOpenRead,
	kOpenWrite,
	kOpenReadAndWrite,
	kOpenAppend,
	kOpenAppendAndRead,
	kOpenReadAndWriteNew
};

enum ShareMode
{
	kShareFull,
	kShareDenyRead,
	kShareDenyWrite,
	kShareDenyReadAndWrite
};

enum FileKind									// You do not have extensions. Instead you now have file types.
{
	kFileKindIllegal		= 0,				// illegal file kind (for trapping errors)
	kFileKindText			= 1,				// "*.txt", a readable text file in plain ascii
	kFileKindHTML			= 2,				// "*.html", HTML document
	kFileKindGIF			= 3,				// "*.gif", Graphics Interchange Format
	kFileKindJpeg			= 4,				// "*.jpg", Jpeg
	kFileKindPiNG			= 5,				// "*.png", Portable Network Graphics
	kFileKindBitmap			= 6,				// "*.bmp", Bitmap
	kFileKindXBitmap		= 7,				// "*.xbm", Compressed Bitmap
	kFileKindPreferences	= 8,				// "*.ini", Preferences file ('pref' on Mac)
	kFileKindCache			= 9,				// "tmp*.*", cache file
	kFileKindCookie			= 10,				// "cookies.dat" or "cookies4.dat", -cookie file
	kFileKindVisitedLinks	= 11,				// "vlink.dat" -visited links
	kFileKindButtons		= 12,				// "buttons.ini" (note: due to extension problems, it will not be possible to support this on all platforms)
	kFileKindBookmarks		= 13,				// "*.adr" (opera3.adr, default.adr)
	kFileKindXML			= 14,				// "*.xml"
	kFileKindCSS			= 15,				// "*.css"
	kFileKindMUBF			= 16,				// Mac specific.
	kFileKindUrlHttp		= 17,
#ifndef NO_FTP_SUPPORT
	kFileKindUrlFtp			= 18,
#endif //NO_FTP_SUPPORT
	kFileKindUrlFile		= 19,
	kFileKindUrlEmail		= 20,
	kFileKindUrlNetscape	= 21,
	kFileKindUrlExplorer	= 22,
	kFileKindApplication	= 23,				// "*.exe", Executable application
	kFileKindCertificates	= 24,				// "*cert.dat", certificate files
	kFileKindChartables     = 25,				// "chartables.bin", our encoding database
	kFileKindBinary			= 26,				// miscellaneous binary files
	kFileKinds									// number of filetypes available
};

enum TranslationMode
{
	kTranslateBinary,
	kTranslateText
};


class OpFile64
{

private:
	OpFileLength		m_bytesWritten;		// number of bytes actually written
	OpFileLength		m_bytesRead;		// number of bytes actually read

	OpenMode			m_mode;				// how file was opened

	ShareMode			m_share_mode;		// deny sharing with other applications when open?
#if defined(_WINDOWS_)
	HANDLE				m_fp;
#elif defined(UNIX) || defined(_MACINTOSH_)
    FILE*               m_fp;
#else
	OpFile				m_opfile;
#endif
	OpString		    m_pathname;

// Make copy constructor and assignment operator inaccessible.
	OpFile64(const OpFile64 &copy);
	OpFile64 &operator=(const OpFile64 &copy);

public:
	OpFile64();
	virtual ~OpFile64();
	
	virtual OP_STATUS Open(FileKind fileType, OpenMode openMode, ShareMode shareMode, TranslationMode translationMode);
    virtual BOOL	IsOpen();
	BOOL SetFilePos(OpFileLength position, SeekMode seekMode);
	OpFileLength GetFilePos();
	virtual BOOL Length(OpFileLength *length);
	virtual BOOL		Read(void *buffer, OpFileLength bytes);
	virtual BOOL		Write(const void *buffer, OpFileLength bytes);
	virtual BOOL		Close();
	virtual BOOL		Flush();
	virtual OP_STATUS	GetFullPath(OpString& path)
	{
		return path.Set(m_pathname.CStr());
	}
	void ConstructL(const OpStringC &location, const OpStringC& filename);
	static OP_STATUS MakeDir(const uni_char* path);
	static OP_STATUS MakePath(uni_char* path);
};

#endif // BT_P2POpFile64
