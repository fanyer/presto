/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/


#ifndef __MIME_LIB_H__
#define __MIME_LIB_H__


#ifndef _MIME_SEC_ENABLED_
#if defined(_SUPPORT_SMIME_) || defined(_SUPPORT_OPENPGP_)
#define _MIME_SEC_ENABLED_
#endif
#endif

#ifdef _MIME_SEC_ENABLED_

#include "modules/datastream/fl_lib.h"

uint32 ScanForEOL(const unsigned char *buf, uint32 len);

class Hash_Item : public DataStream
{
	// This class is a sink
public:
	SSL_HashAlgorithmType alg;
	SSL_Hash	*hasher;
	uint32		count;

public:
	Hash_Item(SSL_HashAlgorithmType _alg, Hash_Item *org);
	Hash_Item(SSL_HashAlgorithmType alg);
	~Hash_Item();
	uint32 GetByteCount(){return count;}

	Hash_Item *Suc(){return (Hash_Item *) Link::Suc();}
	Hash_Item *Pred(){return (Hash_Item *) Link::Pred();}

	void InitHash();
	void HashData(const unsigned char *buffer, unsigned long len);
	void ExtractL(SSL_varvector32 &digest);

public:
	virtual void	WriteDataL(const unsigned char *buffer, unsigned long len);

#ifdef _PGP_STREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "Hash_Item";};
	virtual const char *Debug_OutputFilename(){return "stream.txt";};
#endif
};

class Hash_Head :  public DataStream, public Head
{
private:
	BOOL binary;
	SSL_secure_varvector32 text_buffer;

public:
	Hash_Head();
	~Hash_Head();

	void CopyL(Hash_Head *src);

	void AddMethodL(SSL_HashAlgorithmType _alg);
	Hash_Item *GetDigest(SSL_HashAlgorithmType _alg);

	void SetBinary(BOOL flag){binary = flag; text_buffer.Resize(0);}
	void HashDataL(const unsigned char *buffer, unsigned long len);

	Hash_Item *First(){return (Hash_Item *) Head::First();};
	Hash_Item *Last(){return (Hash_Item *) Head::Last();};

public:
	virtual void	WriteDataL(const unsigned char *buffer, unsigned long len);

private:
	void HashDataStep(const unsigned char *buffer, unsigned long len);

#ifdef _PGP_STREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "Hash_Head";};
	virtual const char *Debug_OutputFilename(){return "stream.txt";};
#endif
};

class MIME_Signed_Processor
{
private:
	Hash_Head digests;
	
protected:
	MIME_Signed_Processor();
	~MIME_Signed_Processor();
	
public:
	void AddDigestAlgL(SSL_HashAlgorithmType alg);
	Hash_Head &AccessDigests(){return digests;}
	
protected:
	void PerformDigestProcessingL(const unsigned char *src, unsigned long src_len);
};


#endif // _MIME_SEC_ENABLED_

#endif

