/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_ENC_OPSTRING_ENCODINGS_H
#define MODULES_ENC_OPSTRING_ENCODINGS_H

/**
 * @file opstring-encodings.h
 * Functions for manipulating OpString8 and OpString16 objects with legacy
 * encodings.
 */

/* Forward declarations */
class OpString8;
class OpString16;
class InputConverter;
class OutputConverter;

/**
  * Reallocates the contents of the string to a representation of
  * a UTF-16 string in any encoding.
  *
  * NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE!
  *
  * This may make the OpString8 contain a string whose length, measured in
  * bytes, is not the number of characters it encodes.
  *
  * NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE!
  *
  * An OpString8 containing legacy data can not be converted to
  * an OpString16 in the usual manner SetFromEncoding()
  * has to be used.
  *
  * NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE!
  *
  * @param target The string where the result should go.
  * @param aEncoding The name of the encoding of the output string.
  * @param aUTF16str The string to be converted to the target encoding.
  * @param aLength KAll to read the whole aUTF16str, else the number of
  * UTF-16 characters to read from aUTF16str.
  * @return The number of unconvertible character entities in the output.
  * A result of zero means that all of the input string could be
  * converted successfully.
  */
int SetToEncodingL(OpString8 *target, const char *aEncoding, const uni_char *aUTF16str, int aLength=KAll);
/** @overload */
int SetToEncodingL(OpString8 *target, const OpStringC8 &aEncoding, const OpStringC16 &aUTF16str, int aLength=KAll);

/**
  * Reallocates the contents of the string to a representation of
  * a UTF-16 string in any encoding.
  *
  * NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE!
  *
  * This may make the OpString8 contain a string whose length, measured in
  * bytes, is not the number of characters it encodes.
  *
  * NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE!
  *
  * An OpString8 containing legacy data can not be converted to
  * an OpString16 in the usual manner SetFromEncoding()
  * has to be used.
  *
  * NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE!
  *
  * @param target The string where the result should go.
  * @param aEncoder A suitable OutputConverter that can convert the
  * input string from UTF-16. This encoder must be deleted by the caller.
  * @param aUTF16str The string to be converted to the target encoding.
  * @param aLength KAll to read the whole aUTF16str, else the number of
  * UTF-16 characters to read from aUTF16str.
  * @return The number of unconvertible character entities in the output.
  * A result of zero means that all of the input string could be
  * converted successfully.
  */
int SetToEncodingL(OpString8 *target, OutputConverter *aEncoder, const uni_char *aUTF16str, int aLength=KAll);
/** @overload */
int SetToEncodingL(OpString8 *target, OutputConverter *aEncoder, const OpStringC16 &aUTF16str, int aLength=KAll);

/**
  * Reallocates the contents of the string with the contents of
  * a character string (octet stream) in any encoding.
  *
  * @param target The string where the result should go.
  * @param aEncoding The name of the encoding of the input string.
  * @param aEncodedStr The character string in the specified encoding.
  * @param aByteLength Length (in bytes) of the input string.
  * @param aInvalidInputs number of invalid inputs in the input string. A result
  * of zero means that the input string did not contain any invalid
  * inputs.
  */
OP_STATUS SetFromEncoding(OpString16 *target, const char *aEncoding, const void *aEncodedStr, int aByteLength, int* aInvalidInputs = NULL);
/** @overload */
OP_STATUS SetFromEncoding(OpString16 *target, const OpStringC8 &aEncoding, const void *aEncodedStr, int aByteLength, int* aInvalidInputs = NULL);

/**
  * Reallocates the contents of the string with the contents of
  * a character string (octet stream) in any encoding.
  *
  * @param target The string where the result should go.
  * @param aEncoding The name of the encoding of the input string.
  * @param aEncodedStr The character string in the specified encoding.
  * @param aByteLength Length (in bytes) of the input string.
  * @return number of invalid inputs in the input string. A result
  * of zero means that the input string did not contain any invalid
  * inputs.
  */
int SetFromEncodingL(OpString16 *target, const char *aEncoding, const void *aEncodedStr, int aByteLength);
/** @overload */
int SetFromEncodingL(OpString16 *target, const OpStringC8 &aEncoding, const void *aEncodedStr, int aByteLength);

/**
  * Reallocates the contents of the string with the contents of
  * a character string (octet stream) in any encoding.
  *
  * @param target The string where the result should go.
  * @param aDecoder A suitable InputConverter that can convert the
  * input string to UTF-16. This decoder must be deleted by the caller.
  * @param aEncodedStr The character string in the specified encoding.
  * @param aByteLength Length (in bytes) of the input string.
  * @param aInvalidInputs The number of invalid inputs in the input string. A result
  * of zero means that the input string did not contain any invalid
  * inputs.
  */
OP_STATUS SetFromEncoding(OpString16 *target, InputConverter *aDecoder, const void *aEncodedStr, int aByteLength, int* aInvalidInputs = NULL);

/**
  * Reallocates the contents of the string with the contents of
  * a character string (octet stream) in any encoding.
  *
  * @param target The string where the result should go.
  * @param aDecoder A suitable InputConverter that can convert the
  * input string to UTF-16. This decoder must be deleted by the caller.
  * @param aEncodedStr The character string in the specified encoding.
  * @param aByteLength Length (in bytes) of the input string.
  * @return The number of invalid inputs in the input string. A result
  * of zero means that the input string did not contain any invalid
  * inputs.
  */
int SetFromEncodingL(OpString16 *target, InputConverter *aDecoder, const void *aEncodedStr, int aByteLength);

inline int SetToEncodingL(OpString8 *target, const OpStringC8 &aEncoding, const OpStringC16 &aUTF16str, int aLength)
{
	return SetToEncodingL(target, aEncoding.CStr(), aUTF16str.CStr(), aLength);
}

inline int SetToEncodingL(OpString8 *target, OutputConverter *aEncoder, const OpStringC16 &aUTF16str, int aLength)
{
	return SetToEncodingL(target, aEncoder, aUTF16str.CStr(), aLength);
}

inline OP_STATUS SetFromEncoding(OpString16 *target, const OpStringC8 &aEncoding, const void *aEncodedStr, int aByteLength, int* aInvalidInputs)
{
	return SetFromEncoding(target, aEncoding.CStr(), aEncodedStr, aByteLength, aInvalidInputs);
}

inline int SetFromEncodingL(OpString16 *target, const OpStringC8 &aEncoding, const void *aEncodedStr, int aByteLength)
{
	return SetFromEncodingL(target, aEncoding.CStr(), aEncodedStr, aByteLength);
}

/** Size in bytes of work buffer for set from/to encoding */
#define OPSTRING_ENCODE_WORKBUFFER_SIZE 1024

#endif // MODULES_ENC_OPSTRING_ENCODINGS_H
