/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _CRYPTO_METHOD_DISABLE_H_
#define _CRYPTO_METHOD_DISABLE_H_

/* !!!!!NOTE!!!!!NOTE!!!!!NOTE!!!!!NOTE!!!!!NOTE!!!!!
 * The value of the constants MUST NOT be changed
 * 0x80000000 is reserved (it is negative)
 * !!!!!NOTE!!!!!NOTE!!!!!NOTE!!!!!NOTE!!!!!NOTE!!!!!
 */

/** Warn about MD2/MD5 use in certificate */
#define CRYPTO_METHOD_WARN_MD5			0x01
/** Completely disable MD2/MD5 use in certificates */
#define CRYPTO_METHOD_DISABLE_MD5		0x02

/** Warn about SHA-1 use in certificate */
#define CRYPTO_METHOD_WARN_SHA1			0x04
/** Completely disable SHA-1 use in certificates */
#define CRYPTO_METHOD_DISABLE_SHA1		0x08

/** Warn about sites using SSLv3-only servers */
#define CRYPTO_METHOD_WARN_SSLV3		0x10
/** Completely disable the SSL v3 protocol */
#define CRYPTO_METHOD_DISABLE_SSLV3		0x20

/** Completely disable MD2 */
#define CRYPTO_METHOD_DISABLE_MD2		0x40

/** Disable EV indication for servers that does not support RENEG indication */
#define CRYPTO_METHOD_RENEG_DISABLE_EV	0x80

/** Warn about servers that does not support RENEG indication */
#define CRYPTO_METHOD_RENEG_WARN		0x0100

/** Refuse to connect to servers that does not support RENEG indication */
#define CRYPTO_METHOD_RENEG_REFUSE		0x0200

/** Remove padlock for servers that does not support RENEG indication */
#define CRYPTO_METHOD_RENEG_DISABLE_PADLOCK		0x0400

/** Disable support for renegotiation for servers that does not support RENEG indication */
#define CRYPTO_METHOD_RENEG_DISABLE_UNPATCHED_RENEGO		0x0800

#endif // _CRYPTO_METHOD_DISABLE_H_
