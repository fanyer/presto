/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKEND_DIRECT3D10

#include <d3d10_1.h>
// If WebGL is enabled we need both the DXSDK and the D3DCompiler header. To find out if we
// have both we include the D3DCompiler header here. If the header isn't installed we will
// fall back and include our dummy D3DCompiler header which will define NO_DXSDK. That will
// in turn disable compilation of the DX backend.
#ifdef CANVAS3D_SUPPORT
#include <D3DCompiler.h>
#endif //CANVAS3D_SUPPORT
#ifndef NO_DXSDK

#include "platforms/vega_backends/d3d10/vegad3d10buffer.h"
#include "platforms/vega_backends/d3d10/vegad3d10shader.h"
#include "modules/libvega/vegavertexbufferutils.h"

static unsigned int sVEGAVertexTypeByteSize[] =
{
	16,//	FLOAT4
	12,//	FLOAT3
	8,//	FLOAT2
	4,//	FLOAT1
	8,//	USHORT4
	6,//	USHORT3
	4,//	USHORT2
	2,//	USHORT1
	8,//	SHORT4
	6,//	SHORT3
	4,//	SHORT2
	2,//	SHORT1
	4,//	UBYTE4
	3,//	UBYTE3
	2,//	UBYTE2
	1,//	UBYTE1
	4,//	BYTE4
	3,//	BYTE3
	2,//	BYTE2
	1 //	BYTE1
};

/* Maps input type and normalized flag to an output type.

Normalized component types can be mapped to the closest 4-byte aligned type,
but non normalized types will need to be normalized and converted to float
types as DX will reinterpret the bits as a float in the shader.

e.g.
D3D10: WARNING: ID3D10Device::CreateInputLayout: The provided input signature
expects to read an element with SemanticName/Index: 'TEXCOORD'/0 and component(s)
of the type 'float32'.  However, the matching entry in the Input Layout declaration,
element[0], specifies mismatched format: 'R8G8B8A8_SINT'.  This is not an error,
since behavior is well defined: The element format determines what data conversion
algorithm gets applied before it shows up in a shader register. Independently,
the shader input signature defines how the shader will interpret the data that has
been placed in its input registers, with no change in the bits stored.  It is valid
for the application to reinterpret data as a different type once it is in the
vertex shader, so this warning is issued just in case reinterpretation was not
intended by the author. [ STATE_CREATION WARNING #391: CREATEINPUTLAYOUT_TYPE_MISMATCH ]
*/
static VEGA3dVertexLayout::VertexType sDX10VertexTypeRemap[][2] =
{
	{VEGA3dVertexLayout::FLOAT4,  VEGA3dVertexLayout::FLOAT4},  // FLOAT4   unnormalized, normalized
	{VEGA3dVertexLayout::FLOAT3,  VEGA3dVertexLayout::FLOAT3},  // FLOAT3
	{VEGA3dVertexLayout::FLOAT2,  VEGA3dVertexLayout::FLOAT2},  // FLOAT2
	{VEGA3dVertexLayout::FLOAT1,  VEGA3dVertexLayout::FLOAT1},  // FLOAT1
	{VEGA3dVertexLayout::FLOAT4,  VEGA3dVertexLayout::USHORT4}, // USHORT4
	{VEGA3dVertexLayout::FLOAT3,  VEGA3dVertexLayout::USHORT4}, // USHORT3
	{VEGA3dVertexLayout::FLOAT2,  VEGA3dVertexLayout::USHORT2}, // USHORT2
	{VEGA3dVertexLayout::FLOAT1,  VEGA3dVertexLayout::USHORT2}, // USHORT1
	{VEGA3dVertexLayout::FLOAT4,  VEGA3dVertexLayout::SHORT4},  // SHORT4
	{VEGA3dVertexLayout::FLOAT3,  VEGA3dVertexLayout::SHORT4},  // SHORT3
	{VEGA3dVertexLayout::FLOAT2,  VEGA3dVertexLayout::SHORT2},  // SHORT2
	{VEGA3dVertexLayout::FLOAT1,  VEGA3dVertexLayout::SHORT2},  // SHORT1
	{VEGA3dVertexLayout::FLOAT4,  VEGA3dVertexLayout::UBYTE4},  // UBYTE4
	{VEGA3dVertexLayout::FLOAT3,  VEGA3dVertexLayout::UBYTE4},  // UBYTE3
	{VEGA3dVertexLayout::FLOAT2,  VEGA3dVertexLayout::UBYTE4},  // UBYTE2
	{VEGA3dVertexLayout::FLOAT1,  VEGA3dVertexLayout::UBYTE4},  // UBYTE1
	{VEGA3dVertexLayout::FLOAT4,  VEGA3dVertexLayout::BYTE4},   // BYTE4
	{VEGA3dVertexLayout::FLOAT3,  VEGA3dVertexLayout::BYTE4},   // BYTE3
	{VEGA3dVertexLayout::FLOAT2,  VEGA3dVertexLayout::BYTE4},   // BYTE2
	{VEGA3dVertexLayout::FLOAT1,  VEGA3dVertexLayout::BYTE4}    // BYTE1
};


VEGAD3d10Buffer::VEGAD3d10Buffer() : device(NULL), buffer(NULL), size(0), usage(DYNAMIC), unalignedBuffer(NULL), deferredUpdate(false), cachedType(CL_Invalid), cachedLayout(NULL), isIndexBuffer(false), currentOffset(0)
{}

VEGAD3d10Buffer::~VEGAD3d10Buffer()
{
	if (buffer)
		buffer->Release();
	if (unalignedBuffer)
		OP_DELETEA(unalignedBuffer);
}

OP_STATUS VEGAD3d10Buffer::Construct(ID3D10Device1* dev, unsigned int size, VEGA3dBuffer::Usage usage, bool isIndexBuffer, bool deferredUpdate)
{
	device = dev;

	this->size = size;
	this->usage = usage;
	this->deferredUpdate = deferredUpdate;
	this->isIndexBuffer = isIndexBuffer;

	// If we're using deferred update we copy the data to a buffer in RAM and update it later.
	if (deferredUpdate)
	{
		unalignedBuffer = OP_NEWA(char, size);
		RETURN_OOM_IF_NULL(unalignedBuffer);
		op_memset(unalignedBuffer, 0 , size);
		clearCached();
	}
	// Otherwise just upload it to VRAM.
	else
	{
		D3D10_BUFFER_DESC bdesc;
		bdesc.Usage = usage==STREAM_DISCARD?D3D10_USAGE_DYNAMIC:D3D10_USAGE_DEFAULT;
		bdesc.ByteWidth = size;
		bdesc.BindFlags = isIndexBuffer?D3D10_BIND_INDEX_BUFFER:D3D10_BIND_VERTEX_BUFFER;
		bdesc.CPUAccessFlags = usage==STREAM_DISCARD?D3D10_CPU_ACCESS_WRITE:0;
		bdesc.MiscFlags = 0;
		if (FAILED(dev->CreateBuffer(&bdesc, NULL, &buffer)))
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Buffer::writeAtOffset(unsigned int offset, unsigned int dataSize, void* data)
{
	if (deferredUpdate)
	{
		op_memcpy(unalignedBuffer + offset, data, dataSize);
		clearCached();
	}
	else
	{
		if (usage==STREAM_DISCARD)
		{
			UINT8* bufdata;
			buffer->Map(D3D10_MAP_WRITE_DISCARD, 0, (void**)&bufdata);
			op_memcpy(bufdata+offset, data, dataSize);
			buffer->Unmap();
		}
		else
		{
			D3D10_BOX box;
			box.left = offset;
			box.right = offset+dataSize;
			box.top = 0;
			box.bottom = 1;
			box.front = 0;
			box.back = 1;
			device->UpdateSubresource(buffer, 0, &box, data, 0, 0);
		}
	}
	currentOffset = offset + dataSize;
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Buffer::writeAnywhere(unsigned int num, unsigned int size, void* data, unsigned int& index)
{
	/* There is nothing wrong in itself with calling this
	 * method on a buffer which is not DISCARD.  However, it
	 * is believed that there is no sensible use case where
	 * this method would be called on a non-DISCARD buffer.
	 * So if that happens, it indicates that there probably is
	 * a sub-optimality somewhere.
	 *
	 * If you trigger this assert and believe that the call to
	 * this method is sensible, please tell the owner of this
	 * code about it.  He can probably be persuaded by a good
	 * example.
	 */
	OP_ASSERT(usage == STREAM_DISCARD);

	unsigned int updateSize = num * size;

	if (usage==STREAM_DISCARD)
	{
		// Align the index, so it can also be used with an index buffer retrived with getQuadIndexBuffer.
		index = (currentOffset + size - 1) / size;
		index = (index + 3) & (~3u);

		// Check that there is still enough space left in the buffer at this index.
		if (currentOffset != 0 && (index*size) + updateSize < this->size)
		{
			UINT8* bufdata;
			buffer->Map(D3D10_MAP_WRITE_NO_OVERWRITE, 0, (void**)&bufdata);
			op_memcpy(bufdata+(index*size), data, updateSize);
			buffer->Unmap();
			currentOffset = (index*size) + updateSize;
			return OpStatus::OK;
		}
	}

	index = 0;
	return writeAtOffset(0, updateSize, data);
}

void VEGAD3d10Buffer::CalculateBufferSizes(D3dPerVertexComponent *inputs,  D3dPerVertexComponent *aligned, unsigned int &alignedSize, unsigned int &maxVertices)
{
	maxVertices = 0xffffffff;
	if (aligned)
	{
		/* Calculate the maximum number of vertices we can fit in the buffer,
		   as determined by the components that use it. The component that
		   allows for the fewest vertices determines the maximum. */
		for (D3dPerVertexComponent* comp = inputs; comp; comp = comp->next)
			if (comp->buffer == this)
				maxVertices = MIN(maxVertices,
				                  maxVerticesForBuffer(getSize(), comp->offset, comp->stride,
				                                       sVEGAVertexTypeByteSize[comp->type]));

		/* Calculate the storage required for the reshuffled buffer, where each
		   component has its data in a separate continuous segment within the
		   buffer. */

		alignedSize = 0;
		for (D3dPerVertexComponent* comp = aligned; comp; comp = comp->next)
			if (comp->buffer == this)
				alignedSize += sVEGAVertexTypeByteSize[comp->type] * maxVertices;
	}
}


OP_STATUS VEGAD3d10Buffer::Sync(bool byteToShort)
{
	OP_ASSERT(isIndexBuffer);
	if (deferredUpdate && (cachedType != (byteToShort ? CL_ByteToShort : CL_Short) || !buffer))
	{
		if (buffer)
			buffer->Release();

		unsigned int alignedSize = byteToShort ? size * 2 : size;

		D3D10_BUFFER_DESC bdesc;
		bdesc.Usage = (usage == STREAM_DISCARD) ? D3D10_USAGE_DYNAMIC : D3D10_USAGE_DEFAULT;
		bdesc.ByteWidth = alignedSize;
		bdesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
		bdesc.CPUAccessFlags = (usage == STREAM_DISCARD) ? D3D10_CPU_ACCESS_WRITE : 0;
		bdesc.MiscFlags = 0;
		if (FAILED(device->CreateBuffer(&bdesc, NULL, &buffer)))
			return OpStatus::ERR;

		char *bufferPtr = unalignedBuffer;

		if (byteToShort)
		{
			bufferPtr = OP_NEWA(char, alignedSize);
			RETURN_OOM_IF_NULL(bufferPtr);
			char *src = unalignedBuffer;
			for (short *dst = (short *)bufferPtr; dst < (short *)(bufferPtr + alignedSize);)
				*dst++ = *src++;
		}

		if (usage == STREAM_DISCARD)
		{
			UINT8* bufdata;
			buffer->Map(D3D10_MAP_WRITE_DISCARD, 0, (void**)&bufdata);
			op_memcpy(bufdata, bufferPtr, alignedSize);
			buffer->Unmap();
		}
		else
		{
			D3D10_BOX box;
			box.left = 0;
			box.right = alignedSize;
			box.top = 0;
			box.bottom = 1;
			box.front = 0;
			box.back = 1;
			device->UpdateSubresource(buffer, 0, &box, bufferPtr, 0, 0);
		}

		if (byteToShort)
		{
			OP_DELETEA(bufferPtr);
		}

		clearCached(byteToShort ? CL_ByteToShort : CL_Short);
	}
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Buffer::Sync(D3dPerVertexComponent *inputs,  D3dPerVertexComponent *aligned, ID3D10InputLayout* layoutId)
{
	OP_ASSERT(!isIndexBuffer);
	if (deferredUpdate && (cachedType != CL_VertexLayout || cachedLayout != layoutId))
	{
		if (buffer)
			buffer->Release();

		unsigned int alignedSize = size;
		unsigned int maxVertices = 0xffffffff;
		CalculateBufferSizes(inputs, aligned, alignedSize, maxVertices);

		D3D10_BUFFER_DESC bdesc;
		bdesc.Usage = (usage == STREAM_DISCARD) ? D3D10_USAGE_DYNAMIC : D3D10_USAGE_DEFAULT;
		bdesc.ByteWidth = alignedSize;
		bdesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
		bdesc.CPUAccessFlags = (usage == STREAM_DISCARD) ? D3D10_CPU_ACCESS_WRITE : 0;
		bdesc.MiscFlags = 0;
		if (FAILED(device->CreateBuffer(&bdesc, NULL, &buffer)))
			return OpStatus::ERR;

		char *bufferPtr = unalignedBuffer;

		if (aligned)
		{
			bufferPtr = OP_NEWA(char, alignedSize);
			RETURN_OOM_IF_NULL(bufferPtr);

			D3dPerVertexComponent* in = inputs;
			D3dPerVertexComponent* out = aligned;
			while (in && out)
			{
				if (in->buffer == this)
				{
					char *inPtr = unalignedBuffer + in->offset;
					char *outPtr = bufferPtr + out->offset;
					CopyVertexComponent(inPtr, in->stride, in->type, outPtr, out->stride, out->type, maxVertices, out->normalize);
				}
				in = in->next;
				out = out->next;
			}
		}

		if (usage==STREAM_DISCARD)
		{
			UINT8* bufdata;
			buffer->Map(D3D10_MAP_WRITE_DISCARD, 0, (void**)&bufdata);
			op_memcpy(bufdata, bufferPtr, alignedSize);
			buffer->Unmap();
		}
		else
		{
			D3D10_BOX box;
			box.left = 0;
			box.right = alignedSize;
			box.top = 0;
			box.bottom = 1;
			box.front = 0;
			box.back = 1;
			device->UpdateSubresource(buffer, 0, &box, bufferPtr, 0, 0);
		}

		if (aligned)
		{
			OP_DELETEA(bufferPtr);
		}

		cachedType = CL_VertexLayout;
		cachedLayout = layoutId; // Store what layout we've currently cached for.
	}
	return OpStatus::OK;
}

VEGAD3d10VertexLayout::VEGAD3d10VertexLayout(VEGA3dShaderProgram* sp) : m_components(NULL), m_num_components(0), m_num_streams(0)
	, m_d3dCompatibleComponents(NULL), m_d3d_layout(NULL), m_shader(sp), m_perInstanceComponents(NULL)
	, m_perInstanceBuffer(NULL), m_perInstanceBufferSize(0)
{
}

VEGAD3d10VertexLayout::~VEGAD3d10VertexLayout()
{
	releaseInputLayout();

	while (m_components)
	{
		D3dPerVertexComponent* nc = m_components->next;
		VEGARefCount::DecRef(m_components->buffer);
		OP_DELETE(m_components);
		m_components = nc;
	}
	while (m_d3dCompatibleComponents)
	{
		D3dPerVertexComponent* nc = m_d3dCompatibleComponents->next;
		OP_DELETE(m_d3dCompatibleComponents);
		m_d3dCompatibleComponents = nc;
	}
	while (m_perInstanceComponents)
	{
		D3dPerInstanceComponent* nc = m_perInstanceComponents->next;
		OP_DELETE(m_perInstanceComponents);
		m_perInstanceComponents = nc;
	}
	if (m_perInstanceBuffer)
		m_perInstanceBuffer->Release();
}

void VEGAD3d10VertexLayout::releaseInputLayout()
{
	if (m_d3d_layout)
	{
		m_d3d_layout->Release();
		m_d3d_layout = NULL;
		for (D3dPerVertexComponent* c = m_components; c; c = c->next)
			if (c->buffer)
				static_cast<VEGAD3d10Buffer *>(c->buffer)->clearCached();
	}
}

OP_STATUS VEGAD3d10VertexLayout::addComponent(VEGA3dBuffer* buffer, unsigned int index, unsigned int offset, unsigned int stride, VertexType type, bool normalize)
{
	releaseInputLayout();
	while (m_d3dCompatibleComponents)
	{
		D3dPerVertexComponent* nc = m_d3dCompatibleComponents->next;
		OP_DELETE(m_d3dCompatibleComponents);
		m_d3dCompatibleComponents = nc;
	}

	D3dPerVertexComponent* new_comp = OP_NEW(D3dPerVertexComponent, ());
	RETURN_OOM_IF_NULL(new_comp);
	new_comp->buffer = buffer;
	new_comp->stream = m_num_streams;
	new_comp->index = index;
	new_comp->offset = offset;
	new_comp->stride = stride;
	new_comp->type = type;
	new_comp->normalize = normalize;
	VEGARefCount::IncRef(buffer);

	D3dPerVertexComponent *firstOfStream = new_comp;

	D3dPerVertexComponent **nextpp;
	for (nextpp = &m_components; *nextpp; nextpp = &(*nextpp)->next)
	{
		D3dPerVertexComponent *c = *nextpp;
		// Components with the same buffer and stride go in the same stream
		if (c->buffer == buffer && c->stride == stride)
		{
			// This flags that 'firstOfStream' needs to be updated for the
			// component; see below.
			c->firstOfStream = NULL;
			new_comp->stream = c->stream;
			if (c->offset < firstOfStream->offset)
				firstOfStream = c;
		}
	}
	*nextpp = new_comp;

	if (new_comp->stream == m_num_streams)
	{
		// 'comp' did not match the buffer/stride of any other component and
		// went in a new stream
		++m_num_streams;
	}
	++m_num_components;

	for (D3dPerVertexComponent* c = m_components; c; c = c->next)
		if (!c->firstOfStream)
			c->firstOfStream = firstOfStream;

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10VertexLayout::addConstComponent(unsigned int index, float* values)
{
	D3dPerInstanceComponent* comp = OP_NEW(D3dPerInstanceComponent, ());
	if (!comp)
		return OpStatus::ERR_NO_MEMORY;
	comp->index = index;
	comp->values[0] = values[0];
	comp->values[1] = values[1];
	comp->values[2] = values[2];
	comp->values[3] = values[3];
	comp->perInstance = true;
	comp->next = m_perInstanceComponents;
	m_perInstanceComponents = comp;

	// Set up the firstOfStream, making it always point to the lowest offset component.
	D3dComponent* firstOfStream = m_perInstanceComponents;
	for (D3dPerInstanceComponent* c = m_perInstanceComponents->next; c; c = c->next)
		if (c->offset < firstOfStream->offset)
			firstOfStream = c;
	unsigned int offset = 0;
	for (D3dPerInstanceComponent* c = m_perInstanceComponents; c; c = c->next)
	{
		c->firstOfStream = firstOfStream;
		c->offset = offset;
		offset += 4 * 4;
	}

	return OpStatus::OK;
}

#ifdef CANVAS3D_SUPPORT

OP_STATUS VEGAD3d10VertexLayout::addBinding(const char *name, VEGA3dBuffer* buffer, unsigned int offset, unsigned int stride, VertexType type, bool normalize)
{
	int semantic_index = static_cast<VEGAD3d10ShaderProgram*>(m_shader)->attribToSemanticIndex(name);
	if (semantic_index == -1)
	{
		OP_ASSERT(!"addBinding() called with nonexistent attribute");
		return OpStatus::ERR;
	}
	return addComponent(buffer, semantic_index, offset, stride, type, normalize);
}

OP_STATUS VEGAD3d10VertexLayout::addConstBinding(const char *name, float *values)
{
	int semantic_index = static_cast<VEGAD3d10ShaderProgram*>(m_shader)->attribToSemanticIndex(name);
	if (semantic_index == -1)
	{
		OP_ASSERT(!"addConstBinding() called with nonexistent attribute");
		return OpStatus::ERR;
	}
	return addConstComponent(semantic_index, values);
}

#endif // CANVAS3D_SUPPORT

bool VEGAD3d10VertexLayout::isCompliant() const
{
	/* To be seen as compliant a vertex layout must fulfill the following conditions:
	   1) Component offsets and sizes must be multiples of 4
	   2) All components must have the same stride
	   3) All components must fit inside the the interval [0..stride]
	   4) No components may overlap.
	   5) The component type must not be one of the types that are remapped.
	*/
	for (const D3dPerVertexComponent* compA = m_components; compA; compA = compA->next)
	{
		// Check alignment.
		if (compA->offset & 3) return false;
		if (compA->stride & 3) return false;

		for (const D3dPerVertexComponent* compB = m_components; compB; compB = compB->next)
		{
			// Only compare components belonging to the same buffer, and not with itself.
			if (compB == compA || compA->buffer != compB->buffer) continue;

			// The strides need to match to be compliant.
			if (compA->stride != compB->stride) return false;

			// All components must be within the stride of the vertex.
			if (compA->stride < compB->offset + sVEGAVertexTypeByteSize[compB->type]) return false;

			unsigned int aMin = compA->offset,
						 aMax = compA->offset + sVEGAVertexTypeByteSize[compA->type],
						 bMin = compB->offset,
						 bMax = compB->offset + sVEGAVertexTypeByteSize[compB->type];
			// If neither of the ranges are completely on one side of the other then there's an overlap.
			if (!(aMax <= bMin || bMax <= aMin))
				return false;
		}
		if (sDX10VertexTypeRemap[compA->type][compA->normalize] != compA->type)
			return false;
	}
	return true;
}

OP_STATUS VEGAD3d10VertexLayout::addDescription(unsigned int &maxStream, D3D10_INPUT_ELEMENT_DESC* description, unsigned int cnum, bool reshuffle, D3dComponent *comp)
{
	description[cnum].SemanticName = "TEXCOORD";
	description[cnum].SemanticIndex = comp->index;
	switch (sDX10VertexTypeRemap[comp->type][comp->normalize])
	{
	case FLOAT1:
		description[cnum].Format = DXGI_FORMAT_R32_FLOAT;
		break;
	case FLOAT2:
		description[cnum].Format = DXGI_FORMAT_R32G32_FLOAT;
		break;
	case FLOAT3:
		description[cnum].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		break;
	case FLOAT4:
		description[cnum].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		break;
	case USHORT2:
		description[cnum].Format = comp->normalize ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R16G16_UINT;
		break;
	case USHORT4:
		description[cnum].Format = comp->normalize ? DXGI_FORMAT_R16G16B16A16_UNORM : DXGI_FORMAT_R16G16B16A16_UINT;
		break;
	case SHORT2:
		description[cnum].Format = comp->normalize ? DXGI_FORMAT_R16G16_SNORM : DXGI_FORMAT_R16G16_SINT;
		break;
	case SHORT4:
		description[cnum].Format = comp->normalize ? DXGI_FORMAT_R16G16B16A16_SNORM : DXGI_FORMAT_R16G16B16A16_SINT;
		break;
	case UBYTE4:
		description[cnum].Format = comp->normalize ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UINT;
		break;
	case BYTE4:
		description[cnum].Format = comp->normalize ? DXGI_FORMAT_R8G8B8A8_SNORM : DXGI_FORMAT_R8G8B8A8_SINT;
		break;
	case USHORT1:
	case USHORT3:
	case SHORT1:
	case SHORT3:
	case UBYTE1:
	case UBYTE2:
	case UBYTE3:
	case BYTE1:
	case BYTE2:
	case BYTE3:
	default:
		OP_ASSERT(FALSE); // These vertex types shouldn't be used.
		OP_DELETEA(description);
		return OpStatus::ERR;
	}
#ifdef CANVAS3D_SUPPORT
	// If we need to reshuffle the data we put each component in its own stream at offset 0.
	if (reshuffle)
	{
		description[cnum].InputSlot = cnum;
		description[cnum].AlignedByteOffset = 0;
	}
	else
#endif //CANVAS3D_SUPPORT
	{
		description[cnum].InputSlot = comp->firstOfStream->stream;

		/* 'offset' is the offset into the buffer while
			'AlignedByteOffset' is the offset within a single element -
			hence a straight assignment won't work here. We pass the
			buffer offset to IASetVertexBuffers() later. */
		description[cnum].AlignedByteOffset = comp->offset - comp->firstOfStream->offset;
	}
	description[cnum].InputSlotClass = comp->perInstance ? D3D10_INPUT_PER_INSTANCE_DATA : D3D10_INPUT_PER_VERTEX_DATA;
	maxStream = MAX(maxStream, comp->firstOfStream->index);
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10VertexLayout::bind(ID3D10Device1* dev)
{
	if (!m_d3d_layout)
	{
#ifdef CANVAS3D_SUPPORT
		// Custom shaders need special treatment. We trust our own shaders and data to be in
		// a good format already.
		bool isCustomShader = m_shader->getShaderType() == VEGA3dShaderProgram::SHADER_CUSTOM;
		bool needToReshuffle = isCustomShader && !isCompliant();
		unsigned int nShaderInputs = m_shader->getNumInputs();
		unsigned int nInputs = MAX(nShaderInputs, m_num_components);
#else // CANVAS3D_SUPPORT
		unsigned int nInputs = m_num_components;
#endif // CANVAS3D_SUPPORT

		// Build the input description.
		D3D10_INPUT_ELEMENT_DESC *d3dInputDesc = OP_NEWA(D3D10_INPUT_ELEMENT_DESC, nInputs);
		RETURN_OOM_IF_NULL(d3dInputDesc);
		op_memset(d3dInputDesc, 0, sizeof(D3D10_INPUT_ELEMENT_DESC)*nInputs);

		unsigned int maxStream = 0;
		unsigned int cnum = 0;

		if (m_perInstanceComponents)
		{
			// If there are any per instance components they will be placed in stream 0.
			for (D3dPerInstanceComponent* comp = m_perInstanceComponents; comp; ++cnum, comp = comp->next)
			{
				RETURN_IF_ERROR(addDescription(maxStream, d3dInputDesc, cnum, false, comp));
			}
			++maxStream;
			for (D3dPerVertexComponent* comp = m_d3dCompatibleComponents ? m_d3dCompatibleComponents : m_components; comp; comp = comp->next)
				++comp->stream;
		}
#ifdef CANVAS3D_SUPPORT
		for (D3dPerVertexComponent* comp = m_d3dCompatibleComponents ? m_d3dCompatibleComponents : m_components; comp; ++cnum, comp = comp->next)
		{
			RETURN_IF_ERROR(addDescription(maxStream, d3dInputDesc, cnum, needToReshuffle, comp));
		}

		// One stream per component if we reshuffled.
		if (needToReshuffle)
			maxStream = cnum;

		// Check that each shader attribute has input bound to it. Feed zero to
		// unbound attributes.
		if (isCustomShader && nShaderInputs != cnum)
		{
			++maxStream;
			for (unsigned int i = 0; i < nShaderInputs; ++i)
			{
				int semanticIndex = static_cast<VEGAD3d10ShaderProgram *>(m_shader)->getSemanticIndex(i);
				bool found = false;
				for (unsigned int c = 0; c < cnum; ++c)
					if (d3dInputDesc[c].SemanticIndex == (UINT)semanticIndex)
					{
						found = true;
						break;
					}
				if (!found)
				{
					// Nothing is bound to the shader attribute with semantic 'TEXCOORD<semanticIndex>'

					d3dInputDesc[cnum].SemanticName = "TEXCOORD";
					d3dInputDesc[cnum].SemanticIndex = semanticIndex;
					d3dInputDesc[cnum].InputSlot = maxStream;
					d3dInputDesc[cnum].Format = DXGI_FORMAT_R32_FLOAT;
					d3dInputDesc[cnum].AlignedByteOffset = 0;
					d3dInputDesc[cnum].InputSlotClass = D3D10_INPUT_PER_VERTEX_DATA;
					++cnum;
				}
			}
		}
#endif //CANVAS3D_SUPPORT

		// Create the actual vertex layout.
		HRESULT err = dev->CreateInputLayout(d3dInputDesc, cnum,
		                                     static_cast<VEGAD3d10ShaderProgram*>(m_shader)->getShaderData(),
		                                     static_cast<VEGAD3d10ShaderProgram*>(m_shader)->getShaderDataSize(),
		                                     &m_d3d_layout);
		OP_DELETEA(d3dInputDesc);
		if (FAILED(err))
			return (err == E_OUTOFMEMORY) ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;

#ifdef CANVAS3D_SUPPORT

		/* If we end up with a layout that is not D3D10 compatible, we
		   reshuffle the data so that for each component that uses a particular
		   buffer, we put the data associated with that component into its own
		   separate, continuous segment within the buffer.

		   For example, if components C1 and C2 reference the buffer B, then
		   the layout of B after reshuffling would be [ C1' C2' ], where C1'
		   and C2' is the data associated with C1 and C2. */
		if (needToReshuffle)
		{
			// Allocate an array 'buffers' that holds the unique buffers from 'm_components'

			unsigned int numBuffers = 0;
			VEGA3dBuffer **buffers = OP_NEWA(VEGA3dBuffer *, m_num_components);
			if (!buffers)
			{
				releaseInputLayout();
				dev->IASetInputLayout(NULL);
				return OpStatus::ERR_NO_MEMORY;
			}
			op_memset(buffers, 0, sizeof(VEGA3dBuffer*) * m_num_components);

			// Put the unique buffers from 'm_components' into 'buffers'
			for (D3dPerVertexComponent* comp = m_components; comp; comp = comp->next)
			{
				bool foundBuffer = false;
				for (unsigned i = 0; i < numBuffers; ++i)
					if (comp->buffer == buffers[i])
					{
						foundBuffer = true;
						break;
					}
				if (!foundBuffer)
				{
					// The buffer is not yet in 'buffers', so put it in
					buffers[numBuffers++] = comp->buffer;
				}
			}

			// Put a copy of the input components in 'm_d3dCompatibleComponents', but
			// where each component is in its own stream

			OP_ASSERT(!m_d3dCompatibleComponents);
			unsigned int stream = 0;
			D3dPerVertexComponent **nextPtr = &m_d3dCompatibleComponents;
			for (D3dPerVertexComponent* comp = m_components; comp; comp = comp->next)
			{
				*nextPtr = OP_NEW(D3dPerVertexComponent, ());
				D3dPerVertexComponent *aligned = *nextPtr;
				if (!aligned)
				{
					OP_DELETEA(buffers);
					releaseInputLayout();
					dev->IASetInputLayout(NULL);
					return OpStatus::ERR_NO_MEMORY;
				}
				*aligned = *comp;
				// Reshuffled data always ends up in its own stream.
				aligned->firstOfStream = aligned;
				aligned->stream = stream++;
				aligned->next = NULL;
				nextPtr = (D3dPerVertexComponent**)&(*nextPtr)->next;
			}

			for (unsigned i = 0; i < numBuffers; ++i)
			{
				/* Calculate the maximum number of vertices we can fit in the buffer
				   buffer[i], as determined by the components that use it. The
				   component that allows for the fewest vertices determines the
				   maximum. */

				unsigned int maxVertices = 0xffffffff;
				for (D3dPerVertexComponent* comp = m_components; comp; comp = comp->next)
					if (comp->buffer == buffers[i])
						maxVertices = MIN(maxVertices,
						                  maxVerticesForBuffer(buffers[i]->getSize(), comp->offset, comp->stride,
						                                       sVEGAVertexTypeByteSize[comp->type]));

				/* Remap to new vertex types and put components into separate,
				   continuous (no stride) segments within the buffer */

				unsigned int bufferOffset = 0;
				for (D3dPerVertexComponent* comp = m_d3dCompatibleComponents; comp; comp = comp->next)
					if (comp->buffer == buffers[i])
					{
						comp->offset = bufferOffset;
						comp->type = sDX10VertexTypeRemap[comp->type][comp->normalize];
						comp->stride = sVEGAVertexTypeByteSize[comp->type];

						// Jump to the segment for the next component
						bufferOffset += maxVertices * comp->stride;
					}
			}
			OP_DELETEA(buffers);
		}
#endif //CANVAS3D_SUPPORT
	}

	// Sync buffers (possibly reshuffling the indata) and set up the inputs.
	for (D3dPerVertexComponent* comp = m_d3dCompatibleComponents ? m_d3dCompatibleComponents : m_components; comp; comp = comp->next)
	{
		bool isFirstOfStream = comp->firstOfStream == comp;
		VEGAD3d10Buffer *d3d10Buffer = (VEGAD3d10Buffer*)comp->buffer;
#ifdef _DEBUG
		// Getting rid of an annoying but harmless DX info message in debug.
		if (isFirstOfStream)
		{
			unsigned null = 0;
			ID3D10Buffer* nullBuf = NULL;
			dev->IASetVertexBuffers(comp->stream, 1, &nullBuf, &null, &null);
		}
#endif
		d3d10Buffer->Sync(m_components, m_d3dCompatibleComponents, m_d3d_layout);
		ID3D10Buffer* b = d3d10Buffer->getBuffer();
		if (isFirstOfStream)
			dev->IASetVertexBuffers(comp->stream, 1, &b, &comp->stride, &comp->offset);
	}

	// If we have any per instance data.
	if (m_perInstanceComponents)
	{
		// Check how many we've got.
		int count = 0;
		for (D3dPerInstanceComponent *c = m_perInstanceComponents; c; c = c->next)
			++count;

		// Make sure we have a large enough buffer. We always use 4 floats per component.
		if (m_perInstanceBuffer && m_perInstanceBufferSize <  sizeof(float) * count * 4)
		{
			m_perInstanceBuffer->Release();
			m_perInstanceBuffer = NULL;
		}
		if (!m_perInstanceBuffer)
		{
			m_perInstanceBufferSize =  sizeof(float) * count * 4;
			D3D10_BUFFER_DESC bd;
			bd.Usage = D3D10_USAGE_DYNAMIC;
			bd.ByteWidth = m_perInstanceBufferSize;
			bd.BindFlags = D3D10_BIND_VERTEX_BUFFER;
			bd.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
			bd.MiscFlags = 0;
			if (FAILED(dev->CreateBuffer(&bd, NULL, &m_perInstanceBuffer)))
				return OpStatus::ERR;
		}

		/* Initialize the buffer for per-instance data (used to implement
		   constant attribute values in WebGL). */
		float *pValues = NULL;
		m_perInstanceBuffer->Map(D3D10_MAP_WRITE_DISCARD, NULL, (void **)&pValues);
		for (D3dPerInstanceComponent *c = m_perInstanceComponents; c; c = c->next)
			for (unsigned int i = 0; i < 4; ++i)
				pValues[c->offset / sizeof(float) + i] = c->values[i];
		m_perInstanceBuffer->Unmap();

		// We're always using stream 0 for per instance data.
		unsigned int stride = 0;
		unsigned int offset = 0;
		dev->IASetVertexBuffers(0, 1, &m_perInstanceBuffer, &stride, &offset);
	}

	dev->IASetInputLayout(m_d3d_layout);
	return OpStatus::OK;
}

void VEGAD3d10VertexLayout::unbind(ID3D10Device1* dev)
{
	if (m_d3d_layout)
		dev->IASetInputLayout(NULL);
}

#endif //NO_DXSDK
#endif // VEGA_BACKEND_DIRECT3D10

