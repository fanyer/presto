/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006-2010
 *
 * Web server implementation -- overall server logic
 */


#ifndef WEBSERVER_FILENAME_H_
#define WEBSERVER_FILENAME_H_

/* Class for constructing secure filepaths. Any path given in the constructors will be checked for dangerous /../'s. */

class WebserverFileName
{
public:
	WebserverFileName();
	virtual ~WebserverFileName();
	
	OP_STATUS Construct(const OpStringC &path);

	OP_STATUS Construct(const OpStringC8 &path);
	
	OP_STATUS ConstructFromURL(const OpStringC &path);

	OP_STATUS ConstructFromURL(const OpStringC8 &path);

	/* Returns the full path */	
	const uni_char *GetPathAndFileNamePointer() const;


	/* Returns a pointer to the filename and ignores the full path */
	const uni_char *GetFileNamePointer() const;


	static OP_STATUS SecureURLPathConcat(OpString &safeBasePath, const OpStringC &unsafeUrl);
	static OP_STATUS SecureURLPathConcat(OpString &safeBasePath, const OpStringC8 &unsafeUrl);	
					 
	/* Constructs the string safeBasePath = safeBasePath +"/" +unsafeUrl.
	 * All //, /../ and /./ in unsafeUrl are resolved securely.
	 * All url escaped characters in the unsafeUrl resolved into the real character. 
	 * For example: '%20' into ' ' (space, that is) 
	 * 
	 * @param safeBasePath (out)	The base path. This must be a secure path (not given by user input). 
	 * 								The unsafeUrl will be appended to this path.
	 * @param subPath  				The sub path from user input
	 * @param url_decode			If yes, this function will url decode any %nn codes. 
	 * 
	 * @return 	OpStatus::ERR_NO_MEMORY if memory error
	 * 			OpStatus::ERR 			if safeBasePath is unsecure, or if unsafeSubPath tries to go below safeBasePath
	 */
	static OP_STATUS SecurePathConcat(OpString &safeBasePath, const OpStringC &unsafeSubPath, BOOL url_decode = FALSE);
					 
	/* Constructs the string safeBasePath = safeBasePath +"/" +unsafeSubPath. 
	 * All //, /../ and /./ in are resolved securely 
	 * 
	 * @param safeBasePath (out)	The base path. This must be a secure path (not given by user input). 
	 * 								The unsafeSubPath which will be appended to safeBasePath.
	 * @param unsafeSubPath			The sub path from user input

	 * @return 	OpStatus::ERR_NO_MEMORY if memory error
	 * 			OpStatus::ERR 			if safeBasePath is unsecure, or if unsafeSubPath tries to go below safeBasePath
	 */


#ifdef _DEBUG
	/// Debug method that returns TRUE if the URL is accpetable and does not potentially violate the security of Unite;
	/// if expected is not NULL, it has to be equal to the final URL for the function to return TRUE (as CheckAndFixDangerousPaths can change the URL)
	static BOOL DebugIsURLAcceptable(const char *filePathAndName, const char *expected=NULL);
	/// Debug method that returns TRUE if the URL is NOT accpetable and does not potentially violate the security of Unite;
	/// if expected is not NULL, it has to be equal to the final URL for the function to return TRUE (as CheckAndFixDangerousPaths can change the URL)
	static BOOL DebugIsURLInAcceptable(const char *filePathAndName, const char *expected=NULL);
#endif

private:

	OP_STATUS Construct(const char *path, int length, BOOL url_decode = FALSE ); /* Depricated */
	
	OP_STATUS Construct(const uni_char *path, int length, BOOL url_decode = FALSE ); /* Depricated */

	static OP_STATUS CheckAndFixDangerousPaths(uni_char *filePathAndName, BOOL url_decode = FALSE );
	OpString m_filePathAndName;
	BOOL m_isURL;
};

#endif /*WEBSERVER_FILENAME_H_*/
