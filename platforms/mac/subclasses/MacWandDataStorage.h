#ifndef MAC_WAND_DATA_STORAGE_H
#define MAC_WAND_DATA_STORAGE_H

#ifdef WAND_EXTERNAL_STORAGE

#include "modules/wand/wandmanager.h"
#include <KeychainCore.h>

class MacWandDataStorage : public WandDataStorage
{
public:
	enum ParseResult
	{
		kAllInvalid			= 0,
		kServerNameValid 	= 0x01,
		kPathValid 			= 0x02,
		kPortValid			= 0x04,
		kProtocolValid		= 0x08
	};

	MacWandDataStorage();
	virtual ~MacWandDataStorage();

	/** Store the entrydata. The data should be associated with the serverstring so it can be found with FetchData.
		The serverstring is either a url, or a mix of servername:port depending of the type of login that is stored.
	*/
	virtual void StoreData(const uni_char* server, WandDataStorageEntry* entry);

	/** Return TRUE and set the info in entry if there was a match.
		index is which match to return if there is several. */
	virtual BOOL FetchData(const uni_char* server, WandDataStorageEntry* entry, INT32 index);

	/** Delete data from external storage (if exact match is found) */
	virtual OP_STATUS DeleteData(const uni_char* server, const uni_char* username);

private:
	MacWandDataStorage::ParseResult ParseString(const uni_char* str, uni_char* &servername, uni_char* &path, UInt16 &port, FourCharCode &protocol);
	BOOL GetAccountAndPassword(KCItemRef &itemRef, WandDataStorageEntry* entry);

	OSStatus FindEntryInKeychain(	const uni_char* server,
									size_t serverlen,
									const uni_char* username,
									size_t usernamelen,
									const uni_char* path,
									size_t pathlen,
									UInt16 port,
									OSType protocol,
									OSType authtype,
									UInt32 maxlength,
									Str255 &passwordData,
									Boolean &exactMatch,
									KCItemRef* item);

	KCSearchRef mSearchRef;
	INT32		mLastIndex;
};

#endif // WAND_EXTERNAL_STORAGE
#endif // MAC_WAND_DATA_STORAGE_H
