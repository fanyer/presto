/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _XML_UPDATE_H_
#define _XML_UPDATE_H_

#if defined UPDATERS_ENABLE_AUTO_FETCH 
#include "modules/url/url2.h"
#include "modules/updaters/url_updater.h"

#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"

#include "modules/datastream/fl_lib.h"

#ifdef _NATIVE_SSL_SUPPORT_
class SSL_varvector32;
#endif

#include "modules/updaters/update_man.h"

#ifdef CRYPTO_VERIFY_SIGNED_TEXTFILE_SUPPORT
# include "modules/libcrypto/include/CryptoVerifySignedTextFile.h"
typedef CryptoHashAlgorithm UpdaterSigAlg;
#else
typedef SSL_HashAlgorithmType UpdaterSigAlg;
#endif


/** Basic implementation for downloading and processing a XML document, optionally (default) signed 
 *
 *	Usage:
 *		- Construct the object specifying the URL top be loaded and the finished message
 *		- Optionally specify verification and load policies
 *		- Hand over to manager, or start loading directly
 *		- If verification is enabled (It is enabled by default, and configured to use the AUTOUPDATE_CERTNAME 
 *			public key and RSA with SHA_256) the file will be verified. Unless the subclass implement an override 
 *			the CryptoVerifySignedTextFile will be used, with a default XML prefix.
 *		- When loaded and (optionally verified) the implementation's ProcessFile() function is called
 *			- This function process the "parser" member, using the XMLFragment API and 
 *				the Get* API helper functions in this class for strings, flags and encoded content
 *			- When finished it must return OpStatus::OK or an error, SetFinished is called by the caller.
 *			- The function MUST complete in a single step
 */
class XML_Updater : public URL_Updater
{
private:
	/** Should this document be verified? Default TRUE */
	BOOL verify_file;
	/** Pubkey for signature verification */
	const byte *pub_sigkey;
	/** Pub key length */
	uint32 pub_sigkey_len;
	/** Algorithm for verification */
	UpdaterSigAlg pub_sigalg;

protected:

	/** Parser fragment */
	XMLFragment parser;

public:

	/** Constructor */
	XML_Updater();

	/** Destructor */
	~XML_Updater();

protected:
	/** Construct the updater object, and prepare the URL and callback message*/
	OP_STATUS Construct(URL &url, OpMessage fin_msg, MessageHandler *mh=NULL);

	/** The file is ready for processing */
	virtual OP_STATUS ResourceLoaded(URL &url);

	/** Process the loaded file. Implemented in the application subclass.
	 *	Must complete with OpStatus::OK or an error code 
	 *	the parser member is initialized when this function is entered
	 */
	virtual OP_STATUS ProcessFile()=0;

	/** Get a named Boolean Flag from the current XML parser level */
	BOOL GetFlag(const OpStringC &id);

	/** Get the text data of the current XML parser level */
	OP_STATUS GetTextData(OpString &target);
	/** Get the text data of the current XML parser level and return it as Base64 decoded content */
	OP_STATUS GetBase64Data(DataStream_ByteArray_Base &target);
	/** Get the text data of the current XML parser level and return it as hexadecimal decoded content */
	OP_STATUS GetHexData(DataStream_ByteArray_Base &target);

#ifdef _NATIVE_SSL_SUPPORT_
	/** Get the text data of the current XML parser level and return it as Base64 decoded content */
	OP_STATUS GetBase64Data(SSL_varvector32 &target);
	/** Get the text data of the current XML parser level and return it as hexadecimal decoded content */
	OP_STATUS GetHexData(SSL_varvector32 &target);
	OP_STATUS GetHexData(SSL_varvector24 &target);
#endif

	/** Verifies digital signatures on the XML file. Applications need only define
	 *	an override if they require a format that is different from the format used 
	 *	by CryptoVerifySignedTextFile(). If an override is defined the optional key parameters in 
	 *  SetVerifyFile MUST NOT be used (they won't be used/available).
	 */
	virtual BOOL VerifyFile(URL &url);

public:

	/** Should the content be verified? If verification is enabled either the default will be used or key and alg must be defined, or an alternative Verify function declared by implementations */
	void SetVerifyFile(BOOL flag, const byte *a_pub_sigkey=NULL, uint32 a_pub_sigkey_len=0, UpdaterSigAlg a_alg= (UpdaterSigAlg) 0){verify_file = flag; if(a_pub_sigkey != NULL){pub_sigkey=a_pub_sigkey;pub_sigkey_len = a_pub_sigkey_len; pub_sigalg = a_alg;}}

};
#endif

#endif // _XML_UPDATE_H_

