/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && (defined(VEGA_3DDEVICE) || defined(CANVAS3D_SUPPORT))

#include "modules/libvega/vega3ddevice.h"

// The component copy templates need some info akin to std::numeric_limits<T> so we just implement what we need here.
template<typename T> inline T maxval() { return CHAR_MAX; };
template<> inline unsigned char maxval() { return UCHAR_MAX; }
template<> inline short maxval() { return SHRT_MAX; }
template<> inline unsigned short maxval() { return USHRT_MAX; }
template<typename T> inline bool issigned() { return true; }
template<> inline bool issigned<unsigned char>() { return false; }
template<> inline bool issigned<unsigned short>() { return false; }

// Step any pointer in byte increments.
template <typename T>
inline void IncPointerBytes(T *&ptr, unsigned int bytes)
{
	ptr = (T*)(((char *)ptr) + bytes);
}

// Copy a component over as is without modifying it except for zero-striding it in the destination.
void CopyVertexComponent(void *inBuffer, unsigned int inStride, void *outBuffer, unsigned int outSize, unsigned int maxVertices)
{
	if (inStride == outSize)
		op_memcpy(outBuffer, inBuffer, outSize * maxVertices);
	else
	{
		unsigned char *in = (unsigned char*)inBuffer;
		unsigned char *out = (unsigned char *)outBuffer;

		for (unsigned int i = 0; i < maxVertices; ++i)
		{
			for (unsigned int b = 0; b < outSize; ++b)
				*out++ = in[b];
			in += inStride;
		}
	}
}

// Copy a component over, statically casting it to the right type and widen it to match the target
// number of components by filling it with zeroes except element four which will be set to one.
template <typename IN_TYPE, unsigned int IN_COMP, typename OUT_TYPE, unsigned int OUT_COMP, bool NORM>
void ConvertVertexComponentCast(void *inBuffer, unsigned int inStride, void *outBuffer, unsigned int maxVertices)
{
	const IN_TYPE *in = (const IN_TYPE*)inBuffer;
	OUT_TYPE *out = (OUT_TYPE *)outBuffer;

	const OUT_TYPE one = NORM ? maxval<OUT_TYPE>() : OUT_TYPE(1);
	for (unsigned int i = 0; i < maxVertices; ++i)
	{
		for (unsigned int c = 0; c < IN_COMP; ++c)
			*out++ = static_cast<OUT_TYPE>(in[c]);
		for (unsigned int c = IN_COMP; c < OUT_COMP; ++c)
			*out++ = c == 3 ? one : OUT_TYPE(0);
		IncPointerBytes(in, inStride);
	}
}

// Copy a component over but normalize it to a float. No need to widen.
template <typename IN_TYPE, unsigned int IN_COMP>
void ConvertVertexComponentNormFloat(void *inBuffer, unsigned int inStride, void *outBuffer, unsigned int maxVertices)
{
	const IN_TYPE *in = (const IN_TYPE*)inBuffer;
	float *out = (float *)outBuffer;
	const IN_TYPE one = maxval<IN_TYPE>();

	const float div = 1.0f/(2*static_cast<float>(maxval<IN_TYPE>())+1);
	for (unsigned int i = 0; i < maxVertices; ++i)
	{
		for (unsigned int c = 0; c < IN_COMP; ++c)
		{
			float f = static_cast<float>(in[c]);
			if (issigned<IN_TYPE>())
				*out++ = (2 * f + 1) * div;
			else
				*out++ = f / one;
		}
		IncPointerBytes(in, inStride);
	}
}

void CopyVertexComponent(void *inPtr, unsigned int inStride, VEGA3dVertexLayout::VertexType inType, void *outPtr, unsigned int outStride, VEGA3dVertexLayout::VertexType outType, unsigned int maxVertices, bool normalize)
{
	// Identity, copy across as is.
	if (inType == outType)
		CopyVertexComponent(inPtr, inStride, outPtr, outStride, maxVertices);

	// Non-normalized USHORT1-4 --> FLOAT1-4
	// Normalized USHORT1 --> USHORT2
	// Normalized USHORT3 --> USHORT4
	else if (inType == VEGA3dVertexLayout::USHORT1 && outType == VEGA3dVertexLayout::FLOAT1 && !normalize)
		ConvertVertexComponentCast<unsigned short, 1, float, 1, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::USHORT2 && outType == VEGA3dVertexLayout::FLOAT2 && !normalize)
		ConvertVertexComponentCast<unsigned short, 2, float, 2, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::USHORT3 && outType == VEGA3dVertexLayout::FLOAT3 && !normalize)
		ConvertVertexComponentCast<unsigned short, 3, float, 3, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::USHORT4 && outType == VEGA3dVertexLayout::FLOAT4 && !normalize)
		ConvertVertexComponentCast<unsigned short, 4, float, 4, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::USHORT1 && outType == VEGA3dVertexLayout::USHORT2 && normalize)
		ConvertVertexComponentCast<unsigned short, 1, unsigned short, 2, true>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::USHORT3 && outType == VEGA3dVertexLayout::USHORT4 && normalize)
		ConvertVertexComponentCast<unsigned short, 3, unsigned short, 4, true>(inPtr, inStride, outPtr,maxVertices);

	// Non-normalized SHORT1-4 --> FLOAT1-4
	// Normalized SHORT1 --> SHORT2
	// Normalized SHORT3 --> SHORT4
	else if (inType == VEGA3dVertexLayout::SHORT1 && outType == VEGA3dVertexLayout::FLOAT1 && !normalize)
		ConvertVertexComponentCast<short, 1, float, 1, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::SHORT3 && outType == VEGA3dVertexLayout::FLOAT2 && !normalize)
		ConvertVertexComponentCast<short, 2, float, 2, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::SHORT3 && outType == VEGA3dVertexLayout::FLOAT3 && !normalize)
		ConvertVertexComponentCast<short, 3, float, 3, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::SHORT4 && outType == VEGA3dVertexLayout::FLOAT4 && !normalize)
		ConvertVertexComponentCast<short, 4, float, 4, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::SHORT1 && outType == VEGA3dVertexLayout::SHORT2 && normalize)
		ConvertVertexComponentCast<short, 1, short, 2, true>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::SHORT3 && outType == VEGA3dVertexLayout::SHORT4 && normalize)
		ConvertVertexComponentCast<short, 3, short, 4, true>(inPtr, inStride, outPtr, maxVertices);

	// Non-normalized UBYTE1-4 --> FLOAT1-4
	// Normalized UBYTE1-3 --> UBYTE4
	else if (inType == VEGA3dVertexLayout::UBYTE1 && outType == VEGA3dVertexLayout::FLOAT1 && !normalize)
		ConvertVertexComponentCast<unsigned char, 1, float, 1, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::UBYTE2 && outType == VEGA3dVertexLayout::FLOAT2 && !normalize)
		ConvertVertexComponentCast<unsigned char, 2, float, 2, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::UBYTE3 && outType == VEGA3dVertexLayout::FLOAT3 && !normalize)
		ConvertVertexComponentCast<unsigned char, 3, float, 3, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::UBYTE4 && outType == VEGA3dVertexLayout::FLOAT4 && !normalize)
		ConvertVertexComponentCast<unsigned char, 4, float, 4, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::UBYTE1 && outType == VEGA3dVertexLayout::UBYTE4 && normalize)
		ConvertVertexComponentCast<unsigned char, 1, unsigned char, 4, true>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::UBYTE2 && outType == VEGA3dVertexLayout::UBYTE4 && normalize)
		ConvertVertexComponentCast<unsigned char, 2, unsigned char, 4, true>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::UBYTE3 && outType == VEGA3dVertexLayout::UBYTE4 && normalize)
		ConvertVertexComponentCast<unsigned char, 3, unsigned char, 4, true>(inPtr, inStride, outPtr, maxVertices);

	// Non-normalized BYTE1-4 --> FLOAT1-4
	// Normalized BYTE1-4 --> BYTE4
	else if (inType == VEGA3dVertexLayout::BYTE1 && outType == VEGA3dVertexLayout::FLOAT1 && !normalize)
		ConvertVertexComponentCast<char, 1, float, 1, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::BYTE2 && outType == VEGA3dVertexLayout::FLOAT2 && !normalize)
		ConvertVertexComponentCast<char, 2, float, 2, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::BYTE3 && outType == VEGA3dVertexLayout::FLOAT3 && !normalize)
		ConvertVertexComponentCast<char, 3, float, 3, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::BYTE4 && outType == VEGA3dVertexLayout::FLOAT4 && !normalize)
		ConvertVertexComponentCast<char, 4, float, 4, false>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::BYTE1 && outType == VEGA3dVertexLayout::BYTE4 && normalize)
		ConvertVertexComponentCast<char, 1, char, 4, true>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::BYTE2 && outType == VEGA3dVertexLayout::BYTE4 && normalize)
		ConvertVertexComponentCast<char, 2, char, 4, true>(inPtr, inStride, outPtr, maxVertices);
	else if (inType == VEGA3dVertexLayout::BYTE3 && outType == VEGA3dVertexLayout::BYTE4 && normalize)
		ConvertVertexComponentCast<char, 3, char, 4, true>(inPtr, inStride, outPtr, maxVertices);
	else
	{
		OP_ASSERT(false); // Conversion not implemented!
	}
}

#endif
