/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_SIGHASH_H
#define LIBSSL_SIGHASH_H

#if defined _NATIVE_SSL_SUPPORT_

class TLS_SigAndHash : public LoadAndWritableList
{
private:
    SSL_SignatureAlgorithm alg;
	DataStream_IntegralType<SSL_HashAlgorithmType, 1> hash_alg;
	DataStream_IntegralType<SSL_SignatureAlgorithm, 1> sign_alg; 

private:
	void InternalInit();
	void SetValue(SSL_SignatureAlgorithm p_alg);

public:
	TLS_SigAndHash();
	TLS_SigAndHash(SSL_SignatureAlgorithm p_alg);
	TLS_SigAndHash(const TLS_SigAndHash &);

	operator SSL_SignatureAlgorithm() const {return alg;}

	TLS_SigAndHash &operator =(SSL_SignatureAlgorithm p_alg){SetValue(p_alg);return *this;}
	TLS_SigAndHash &operator =(const TLS_SigAndHash &old){SetValue(old);return *this;};
	BOOL operator ==(SSL_SignatureAlgorithm p_alg){return alg == p_alg;}
	BOOL operator ==(const TLS_SigAndHash &old){SSL_SignatureAlgorithm p_alg=old;return alg == p_alg;};

	virtual void	PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);
};

#if defined(_SUPPORT_TLS_1_2)
typedef SSL_LoadAndWriteableListType<TLS_SigAndHash, 2, 0xffff> TLS_SignatureAlgListBase;

class TLS_SignatureAlgList : public TLS_SignatureAlgListBase
{
public:
	void Set(const SSL_SignatureAlgorithm *, uint16 count);
	void GetCommon(TLS_SignatureAlgListBase &master, const SSL_SignatureAlgorithm *preferred, uint16 count);
};
#endif

#endif
#endif // LIBSSL_SIGHASH_H
