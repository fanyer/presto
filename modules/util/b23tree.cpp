/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/util/b23tree.h"
#include "modules/hardcore/mem/mem_man.h"

B23Tree_Item* B23Tree_Item::PrevLeaf()
{
	B23Tree_Node *prnt = parent;
	int i;

	while(prnt)
	{
		if(prnt->AccessItem(0) == this)
			i = 0;
		else
		{
			OP_ASSERT(prnt->AccessItem(1) == this);
			i = 1;
		}

		B23Tree_Node *node = prnt->AccessSubNode(i);
		if(node)
		{
			return node->LastLeaf();
		}

		prnt = prnt->GetParent();
	}

	return NULL;
}

B23Tree_Item* B23Tree_Item::NextLeaf()
{
	B23Tree_Node *prnt = parent;
	int i;

	while(prnt)
	{
		if(prnt->AccessItem(0) == this)
			i = 1;
		else
		{
			OP_ASSERT(prnt->AccessItem(1) == this);
			i = 2;
		}

		B23Tree_Node *node = prnt->AccessSubNode(i);
		if(node)
		{
			return node->FirstLeaf();
		}

		prnt = prnt->GetParent();
	}

	return NULL;
}

void B23Tree_Item::Out()
{
	B23Tree_Store *store = GetMasterStore();

	if(store)
	{
		store->Delete(this, FALSE);
	}
}

void B23Tree_Item::Into(B23Tree_Store* list)
{
	if(InList())
		Out();

	if(list)
	{
		TRAPD(op_err, list->InsertL(this));

		if(OpStatus::IsFatal(op_err))
			g_memory_manager->RaiseCondition(op_err);
	}
}

void B23Tree_Item::Follow(B23Tree_Item* link)
{
	if(link)
	{
		B23Tree_Store *store = link->GetMasterStore();

		if(store)
			Into(store);
	}
}

B23Tree_Store *B23Tree_Item::GetMasterStore() const
{
	B23Tree_Node *node = GetParent();

	while(node && node->GetParent())
		node = node->GetParent();

	return (node ? node->GetParentStore() : NULL);

}

B23Tree_Node::B23Tree_Node(B23_CompareFunction comp)
: parent(NULL), parent_store(NULL), compare(comp)
{
	next[0] = NULL;
	next[1] = NULL;
	next[2] = NULL;

	items[0] = NULL;
	items[1] = NULL;
}

B23Tree_Node::~B23Tree_Node()
{
	OP_DELETE(next[0]);
	next[0] = NULL;
	OP_DELETE(next[1]);
	next[1] = NULL;
	OP_DELETE(next[2]);
	next[2] = NULL;

	OP_DELETE(items[0]);
	items[0] = NULL;
	OP_DELETE(items[1]);
	items[1] = NULL;
}

void B23Tree_Node::UpdateParents()
{
	if(items[0])
		items[0]->SetParent(this);
	if(items[1])
		items[1]->SetParent(this);
	if(next[0])
		next[0]->SetParent(this);
	if(next[1])
		next[1]->SetParent(this);
	if(next[2])
		next[2]->SetParent(this);
}

BOOL B23Tree_Node::TraverseL(uint32 action, void *params)
{
	if(next[0])
	{
		if(!next[0]->TraverseL(action, params))
			return FALSE;
	}
	if(items[0])
	{
		if(!items[0]->TraverseActionL(action, params))
			return FALSE;
	}
	if(next[1])
	{
		if(!next[1]->TraverseL(action, params))
			return FALSE;
	}
	if(items[1])
	{
		if(!items[1]->TraverseActionL(action, params))
			return FALSE;
	}
	if(next[2])
	{
		if(!next[2]->TraverseL(action, params))
			return FALSE;
	}
	return TRUE;
}

B23Tree_Node *B23Tree_Node::ConstructNodeL()
{
	return OP_NEW_L(B23Tree_Node, (compare));
}

OP_STATUS B23Tree_Node::ConstructNode(B23Tree_Node*& new_node)
{
	new_node = OP_NEW(B23Tree_Node, (compare));
	return (new_node ? OpStatus::OK : OpStatus::ERR_NO_MEMORY);
}

B23Tree_Node *B23Tree_Node::InsertL(B23Tree_Item *item)
{
	if(!item)
		return NULL;

	int index =0;
	int cmpval = -1;

	if(items[0] && (cmpval = (*compare)(item, items[0])) > 0)
	{
		index++;
		if(items[1] && (cmpval = (*compare)(item, items[1])) > 0)
			index ++;
	}

	if(cmpval == 0)
	{
		return NULL;
	}

	if(next[index] == NULL)
	{
		if(items[1] != NULL)
		{
			OpStackAutoPtr<B23Tree_Item> nitem(item);

			// Split leaf
			OpStackAutoPtr<B23Tree_Node> new_leaf(ConstructNodeL());
			OpStackAutoPtr<B23Tree_Node> new_node(ConstructNodeL());

			nitem.release();

			new_node->next[0] = this;
			new_node->next[1] = new_leaf.get();

			new_leaf->items[0] = (index == 2 ? item : items[1]);
			new_node->items[0] = (index == 1 ? item : items[(index == 0 ? 0 : 1)]);
			if(index == 0)
				items[0] = item;
			items[1] = NULL;

			UpdateParents();
			new_leaf->UpdateParents();
			new_node->UpdateParents();

			new_leaf.release();

			return new_node.release();
		}

		if(index == 0)
			items[1] = items[0];

		item->SetParent(this);
		items[index] = item;
		return NULL;
	}

	B23Tree_Node *split_parent = next[index]->InsertL(item);

	if(split_parent)
	{
		if(items[1] == NULL)
		{
			if(index == 0)
			{
				items[1] = items[0];
				next[2] = next[1];
			}
			items[index] = split_parent->items[0];
			next[index] = split_parent->next[0];
			next[index+1] = split_parent->next[1];
			split_parent->items[0] = NULL;
			split_parent->next[0] = NULL;
			split_parent->next[1] = NULL;
			OP_DELETE(split_parent);

			UpdateParents();
	
			return NULL;
		}

		B23Tree_Node* new_node;

		OP_STATUS op_err = ConstructNode(new_node);
		if(OpStatus::IsError(op_err))
		{
			split_parent->next[0] = NULL;
			OP_DELETE(split_parent);
			LEAVE(op_err);
		}

		if(index == 0)
		{
			B23Tree_Item *titem = items[0];

			new_node->items[0] = items[1];
			new_node->next[1] = next[2];
			new_node->next[0] = next[1];
			new_node->UpdateParents();

			items[0] = split_parent->items[0];
			split_parent->items[0] = titem;
			next[1] = split_parent->next[1];
		}
		else if(index == 1)
		{
			new_node->items[0] = items[1];
			new_node->next[1] = next[2];
			new_node->next[0] = split_parent->next[1];
			new_node->UpdateParents();
		}
		else //if (index == 2)
		{
			new_node->items[0] = split_parent->items[0];
			new_node->next[1] = split_parent->next[1];
			new_node->next[0] = next[2];
			new_node->UpdateParents();

			split_parent->items[0] = items[1];
		}

		items[1] = NULL;
		next[2] = NULL;
		UpdateParents();

		split_parent->next[0] = this;
		split_parent->next[1] = new_node;
		split_parent->UpdateParents();

		return split_parent;
	}

	return NULL;
}

B23Tree_Node *B23Tree_Node::Delete(B23Tree_Item *nitem, BOOL delete_item)
{
	if(nitem == NULL)
		return NULL;

	int index =0;
	int cmpval = -1;

	if(items[0] && (cmpval = (*compare)(items[0], nitem)) > 0)
	{
		index++;
		if(items[1] && (cmpval = (*compare)(items[1], nitem)) > 0)
			index ++;
	}

	B23Tree_Node *temp_node;
	if(cmpval == 0)
	{
		if(next[index+1] == NULL)
		{
			if(delete_item)
				OP_DELETE(items[index]);
			else
				items[index]->SetParent(NULL);

			items[index] = (index ? NULL : items[1]);
			if(!index)
				items[1] = NULL;

			//OP_ASSERT(next[0] == NULL && next[1] == NULL && next[2] == NULL );
			return NULL;
		}

		temp_node = next[index+1]->adjustTree(items[index], delete_item);
		UpdateParents();
		index ++;
	}
	else if(next[index])
	{
		temp_node = next[index]->Delete(nitem, delete_item);
	}
	else
		return NULL;

	if(next[index]->items[0] != NULL)
	{

		/*
		OP_ASSERT(items[0] != NULL);
		OP_ASSERT(next[0] != NULL && next[1] != NULL);
		OP_ASSERT((items[1] == NULL && next[2] == NULL)|| (items[1] != NULL && next[2] != NULL));
		OP_ASSERT(!next[0] || next[0]->items[0] != NULL);
		OP_ASSERT(!next[1] || next[1]->items[0] != NULL);
		OP_ASSERT(!next[2] || next[2]->items[0] != NULL);
		*/

		return NULL;
	}

	return adjustNode(temp_node, index);
}

B23Tree_Node *B23Tree_Node::adjustTree(B23Tree_Item *&target, BOOL delete_item)
{
	if(next[0] == NULL)
	{
		if(delete_item)
			OP_DELETE(target);
		else
			target->SetParent(NULL);
		target = items[0];
		items[0] = items[1];
		items[1] = NULL;

		return NULL;
	}

	B23Tree_Node *temp_node;

	temp_node = next[0]->adjustTree(target, delete_item);

	if(next[0]->items[0] != NULL)
	{
		/*
		OP_ASSERT(items[0] != NULL);
		OP_ASSERT(next[0] != NULL && next[1] != NULL);
		OP_ASSERT((items[1] == NULL && next[2] == NULL)|| (items[1] != NULL && next[2] != NULL));
		*/

		return NULL;
	}

	return adjustNode(temp_node, 0);
}

B23Tree_Node *B23Tree_Node::adjustNode(B23Tree_Node *temp_node, int index)
{
	B23Tree_Node *node = next[index];
	B23Tree_Node *sibling;

	if(index && next[index-1]->items[1])
	{
		sibling = next[index-1];
		node->items[0] = items[index-1];
		node->next[1] = temp_node;
		node->next[0] = sibling->next[2];
		items[index-1] = sibling->items[1];
		sibling->items[1] = NULL;
		sibling->next[2] = NULL;

		node->UpdateParents();
		sibling->UpdateParents();
		UpdateParents();

		/*
		OP_ASSERT(items[0] != NULL);
		OP_ASSERT(next[0] != NULL && next[1] != NULL);
		OP_ASSERT((items[1] == NULL && next[2] == NULL)|| (items[1] != NULL && next[2] != NULL));
		OP_ASSERT(!next[0] || next[0]->items[0] != NULL);
		OP_ASSERT(!next[1] || next[1]->items[0] != NULL);
		OP_ASSERT(!next[2] || next[2]->items[0] != NULL);
		*/

		return NULL;
	}
	else if(index < 2 &&  next[index +1] && next[index +1]->items[1])
	{
		sibling = next[index+1];
		node->items[0] = items[index];
		node->next[0] = temp_node;
		node->next[1] = sibling->next[0];
		items[index] = sibling->items[0];
		sibling->items[0] = sibling->items[1];
		sibling->items[1] = NULL;
		sibling->next[0] = sibling->next[1];
		sibling->next[1] = sibling->next[2];
		sibling->next[2] = NULL;
	
		node->UpdateParents();
		sibling->UpdateParents();
		UpdateParents();

		/*
		OP_ASSERT(items[0] != NULL);
		OP_ASSERT(next[0] != NULL && next[1] != NULL);
		OP_ASSERT((items[1] == NULL && next[2] == NULL)|| (items[1] != NULL && next[2] != NULL));
		OP_ASSERT(!next[0] || next[0]->items[0] != NULL);
		OP_ASSERT(!next[1] || next[1]->items[0] != NULL);
		OP_ASSERT(!next[2] || next[2]->items[0] != NULL);
		*/

		return NULL;
	}

	node->next[0] = NULL;
	OP_DELETE(node);

	if(index)
	{
		sibling = next[index-1];
		if(index== 1)
			next[1] = next[2];
		next[2] = NULL;

		sibling->items[1] = items[index-1];
		items[index-1] = index >1 ? NULL : items[index];
		if(index == 1)
			items[1] = NULL;

		sibling->next[2] = temp_node;
	}
	else
	{
		next[0] = sibling = next[1];
		next[1] = next[2];
		next[2] = NULL;

		sibling->items[1] = sibling->items[0];
		sibling->items[0] = items[0];
		items[0] = items[1];
		items[1] = NULL;
		sibling->next[2] = sibling->next[1];
		sibling->next[1] = sibling->next[0];
		sibling->next[0] = temp_node;
	}

	sibling->UpdateParents();
	UpdateParents();

	/*

		OP_ASSERT(next[0] != NULL && (items[0] == NULL || next[1] != NULL));
		OP_ASSERT((items[1] == NULL && next[2] == NULL)|| (items[1] != NULL && next[2] != NULL));
		OP_ASSERT(!next[0] || next[0]->items[0] != NULL);
		//OP_ASSERT(!next[0] || (next[0]->next[0] != NULL && next[0]->next[1] != NULL));
		//OP_ASSERT(!next[0] || (next[0]->items[1] == NULL && next[0]->next[2] == NULL)||
		//	(next[0]->items[1] != NULL && next[0]->next[2] != NULL));
		OP_ASSERT(!next[1] || next[1]->items[0] != NULL);
		//OP_ASSERT(!next[1] || (next[1]->next[0] != NULL && next[1]->next[1] != NULL));
		//OP_ASSERT(!next[1] || (next[1]->items[1] == NULL && next[1]->next[2] == NULL)||
		//	(next[1]->items[1] != NULL && next[1]->next[2] != NULL));
		OP_ASSERT(!next[2] || next[2]->items[0] != NULL);
		//OP_ASSERT(!next[2] || (next[2]->next[0] != NULL && next[2]->next[1] != NULL));
		//OP_ASSERT(!next[2] || (next[2]->items[1] == NULL && next[2]->next[2] == NULL)||
		//	(next[2]->items[1] != NULL && next[2]->next[2] != NULL));
	*/

	return (items[0] ? NULL : sibling);
}

B23Tree_Item *B23Tree_Node::Search(void *search_param)
{
	if(items[0] == NULL)
		return NULL;

	B23Tree_Item *match = NULL;

	int cmpval;
	int index = 0;

	cmpval = items[0]->SearchCompare(search_param);
	if(cmpval <0)
	{
		index++;
		if(items[1])
		{
			cmpval = items[1]->SearchCompare(search_param);
			if(cmpval <0)
				index++;
		}
	}
	
	if(cmpval == 0)
		match = items[index];
	else if(next[index])
	{
		match = next[index]->Search(search_param);
	}

	return match;
}

B23Tree_Item* B23Tree_Node::LastLeaf()
{
	B23Tree_Node *node = (next[2] ? next[2] : next[1]);

	if(node)
		return node->LastLeaf();
	
	return items[1] ? items[1] : items[0];
}

B23Tree_Item* B23Tree_Node::FirstLeaf()
{
	return (next[0] ? next[0]->FirstLeaf() : items[0]);
}

B23Tree_Store::B23Tree_Store(B23_CompareFunction comp)
: top(NULL), compare(comp)
{
}

B23Tree_Store::~B23Tree_Store()
{
	OP_DELETE(top);
	top = NULL;
	compare = NULL;
}

B23Tree_Node *B23Tree_Store::ConstructNodeL()
{
	return OP_NEW_L(B23Tree_Node, (compare));
}

void B23Tree_Store::InsertL(B23Tree_Item *item)
{
	if(item == NULL)
		return;

	if(!top)
	{
		OpStackAutoPtr<B23Tree_Item> item1(item);
		top = ConstructNodeL();
		item1.release();
		top->SetParentStore(this);
	}
	
	B23Tree_Node *new_top = top->InsertL(item);
	if(new_top)
	{
		top = new_top;
		top->SetParentStore(this);
	}
}

void B23Tree_Store::Delete(B23Tree_Item *item, BOOL delete_item)
{
	if(top)
	{
		B23Tree_Node *new_top = top->Delete(item, delete_item);
		if(new_top)
		{
			top->next[0] = NULL;
			OP_DELETE(top);
			top = new_top;
			top->SetParentStore(this);
		}
	}
}

void B23Tree_Store::TraverseL(uint32 action, void *params)
{
	if(top)
		top->TraverseL(action, params);
}

B23Tree_Item *B23Tree_Store::Search(void *search_param)
{
	return (top ? top->Search(search_param) : NULL);
}

B23Tree_Item* B23Tree_Store::LastLeaf()
{
	return (top ? top->LastLeaf() : NULL);
}

BOOL B23Tree_Store::Empty()
{
	return (top && top->AccessItem(0) != NULL ? FALSE : TRUE);
}

void B23Tree_Store::Clear()
{
	OP_DELETE(top);
	top = NULL;
}
