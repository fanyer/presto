/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef UTIL_HASH_H
#define UTIL_HASH_H

/**
 * Calculate djb2 hash of a nul-terminated string.
 *
 * @param s NUL-terminated string to hash.
 * @return djb2 hashed value of the string.
 */
unsigned int djb2hash(const char* s);

/** @overload */
unsigned int djb2hash(const uni_char* s);

/**
 * Calculate djb2 hash of a nul-terminated string, ignoring letter casing.
 *
 * @param s NUL-terminated string to hash.
 * @return djb2 hashed value of the string.
 */
unsigned int djb2hash_nocase(const char* s);

/** @overload */
unsigned int djb2hash_nocase(const uni_char* s);

/**
 * Calculate djb2 hash of a non-nul-terminated string.
 *
 * @param s String to hash.
 * @param len Length of the string.
 * @return djb2 hashed value of the string.
 */
unsigned int djb2hash(const char* s, size_t len);

/** @overload */
unsigned int djb2hash(const uni_char* s, size_t len);

/**
 * Calculate djb2 hash of a non-nul-terminated string, ignoring letter casing.
 *
 * @param s String to hash.
 * @param len Length of the string.
 * @return djb2 hashed value of the string.
 */
unsigned int djb2hash_nocase(const char* s, size_t len);

/** @overload */
unsigned int djb2hash_nocase(const uni_char* s, size_t len);

#endif // UTIL_HASH_H
