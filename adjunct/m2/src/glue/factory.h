#ifndef GLUE_FACTORY_H
#define GLUE_FACTORY_H

#include "adjunct/m2/src/glue/connection.h"
#include "adjunct/m2/src/glue/mh.h"
#include "adjunct/m2/src/glue/util.h"
#include "adjunct/m2/src/glue/mime.h"

#include "modules/util/opfile/unistream.h"
//# include "modules/prefs/storage/prefsfile.h"

# include "modules/pi/network/OpSocket.h"
# include "modules/pi/network/OpHostResolver.h"

#include "modules/windowcommander/src/TransferManager.h"

#ifdef _DEBUG
#define CreateDirectorySearch()				CreateDirectorySearchDbg(__FILE__,__LINE__)
#define CreateCommGlue()					CreateCommGlueDbg(__FILE__,__LINE__)
#define CreateMessageLoop()					CreateMessageLoopDbg(__FILE__,__LINE__)
#define CreatePrefsFile(p)					CreatePrefsFileDbg(p,__FILE__,__LINE__)
#define CreatePrefsSection(p)				CreatePrefsSectionDbg(p,__FILE__,__LINE__)
#define CreateOpFile(p)						CreateOpFileDbg(p,__FILE__,__LINE__)
#define CreateURL()							CreateURLDbg(__FILE__,__LINE__)
#define CreateUnicodeFileInputStream(p,q)	CreateUnicodeFileInputStreamDbg(p, q, __FILE__,__LINE__)
#define CreateUnicodeFileOutputStream(p,q)	CreateUnicodeFileOutputStreamDbg(p, q, __FILE__,__LINE__)
#define CreateUTF8toUTF16Converter()		CreateUTF8toUTF16ConverterDbg(__FILE__,__LINE__)
#define CreateUTF16toUTF8Converter()		CreateUTF16toUTF8ConverterDbg(__FILE__,__LINE__)
#define CreateUTF7toUTF16Converter(p)		CreateUTF7toUTF16ConverterDbg(p, __FILE__,__LINE__)
#define CreateUTF16toUTF7Converter(p)		CreateUTF16toUTF7ConverterDbg(p, __FILE__,__LINE__)
#endif


class MimeUtils;
class BrowserUtils;
class ProtocolConnection;
class PrefsFile;

class GlueFactory 
{
  public:

	virtual ~GlueFactory() {};
		
// ----------------------------------------------------

#ifdef _DEBUG
	virtual ProtocolConnection* CreateCommGlueDbg(const OpStringC8& source_file, int source_line) = 0;
#else
	virtual ProtocolConnection* CreateCommGlue() = 0;
#endif
	virtual void DeleteCommGlue(ProtocolConnection*) = 0;

// ----------------------------------------------------

#ifdef _DEBUG
	virtual MessageLoop* CreateMessageLoopDbg(const OpStringC8& source_file, int source_line) = 0;
#else
	virtual MessageLoop* CreateMessageLoop() = 0;
#endif
	virtual void DeleteMessageLoop(MessageLoop* loop) = 0;

// ----------------------------------------------------

#ifdef _DEBUG
	virtual PrefsFile* CreatePrefsFileDbg(const OpStringC& path, const OpStringC8& source_file, int source_line) = 0;
#else
	virtual PrefsFile* CreatePrefsFile(const OpStringC& path) = 0;
#endif
    virtual void       DeletePrefsFile(PrefsFile* file) = 0;

// ----------------------------------------------------

#ifdef _DEBUG
    virtual PrefsSection* CreatePrefsSectionDbg(PrefsFile* prefsfile, const OpStringC& section, const OpStringC8& source_file, int source_line) = 0;
#else
    virtual PrefsSection* CreatePrefsSection(PrefsFile* prefsfile, const OpStringC& section) = 0;
#endif
    virtual void          DeletePrefsSection(PrefsSection* section) = 0;

// ----------------------------------------------------

#ifdef _DEBUG
	virtual OpFile* CreateOpFileDbg(const OpStringC& path, const OpStringC8& source_file, int source_line) = 0;
#else
	virtual OpFile* CreateOpFile(const OpStringC& path) = 0;
#endif
    virtual void    DeleteOpFile(OpFile* file) = 0;

// ----------------------------------------------------

#ifdef _DEBUG
    virtual URL* CreateURLDbg(const OpStringC8& source_file, int source_line) = 0;
#else
    virtual URL* CreateURL() = 0;
#endif
    virtual void DeleteURL(URL* url) = 0;

// ----------------------------------------------------

#ifdef _DEBUG
    virtual UnicodeFileInputStream*  CreateUnicodeFileInputStreamDbg(OpFile *inFile, UnicodeFileInputStream::LocalContentType content, const OpStringC8& source_file, int source_line) = 0;
#else
    virtual UnicodeFileInputStream*  CreateUnicodeFileInputStream(OpFile *inFile, UnicodeFileInputStream::LocalContentType content) = 0;
#endif
    virtual void                     DeleteUnicodeFileInputStream(UnicodeFileInputStream* stream) = 0;

// ----------------------------------------------------

#ifdef _DEBUG
    virtual UnicodeFileOutputStream* CreateUnicodeFileOutputStreamDbg(const uni_char* filename, const char* encoding, const OpStringC8& source_file, int source_line) = 0;
#else
    virtual UnicodeFileOutputStream* CreateUnicodeFileOutputStream(const uni_char* filename, const char* encoding) = 0;
#endif
    virtual void                     DeleteUnicodeFileOutputStream(UnicodeFileOutputStream* stream) = 0;

// ----------------------------------------------------

	virtual OpSocket*			CreateSocket(OpSocketListener &listener) = 0;
	virtual void				DestroySocket(OpSocket *socket) = 0;

	virtual OpSocketAddress*	CreateSocketAddress() = 0;
	virtual void				DestroySocketAddress(OpSocketAddress* socket_address) = 0;

	virtual OpHostResolver*		CreateHostResolver(OpHostResolverListener &resolver_listener) = 0;
	virtual void				DestroyHostResolver(OpHostResolver* host_resolver) = 0;

// ----------------------------------------------------

	virtual BrowserUtils* GetBrowserUtils() = 0;

	virtual MimeUtils* GetMimeUtils() = 0;

    virtual char*     OpNewChar(int size) = 0;
    virtual uni_char* OpNewUnichar(int size) = 0;
    virtual void      OpDeleteArray(void* p) = 0;

	// CreateMemoryManager (Alloc, Free)

	// 
};

#endif // GLUE_FACTORY_H
