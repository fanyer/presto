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

#ifndef _SSLENUM2_H_	// {
#define _SSLENUM2_H_

enum SSL_HashAlgorithmType {
	SSL_NoHash=0, 
	SSL_MD5=1, 
	SSL_SHA=2,
	SSL_SHA_224=3,
	SSL_SHA_256=4,
	SSL_SHA_384=5,
	SSL_SHA_512=6,
	SSL_max_hash_num = 0xff,
	SSL_RIPEMD160,
	SSL_HMAC_MD5, 
	SSL_HMAC_SHA,
	SSL_HMAC_RIPEMD160,
	SSL_MD5_SHA,
	SSL_HMAC_SHA_224,
	SSL_HMAC_SHA_256,
	SSL_HMAC_SHA_384,
	SSL_HMAC_SHA_512,
	SSL_HASH_MAX_NUM
};     

#ifdef _SUPPORT_TLS_1_2
enum TLS_PRF_func {
	TLS_PRF_default
};
#endif

#endif	// _SSLENUM2_H_
