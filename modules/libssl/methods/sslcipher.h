/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef __SSLCIPHER_H
#define __SSLCIPHER_H

#ifdef _NATIVE_SSL_SUPPORT_

class SSL_Cipher: public SSL_Error_Status
{
public:
#ifdef USE_SSL_PGP_CFB_MODE
	enum PGP_CFB_Mode_Type {
		PGP_CFB_Mode_Disabled,
		PGP_CFB_Mode_Enabled,
		PGP_CFB_Mode_Enabled_No_Resync
	};
#endif
protected:
	SSL_PADDING_strategy appendpad;

    SSL_BulkCipherType cipher_alg;
	SSL_CipherType	cipher_type;

	uint16 in_block_size;
	uint16 out_block_size;
   
public:
	SSL_Cipher():appendpad(SSL_NO_PADDING), cipher_alg(SSL_NoCipher), cipher_type(SSL_StreamCipher),in_block_size(1),out_block_size(1) {}
	SSL_CipherType CipherType() const{return cipher_type;} 
    SSL_BulkCipherType CipherID() const{return cipher_alg;}
	virtual void SetPaddingStrategy(SSL_PADDING_strategy);
	virtual SSL_PADDING_strategy GetPaddingStrategy();

    virtual void InitEncrypt()=0;
    virtual byte *Encrypt(const byte *source,uint32 len,byte *target, uint32 &,uint32 bufferlen)=0;
    void EncryptVector(const SSL_varvector32 &, SSL_varvector32 &);
    virtual byte *FinishEncrypt(byte *target, uint32 &, uint32)=0;

    virtual void InitDecrypt()=0;
    virtual byte *Decrypt(const byte *source,uint32 len,byte *target, uint32 &,uint32 bufferlen)=0; 
    void DecryptVector(const SSL_varvector32 &source, SSL_varvector32 &target);
    virtual byte *FinishDecrypt(byte *target, uint32 &,uint32 bufferlen)=0;

#ifdef USE_SSL_PGP_CFB_MODE
	virtual void CFB_Resync();
	virtual void SetPGP_CFB_Mode(SSL_Cipher::PGP_CFB_Mode_Type mode);
    virtual void UnloadPGP_Prefix(SSL_varvector32 &);    
#endif

	/** InitDecrypt must be called first, FinishDecrypt called if source has no MoreData */
    uint32 DecryptStreamL(DataStream *source, DataStream *target, uint32 len=0);

	/** InitEncrypt must be called first, FinishDecrypt is called if source is NULL */
    uint32 EncryptStreamL(DataStream *source, DataStream *target, uint32 len=0);

	uint16 InputBlockSize() const{return in_block_size;}
    uint16 OutputBlockSize() const{return out_block_size;};
    uint32 Calc_BufferSize(uint32);
}; 

class SSL_GeneralCipher: public SSL_Cipher
{
protected:
	uint16 key_size;
	uint16 iv_size;
public:
	SSL_GeneralCipher():key_size(0), iv_size(0) {}
	virtual void SetCipherDirection(SSL_CipherDirection dir)=0;
	virtual uint16 KeySize() const{return key_size;}
	virtual uint16 IVSize() const{return iv_size;}
    virtual const byte *LoadKey(const byte *)=0;
    virtual const byte *LoadIV(const byte *)=0;
    virtual void BytesToKey(SSL_HashAlgorithmType, const SSL_varvector32 &string,  const SSL_varvector32 &salt, int count)= 0;
}; 
#endif
#endif
