/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAD3D10BUFFER_H
#define VEGAD3D10BUFFER_H

#ifdef VEGA_BACKEND_DIRECT3D10

#include "modules/libvega/vega3ddevice.h"

// The different types of cached layouts. CL_ByteToShort and CL_Short for indices and >CL_UID_start
// for vertex layouts.
enum CachedLayout
{
	CL_Invalid,
	CL_ByteToShort,
	CL_Short,
	CL_VertexLayout
};

struct D3dComponent
{
	D3dComponent() { op_memset(this, 0, sizeof(D3dComponent)); }

	unsigned int stream;
	D3dComponent *firstOfStream;
	unsigned int index;
	unsigned int offset;
	VEGA3dVertexLayout::VertexType type;
	bool normalize;
	bool perInstance;
};

struct D3dPerVertexComponent : public D3dComponent
{
	D3dPerVertexComponent() : next(NULL), buffer(NULL), stride(0) {}

	D3dPerVertexComponent *next;
	VEGA3dBuffer* buffer;
	unsigned int stride;
};

struct D3dPerInstanceComponent : public D3dComponent
{
	D3dPerInstanceComponent() : next(NULL) { values[0] = values[1] = values[2] = 0; values[3] = 1; }

	D3dPerInstanceComponent *next;
	float values[4];
};

class VEGAD3d10Buffer : public VEGA3dBuffer
{
public:
	VEGAD3d10Buffer();
	~VEGAD3d10Buffer();

	OP_STATUS Construct(ID3D10Device1* dev, unsigned int size, VEGA3dBuffer::Usage usage, bool isIndexBuffer, bool deferredUpdate);

	virtual OP_STATUS writeAtOffset(unsigned int offset, unsigned int size, void* data);
	virtual OP_STATUS writeAnywhere(unsigned int num, unsigned int size, void* data, unsigned int& index);
	unsigned int getSize(){return size;}
	ID3D10Buffer* getBuffer(){return buffer;}

	/* Copies buffers from RAM to the GPU when using deferred update. Otherwise, does nothing. */
	OP_STATUS Sync(D3dPerVertexComponent *inputs,  D3dPerVertexComponent *aligned, ID3D10InputLayout* layoutId);
	OP_STATUS Sync(bool byteToShort);

	const char *GetUnalignedBuffer() const { return unalignedBuffer; }
	void clearCached(CachedLayout l = CL_Invalid) { cachedLayout = NULL; cachedType = l; }
private:
	void CalculateBufferSizes(D3dPerVertexComponent *inputs,  D3dPerVertexComponent *aligned, unsigned int &alignedSize, unsigned int &maxVertices);

	ID3D10Device1* device;

	ID3D10Buffer* buffer;
	unsigned int size;
	VEGA3dBuffer::Usage usage;
	char *unalignedBuffer;
	bool deferredUpdate;
	CachedLayout cachedType;
	ID3D10InputLayout* cachedLayout;
	bool isIndexBuffer;
	unsigned int currentOffset;
};

class VEGAD3d10VertexLayout : public VEGA3dVertexLayout
{
public:
	VEGAD3d10VertexLayout(VEGA3dShaderProgram* sp);
	virtual ~VEGAD3d10VertexLayout();
	virtual OP_STATUS addComponent(VEGA3dBuffer* buffer, unsigned int index, unsigned int offset, unsigned int stride, VertexType type, bool normalize);
	virtual OP_STATUS addConstComponent(unsigned int index, float *values);

#ifdef CANVAS3D_SUPPORT
	virtual OP_STATUS addBinding(const char *name, VEGA3dBuffer* buffer, unsigned int offset, unsigned int stride, VertexType type, bool normalize);
	virtual OP_STATUS addConstBinding(const char *name, float *values);
#endif // CANVAS3D_SUPPORT

	OP_STATUS bind(ID3D10Device1* dev);
	void unbind(ID3D10Device1* dev);

	bool isCompliant() const;

	void releaseInputLayout();

private:
	OP_STATUS addDescription(unsigned int &maxStream, D3D10_INPUT_ELEMENT_DESC* description, unsigned int cnum, bool reshuffle, D3dComponent *comp);

	D3dPerVertexComponent* m_components;
	D3dPerVertexComponent* m_d3dCompatibleComponents;
	D3dPerInstanceComponent* m_perInstanceComponents;
	unsigned int m_num_components;
	unsigned int m_num_streams;
	ID3D10InputLayout* m_d3d_layout;
	VEGA3dShaderProgram* m_shader;
	ID3D10Buffer* m_perInstanceBuffer;
	unsigned int m_perInstanceBufferSize;
};

#endif // VEGA_BACKEND_DIRECT3D10
#endif // !VEGAD3D10BUFFER_H

