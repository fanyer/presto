/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CRYPTO_UTILITY_H_
#define CRYPTO_UTILITY_H_

class CryptoUtility
{
public:
	/**
	 * Converts the specified array of source bytes into a string with a base 64
	 * encoded representation of the source array. You can use
	 * ConvertFromBase64() to convert the base 64 encoded string into byte
	 * array.
	 * @param[in]  source        Is the byte array to convert to a string with
	 *                           a base 64 encoding of this data.
	 * @param[in]  source_length Is the number of source bytes to convert.
	 * @param[out] base_64       Receives on success the base 64 encoded string.
	 *
	 * @retval OpStatus::OK      On Success.
	 * @retval OpStatus::ERR     In case of an error.
	 *
	 * @see ConvertFromBase64()
	 */
	static OP_STATUS ConvertToBase64(const UINT8 *source, int source_length, OpString8 &base_64);

	/**
	 * Converts the specified string from base 64 encoding into a byte array.
	 * @param[in]  source    Is the string containing base 64 encoded data.
	 * @param[out] binary    Receives on success a newly allocated byte-array
	 *                       that contains the data of the specified source. The
	 *                       caller needs to free this byte-array by calling
	 *                       OP_DELETEA(binary).
	 * @param[out] length    Receives on success the number of bytes in the
	 *                       returned byte-array.
	 *
	 * @retval OpStatus::OK            On Success.
	 * @retval OpStatus::ERR_NO_MEMORY If there is not enough memory to allocate
	 *                                 the byte-array.
	 * @retval OpStatus::ERR           In case of any other error.
	 *
	 * @see ConvertToBase64()
	 */
	static OP_STATUS ConvertFromBase64(const char *source, UINT8 *&binary, int& length);
};

#endif /* CRYPTO_UTILITY_H_ */
