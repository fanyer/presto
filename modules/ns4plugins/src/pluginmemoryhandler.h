/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _PLUGINMEMORYHANDLER_H_INC_
#define _PLUGINMEMORYHANDLER_H_INC_

#ifdef _PLUGIN_SUPPORT_

/* The plugin memory handler maintains a pool of allocated
   objects.  For some plugins the number of active objects can 
   easily be in the thousands on the average (esp Flash 5 seems
   to be voracious).  A linear list of objects would in that 
   case be an inappropriate representation as the time it takes
   to free objects would be prohibitive.

   This memory handler maintains a hash table based on a simple
   hash function: a cast of void* to long.
   */
#define NODES_PER_BLOCK		128

struct PIMemNode {
	void*			datum;
	PIMemNode*		next;
};

class PIMemBlock {
public:
	PIMemBlock() : next_block(0), next_node(0) {}
	PIMemBlock*		next_block;
	int				next_node;
	PIMemNode		nodes[NODES_PER_BLOCK];
};

class PluginMemoryHandler
{
private:
	PIMemBlock*		blocks;
    PIMemNode**		table;
	PIMemNode*		free_nodes;
	int				size;
	int				population;
	int				Hash( void *p ) {
						return (int)((UINTPTR)p % size); 
					}
	void			Insert( PIMemNode* node ) {
						int h = Hash(node->datum);
						node->next = table[h];
						table[h] = node;
					}
	bool			Grow();
	PIMemNode*		NewNode();
	void			DisposeNode(PIMemNode*);

public:
					PluginMemoryHandler()
						: blocks(0),
						  table(0),
						  free_nodes(0),
						  size(0),
						  population(0) {
					}
    				~PluginMemoryHandler();

    void*			Malloc(int32 siz);
    void			Free(void *ptr);
	static			PluginMemoryHandler* GetHandler();
	static			void Destroy();
};

#endif // _PLUGIN_SUPPORT_
#endif // !_PLUGINMEMORYHANDLER_H_INC_
