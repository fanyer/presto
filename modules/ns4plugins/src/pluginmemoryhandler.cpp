/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/src/pluginmemoryhandler.h"

/* static */ PluginMemoryHandler*
PluginMemoryHandler::GetHandler()
{
	if (!g_pluginmemoryhandler)
		g_pluginmemoryhandler = OP_NEW(PluginMemoryHandler, ());

	return g_pluginmemoryhandler;
}

PluginMemoryHandler::~PluginMemoryHandler()
{
	PIMemBlock *b, *next;

	for ( b=blocks ; b ; b=next ) {

		// free allocated memory, if any
		for (int i=0 ; i < b->next_node ; i++) { 
			PIMemNode *node = &b->nodes[i];
			if (node && node->datum)
				op_free(node->datum);
		}

		next = b->next_block;
		OP_DELETE(b);
	}
	OP_DELETEA(table);
}

/* static */ void
PluginMemoryHandler::Destroy()
{
	if (g_pluginmemoryhandler)
	{
		OP_DELETE(g_pluginmemoryhandler);
		g_pluginmemoryhandler = 0;
	}
}

PIMemNode* PluginMemoryHandler::NewNode()
{
	PIMemNode *node;

	if (free_nodes != 0) {
		node = free_nodes;
		free_nodes = node->next;
	}
	else {
		if (blocks == 0 || blocks->next_node == NODES_PER_BLOCK) {
			PIMemBlock *b = OP_NEW(PIMemBlock, ());
			if (b == 0)	return 0;
			b->next_block = blocks;
			blocks = b;
		}
		node = &blocks->nodes[ blocks->next_node++ ];
	}

	node->next = 0;
	node->datum = 0;
	return node;
}

void PluginMemoryHandler::DisposeNode( PIMemNode *node )
{
	node->next = free_nodes;
	free_nodes = node;
}

bool PluginMemoryHandler::Grow()
{
	int size, i, old_size;
	PIMemNode **t, **old_table;

	if (table != 0)
		size = (int)(this->size*1.20);
	else
		size = (int)(NODES_PER_BLOCK/0.75);

	t = OP_NEWA(PIMemNode*, size);
	if (t == 0) return false;

	for ( i=0 ; i < size ; i++ )
		t[i] = 0;
	old_table = table;
	old_size = this->size;
	table = t;
	this->size = size;

	if (old_table != 0) {
		for ( i=0 ; i < old_size ; i++ ) {
			PIMemNode *p, *next;
			for ( p=old_table[i] ; p ; p=next ) {
				next = p->next;
				Insert( p );
			}
		}
		OP_DELETEA(old_table);
	}
	return true;
}

void* PluginMemoryHandler::Malloc(int32 siz)
{ 
	void *p = op_malloc( siz );
	PIMemNode *node;

	if (p == 0) return 0;

	node = NewNode();
	if (node == 0) {
		op_free(p);
		return 0;
	}
	node->datum = p;

	if (++population >= size * 0.75)
		if (!Grow()) {
			op_free( node->datum );
			node->datum = 0;
			DisposeNode( node );
			return 0;
		}

	Insert( node );
	return p;
}

void PluginMemoryHandler::Free(void *p)
{
	PIMemNode *node, *prev;
	int h;

	if (size)
	{
		h = Hash(p);
		for ( prev=0, node=table[h]; node != 0 && node->datum != p ; prev=node, node=node->next )
			;
		if (node == 0) return;
		if (prev == 0)
			table[h] = node->next;
		else
			prev->next = node->next;
		op_free( node->datum );
		node->datum = 0;
		DisposeNode( node );
		--population;
	}
}


#endif // _PLUGIN_SUPPORT_
