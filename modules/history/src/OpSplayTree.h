/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef OP_SPLAY_TREE_H
#define OP_SPLAY_TREE_H

#ifdef HISTORY_SUPPORT

#include "modules/util/OpTypedObject.h"
#include "modules/util/adt/opvector.h"
#include "modules/history/src/lex_splay_key.h"
#include "modules/history/LexSplayTree.h"

template<class InternalType, class LeafType>
class OpSplayTree : protected LexSplayTree
{
public:

	/**
	 * Constructor
	 * @param num of prefixes that should be supported in the payload
	 **/
	OpSplayTree<InternalType, LeafType>(INT32 num = 1)
	{
		m_num_prefixes = num;
		m_num_of_payloads = 0;
	}

	/**
	 * Destructor
	 **/
    virtual ~OpSplayTree()
	{
		DeleteTree();

		//All nodes should have been deleted
		OP_ASSERT(num_of_nodes == 0);		
		//All leaf node arrays should have been deleted
		OP_ASSERT(m_num_of_payloads == 0);
	}

	/**
	 * Gets the insert location for a Leaf Node with a specific key
	 * @param splay_key
	 * @param prefix_num
	 * @return pointer to insert location or 0 on ERR_NO_MEMORY
	 **/
    LeafType** InsertLeafNode(const LexSplayKey* splay_key,
							   INT32 prefix_num = 0) 
	{ 
		if(!splay_key)
			return 0;
		
		// Will create the path if necessary
		LexSplayNode* c_node = insertHelper(splay_key->GetKey());
		
		if(!c_node)
			return 0;
		
		// If there is a node and it does not have a key_item  - set the key
		if(!HasKeyItem(c_node))
			RETURN_VALUE_IF_ERROR(SetLexSplayKey(c_node, splay_key), 0);
		
		if(!HasItems(c_node))
			RETURN_VALUE_IF_ERROR(CreateItems(c_node), 0); // TODO Maybe pruning is in order?
		
		// Prefix number has to be in the range :
		OP_ASSERT(prefix_num >= 0 && prefix_num < m_num_prefixes);
		if(!(prefix_num >= 0 && prefix_num < m_num_prefixes))
			return 0;
		
		return GetLeafNodePtr(c_node, prefix_num);
	}

	/**
	 * Gets the insert location for a Internal Node with a specific key
	 * @param splay_key
	 * @return pointer to insert location or 0 on ERR_NO_MEMORY
	 **/
	InternalType** InsertInternalNode(const LexSplayKey* splay_key)
	{
		if(!splay_key)
			return 0;
		
		// Will create the path if necessary
		LexSplayNode* c_node = insertHelper(splay_key->GetKey());
		
		if(!c_node)
			return 0;
		
		// If there is a node and it does not have a key_item - set the key
		if(!HasKeyItem(c_node))
			SetLexSplayKey(c_node, splay_key);
		
		return GetInternalNodePtr(c_node);
	}

	/**
	 * Gets the LexSplayKey for a specific key string if it is present in the tree
	 * @param key_string
	 * @return pointer to the matching LexSplayKey if it is present
	 **/
	LexSplayKey* GetKey(const uni_char* key_string)
	{
		if(!key_string)
			return 0;
		
		access(key_string, root); //Will splay first char in c to root if it is in the tree
		
		LexSplayNode* subtree = Find(root, key_string, PERFECT_MATCH);
		
		//If such a node exists return pointer to the key item
		if(HasKeyItem(subtree))
			return GetKeyItem(subtree);
		
		return 0;
	}

	/**
	 * Removes an Internal Node by key
	 * @param lookup_string
	 * @return TRUE if such a node was removed
	 **/
	BOOL RemoveInternalNode(const uni_char* lookup_string)
	{
		LexSplayNode* subtree = Find(root, lookup_string, PERFECT_MATCH);

		if (!subtree)
			return FALSE;
		
		BOOL return_value = FALSE;

		//If such a node exists remove reference to internal_node
		if(HasInternalNode(subtree))
			return_value = TRUE;

		ClearInternalNode(subtree);
		CleanNode(subtree);
		PruneBranch(subtree);
		
		return return_value;
	}

	/**
	 * Removes a Leaf Node by key
	 * @param lookup_string
	 * @param prefix_num
	 * @return TRUE if such a node was removed
	 **/
	BOOL RemoveLeafNode(const uni_char* lookup_string,
						INT32 prefix_num)
	{
		LexSplayNode* subtree = Find(root, lookup_string, PERFECT_MATCH);

		if (!subtree)
			return FALSE;
		
		BOOL return_value = FALSE;

		//If such a node exists remove reference to leaf_node
		if(HasItem(subtree, prefix_num))
			return_value = TRUE;

		ClearItem(subtree, prefix_num);
		CleanNode(subtree);
		PruneBranch(subtree);
		
		return return_value;
	}

	/**
	 * Finds a Leaf by key and prefix number
	 * @param key_string
	 * @param prefix_num
	 * @return pointer to Leaf node if found
	 **/
	LeafType* FindLeafNode(const uni_char* key_string,
						   INT32 prefix_num = 0) const
	{ 
		LexSplayNode* subtree = Find(root, key_string, PERFECT_MATCH);
		return GetItem(subtree, prefix_num);
	}

	/**
	 * Finds all Leafs in the tree and adds them to the vector
	 * @param vector
	 * @param type
	 * @param prefix_num
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 **/
    OP_STATUS FindAllLeafNodes(OpVector<LeafType> &vector, 
							   OpTypedObject::Type type = OpTypedObject::UNKNOWN_TYPE, 
							   INT32 prefix_num = KAll) const
	{
		if(root)
			RETURN_IF_ERROR(FindAll(root, vector, type, prefix_num));
		
		return OpStatus::OK;
	}

	/**
	 * Finds all Leafs in the tree matching the lookup string and adds them to the vector
	 * @param vector
	 * @param key_string
	 * @param type
	 * @param prefix_num
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 **/
    OP_STATUS FindLeafNodes(OpVector<LeafType> &vector, 
							const uni_char* key_string, 
							OpTypedObject::Type type, 
							INT32 prefix_num = KAll) const
	{
		return FindLeafNodes(root, vector, key_string, type, prefix_num);
	}

	/**
	 *
	 * @param vector
	 * @param key_string
	 * @param type
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 **/
	OP_STATUS FindExactLeafNodes(OpVector<LeafType> &vector, 
								 const uni_char* key_string, 
								 OpTypedObject::Type type) const
	{
		LexSplayNode* subtree = Find(root, key_string, LONGEST_PREFIX_MATCH, TRUE);
		
		if(subtree)
		{
			//A terminating node will have leaf_nodes
			for(INT32 i = 0; i < m_num_prefixes; i++)
			{
				if(HasItemOfType(subtree, i, type))
					RETURN_IF_ERROR(AddItem(subtree, vector, i));
			}
		}
		
		return OpStatus::OK;
	}

	/**
	 *
	 * @param key_string
	 * @param vector
	 * @param type
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 **/
	OP_STATUS GetLeafNodesOfSubtree(const uni_char* key_string, 
									OpVector<LeafType> &vector, 
									OpTypedObject::Type type) const
	{
		LexSplayNode* subtree = Find(root, key_string, PERFECT_MATCH);
		
		if(HasInternalNode(subtree))
		{
			//Get leaf_nodes with name exactly like internal_node name
			for(INT32 i = 0; i < m_num_prefixes; i++)
				if(HasItemOfType(subtree, i, type))
					RETURN_IF_ERROR(AddItem(subtree, vector, i));
			
			//This is the matching char - all internal_node leaf_nodes will be in center subtree
			if(subtree->m_center)
				RETURN_IF_ERROR(FindAll(subtree->m_center, vector, type));
		}
		
		return OpStatus::OK;
	}

	/**
	 *
	 * @param vector
	 * @param key_string
	 * @param type
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 **/
	OP_STATUS FindExactInternalNodes(OpVector<InternalType> &vector, 
									 const uni_char* key_string, 
									 OpTypedObject::Type type) const
	{
		uni_char* last_char = 0;
		LexSplayNode* subtree = Find(root, key_string, LONGEST_PREFIX_MATCH, TRUE, TRUE, &last_char);
		
		if(HasInternalNodeOfType(subtree, type))
		{
			RETURN_IF_ERROR(AddInternalNode(subtree, vector));
			
			if(last_char && *last_char)
				RETURN_IF_ERROR(FindLeafNodes(subtree->m_center, vector, last_char, type));
			else
				RETURN_IF_ERROR(FindAll(subtree->m_center, vector, type));
		}
		
		return OpStatus::OK;
	}

	/**
	 *
	 * @param item
	 * @param key_string
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 **/
	OP_STATUS FindPrefixInternalNode(InternalType*& item, 
									 const uni_char* key_string) const
	{
		LexSplayNode* subtree = 0;
		
		if(root)
			subtree = Find(root, key_string, LONGEST_PREFIX_MATCH, TRUE, TRUE);
		
		item = GetInternalNode(subtree);
		
		return OpStatus::OK;
	}


#ifdef HISTORY_DEBUG

	INT32 GetNumberOfNodes() const { return num_of_nodes; }

	INT32 GetNodeSize() const { return sizeof(LexSplayNode); }

	INT32 GetNumberOfPayloads() const { return m_num_of_payloads; }

	INT32 GetPayloadSize() const { return sizeof(SplayNode) + (sizeof(void*) * m_num_prefixes); }

	INT32 GetTreeSize() const { return 
									(GetNumberOfNodes()*GetNodeSize()) +
									(GetNumberOfPayloads()*GetPayloadSize());}
#endif // HISTORY_DEBUG

protected:

	// LEAF
	// INTERNAL
	// This is just wrong in so many ways....
	virtual OP_STATUS AddInternalNodeAsLeaf(InternalType* item,
											OpVector<LeafType> &in_vector) const
	{
		OP_ASSERT(FALSE);
		return OpStatus::OK;
	}

	virtual BOOL HasPayloadContent(LexSplayNode* node) const
	{
		return HasContent(node);
	}

	virtual void DeletePayload(LexSplayNode* node)
	{
		DeleteSplayNode(node);
	}

	// KEY
	virtual void CopyItems(LexSplayNode* from_node, LexSplayNode* to_node)
	{
		OP_ASSERT(from_node && to_node && from_node != to_node);
		
		if(!from_node || !to_node || from_node == to_node)
			return;
		
		// Note that ownership of m_leaf_nodes is transferred from from_node to to_node
		TransferLeafs(from_node, to_node);

		SetLexSplayKey(to_node, GetKeyItem(from_node));
		SetInternalNode(to_node, GetInternalNode(from_node));
	}

	// INTERNAL
	virtual BOOL HasInternalNode(LexSplayNode* node) const
	{
		return HasInternalNode(GetSplayNode(node));
	}

private:

	// -------------------
	// Fields:
	// -------------------

    INT32 m_num_prefixes;
	INT32 m_num_of_payloads;

	// LEAF
	// INTERNAL
    BOOL HasContent(LexSplayNode* subtree) const
	{
		return HasInternalNode(subtree) || ContainsItems(subtree);
	}

	// LEAF
	void DeleteLeafNodes(LexSplayNode* node)
	{
		DeleteLeafNodes(GetSplayNode(node));
	}

	// INTERNAL
	OP_STATUS AddInternalNode(LexSplayNode* node,
							  OpVector<InternalType> &in_vector) const
	{
	    InternalType* item = GetInternalNode(node);
		return item ? in_vector.Add(item) : OpStatus::OK;
	}

	// INTERNAL
	void ClearInternalNode(LexSplayNode* node)
	{
		ClearInternalNode(GetSplayNode(node));
	}

	// INTERNAL
	BOOL HasInternalNodeOfType(LexSplayNode* node, OpTypedObject::Type type) const
	{
		InternalType * item = GetInternalNode(node);
		return item && item->GetType() == type;
	}

	// LEAF
	OP_STATUS CollectNodeItems(LexSplayNode* subtree,
							   OpVector<LeafType> &vector,
							   OpTypedObject::Type type) const
	{
		if(!subtree)
			return OpStatus::OK;
	
  		if(HasInternalNodeOfType(subtree, type))
			RETURN_IF_ERROR(AddInternalNodeAsLeaf(GetInternalNode(subtree),
												  vector));
	   
		for(INT32 i = 0; i < m_num_prefixes; i++)
			RETURN_IF_ERROR(CollectNodeItem(subtree, vector, type, i));
		
		return OpStatus::OK;
	}

	// LEAF
	inline OP_STATUS CollectNodeItem(LexSplayNode* subtree,
									 OpVector<LeafType> &vector,
									 OpTypedObject::Type type,
									 INT32 prefix_num  = KAll) const
	{
		if(HasItemOfType(subtree, prefix_num, type))
			RETURN_IF_ERROR(AddItem(subtree, vector, prefix_num));
		
		return OpStatus::OK;
	}

	// LEAF
	OP_STATUS FindAll(LexSplayNode* subtree,
					  OpVector<LeafType> &vector,
					  OpTypedObject::Type type,
					  INT32 prefix_num = KAll) const
	{
		if(!subtree)
			return OpStatus::OK;
		
		//Tree is sorted - this recursion will give alphabetical order
		if(subtree->m_left)   RETURN_IF_ERROR(FindAll(subtree->m_left, vector, type, prefix_num));
		
		if(prefix_num == KAll)
			RETURN_IF_ERROR(CollectNodeItems(subtree, vector, type));
		else
			RETURN_IF_ERROR(CollectNodeItem(subtree, vector, type, prefix_num));
		
		if(subtree->m_center) RETURN_IF_ERROR(FindAll(subtree->m_center, vector, type, prefix_num));
		if(subtree->m_right)  RETURN_IF_ERROR(FindAll(subtree->m_right, vector, type, prefix_num));
		
		return OpStatus::OK;
	}

	// LEAF
    OP_STATUS FindLeafNodes(LexSplayNode* subtree,
							OpVector<LeafType> &vector,
							const uni_char * lookup_string,
							OpTypedObject::Type type,
							INT32 prefix_num = KAll) const
	{
		if(subtree)
			subtree = Find(subtree, lookup_string, PREFIX_MATCH);
		
		//If such a node exists then the center subtree is a tree
		//of strings having matchstring as prefix
		//find all these strings and add them to vector
		
		if(subtree)
		{
			if(prefix_num == KAll)
				RETURN_IF_ERROR(CollectNodeItems(subtree, vector, type));
			else
				RETURN_IF_ERROR(CollectNodeItem(subtree, vector, type, prefix_num));
			
			RETURN_IF_ERROR(FindAll(subtree->m_center, vector, type, prefix_num));
		}
		
		return OpStatus::OK;
	}
	
	// LEAF
	OP_STATUS AddItem(LexSplayNode* node,
					  OpVector<LeafType> &in_vector,
					  INT32 prefix_num) const
	{
		LeafType * item = GetItem(node, prefix_num);
		return item ? in_vector.Add(item) : OpStatus::OK;
	}

	// LEAF
	inline BOOL HasItemOfType(LexSplayNode* node, INT32 prefix_num, OpTypedObject::Type type) const
	{
		LeafType* item = GetItem(node, prefix_num);
		return item && (item->GetType() == type || type == OpTypedObject::UNKNOWN_TYPE);
	}

	// LEAF
	BOOL ContainsItems(LexSplayNode* subtree) const
	{
		for(INT32 i = 0; i < m_num_prefixes; i++)
			if(HasItem(subtree, i))
				return TRUE;
		
		return FALSE;
	}

	// LEAF
	OP_STATUS CreateItems(LexSplayNode* node)
	{
		if(!node || HasItems(node))
			return OpStatus::OK;
		
		RETURN_IF_ERROR(CreateLeafs(node));
		
		if(HasItems(node))
		{
			for(INT32 i = 0; i < m_num_prefixes; i++)
				ClearItem(node, i);
			
			return OpStatus::OK;
		}
		
		return OpStatus::ERR_NO_MEMORY;
	}

	// ------------------------------
	// SplayNode Handling
	// ------------------------------

    struct SplayNode
	{
		LexSplayKey  *  m_key_item;
		LeafType     ** m_leaf_nodes;
		InternalType *  m_internal_node;

		SplayNode() : m_key_item(0),
					  m_leaf_nodes(0),
					  m_internal_node(0)
		{ }

		~SplayNode()
		{ 
		}
    };

	SplayNode* GetSplayNode(LexSplayNode* node) const
	{
		return node ? static_cast<SplayNode*>(node->m_pay_load) : 0;
	}

	BOOL HasPayload(LexSplayNode* node) const
	{
		return node->m_pay_load != 0;
	}

	OP_STATUS CreateSplayNode(LexSplayNode* node)
	{
		if(!node)
			return OpStatus::ERR;

		// If it already has a payload there is no reason to make one
		if(HasPayload(node))
			return OpStatus::OK;

		OpAutoPtr<SplayNode> splay_node(OP_NEW(SplayNode, ()));

		if(!splay_node.get())
			return OpStatus::ERR_NO_MEMORY;

		// Create the leaf array
		splay_node->m_leaf_nodes = OP_NEWA(LeafType*, m_num_prefixes);

		if(splay_node->m_leaf_nodes == 0)
			return OpStatus::ERR_NO_MEMORY;

		// Init the leaf array
		for(int i = 0; i < m_num_prefixes; i++)
			splay_node->m_leaf_nodes[i] = 0;

		node->m_pay_load = splay_node.release();
		m_num_of_payloads++;

		return OpStatus::OK;
	}

	void DeleteLeafNodes(SplayNode* splay_node)
	{
		if(!splay_node)
			return;
		
		OP_DELETEA(splay_node->m_leaf_nodes);
		splay_node->m_leaf_nodes = 0;
	}

	void CleanNode(LexSplayNode* subtree)
	{
		if(HasContent(subtree))
			return;

		// If there is nothing here then dump the splaynode
		DeleteSplayNode(subtree);
	}

	void DeleteSplayNode(LexSplayNode* subtree)
	{
		SplayNode* splay_node = GetSplayNode(subtree);

		subtree->m_key = 0;
		subtree->m_pay_load = 0;

		DeleteLeafNodes(splay_node);

		if(splay_node)
		{
			delete splay_node;
			m_num_of_payloads--;
		}
	}

	OP_STATUS SetLexSplayKey(LexSplayNode* node, const LexSplayKey* splay_key)
	{
		RETURN_IF_ERROR(CreateSplayNode(node));
		GetSplayNode(node)->m_key_item = const_cast<LexSplayKey*>(splay_key);
		return OpStatus::OK;
	}

	OP_STATUS SetInternalNode(LexSplayNode* node, InternalType* item)
	{
		RETURN_IF_ERROR(CreateSplayNode(node));
		GetSplayNode(node)->m_internal_node = item;
		return OpStatus::OK;
	}

	LeafType** GetLeafNodePtr(LexSplayNode* node, INT32 prefix_num)
	{ 
		RETURN_VALUE_IF_ERROR(CreateSplayNode(node), NULL);
		return &GetSplayNode(node)->m_leaf_nodes[prefix_num];
	}

	InternalType** GetInternalNodePtr(LexSplayNode* node)
	{ 
		RETURN_VALUE_IF_ERROR(CreateSplayNode(node), NULL);
		return &GetSplayNode(node)->m_internal_node;
	}

	InternalType * GetInternalNode(LexSplayNode* node) const
	{
		return GetSplayNode(node) ? GetSplayNode(node)->m_internal_node : 0;
	}

	LexSplayKey* GetKeyItem(LexSplayNode* node) const
	{
		return GetSplayNode(node)->m_key_item;
	}

	inline LeafType * GetItem(LexSplayNode* node, INT32 prefix_num) const
	{
		return HasItem(node, prefix_num) ? GetSplayNode(node)->m_leaf_nodes[prefix_num] : 0;
	}

	inline BOOL HasItems(LexSplayNode* node) const
	{
		return HasItems(GetSplayNode(node));
	}

	inline BOOL HasItems(SplayNode* node) const
	{
		return node && node->m_leaf_nodes != 0;
	}

	inline BOOL HasItem(LexSplayNode* node, INT32 prefix_num) const
	{
		if(!node)
			return FALSE;
		
		OP_ASSERT(prefix_num >= 0 && prefix_num < m_num_prefixes);
		if(!(prefix_num >= 0 && prefix_num < m_num_prefixes))
			return FALSE;
		
		return HasItems(node) && GetSplayNode(node)->m_leaf_nodes[prefix_num] != 0;
	}

	BOOL HasInternalNode(SplayNode* node) const
	{
		return node && node->m_internal_node != 0;
	}

	BOOL HasKeyItem(LexSplayNode* node) const
	{
		return GetSplayNode(node) && GetSplayNode(node)->m_key_item != 0;
	}

	void ClearInternalNode(SplayNode* node)
	{
		if(node)
			node->m_internal_node = 0;
	}

	void ClearItem(LexSplayNode* node, INT32 prefix_num)
	{
		// Prefix number has to be in the range :
		OP_ASSERT(prefix_num >= 0 && prefix_num < m_num_prefixes);
		if(!(prefix_num >= 0 && prefix_num < m_num_prefixes))
			return;
		
		if(HasItems(node))
			GetSplayNode(node)->m_leaf_nodes[prefix_num] = 0;
	}

	OP_STATUS TransferLeafs(LexSplayNode* from_node, LexSplayNode* to_node)
	{
		// If neither has anything then there is nothing to do
		if(!HasPayload(from_node) && !HasPayload(to_node))
			return OpStatus::OK;

		// If to doesn't have anything then there is something in from and
		// to will have to have something too
		if(!HasPayload(to_node))
			RETURN_IF_ERROR(CreateSplayNode(to_node));

		// Use the leafs in from if there are any
		if(GetSplayNode(from_node))
		{
			DeleteLeafNodes(to_node);
			GetSplayNode(to_node)->m_leaf_nodes = GetSplayNode(from_node)->m_leaf_nodes;
		}

		// Release ownership of the leafs
		if(GetSplayNode(from_node))
			GetSplayNode(from_node)->m_leaf_nodes = 0;

		return OpStatus::OK;
	}

	OP_STATUS CreateLeafs(LexSplayNode* node)
	{
		// Create the payload if it does not exist
		return CreateSplayNode(node);
	}

	// ------------------------------
	// Debugging
	// ------------------------------

#ifdef SPLAY_DEBUG
	static void makeTabs(INT32 tabs)
	{
		for(INT32 i = 0; i < tabs; i++)
			printf("| ");
	}
	
	void printTree(LexSplayNode* node,
				   INT32 numTabs)
	{
		if(!node)
			return;
		
		if(node->m_left)
			printTree(node->m_left, numTabs+1);
		
		makeTabs(numTabs);
		
		if(node->m_key)
		{
			OpString8 tmp;
			tmp.Set(node->m_key);
			printf("%s\n", tmp.CStr());
		}
		else
		{
			printf("%c\n", node->m_char);
		}
		
		if(node->m_center)
			printTree(node->m_center, numTabs+1);
		
		if(node->m_right)
			printTree(node->m_right, numTabs+1);
	}

	
	void printTree2(LexSplayNode* node)
	{
		if(!node)
		{
			printf("()");
			return;
		}
		
		printf("(");
		
		if(node->m_key)
		{
			OpString8 tmp;
			tmp.Set(node->m_key);
			printf("%s ", tmp.CStr());
		}
		else
		{
			printf("%c ", node->m_char);
		}
		
		if(node->m_left   ||
		   node->m_center ||
		   node->m_right)
		{
			printTree2(node->m_left);
			printTree2(node->m_center);
			printTree2(node->m_right);
		}
		
		printf(")");
	}
#endif //SPLAY_DEBUG
};

template<class NodeType>
class OpSimpleSplayTree : protected OpSplayTree<NodeType, NodeType>
{
public:
	OpSimpleSplayTree(INT32 num = 1) : OpSplayTree<NodeType, NodeType>(num) { }

	LexSplayKey * GetKey(const uni_char * key_string)
		{
			return OpSplayTree<NodeType, NodeType>::GetKey(key_string);
		}

    NodeType ** InsertLeafNode(const LexSplayKey * splay_key,
							   INT32 prefix_num = 0) 
		{ 
			return OpSplayTree<NodeType, NodeType>::InsertLeafNode(splay_key, prefix_num);
		}

	NodeType ** InsertInternalNode(const LexSplayKey * splay_key)
		{ 
			return OpSplayTree<NodeType, NodeType>::InsertInternalNode(splay_key); 
		}

	OP_STATUS FindPrefixInternalNode(NodeType *& item, 
									 const uni_char * key_string) const
		{
			return OpSplayTree<NodeType, NodeType>::FindPrefixInternalNode(item, 
																		   key_string);
		}

	OP_STATUS FindExactInternalNodes(OpVector<NodeType> &vector, 
									 const uni_char * key_string, 
									 OpTypedObject::Type type) const
		{
			return OpSplayTree<NodeType, NodeType>::FindExactInternalNodes(vector, 
																		   key_string, 
																		   type);
		}

	OP_STATUS FindExactLeafNodes(OpVector<NodeType> &vector, 
								 const uni_char * key_string, 
								 OpTypedObject::Type type) const
		{
			return OpSplayTree<NodeType, NodeType>::FindExactLeafNodes(vector, 
																	   key_string, 
																	   type);
		}

#ifdef HISTORY_DEBUG

	INT32 GetNumberOfNodes() const
		{ 
			return OpSplayTree<NodeType, NodeType>::GetNumberOfNodes();
		}

	INT32 GetNodeSize() const
		{ 
			return OpSplayTree<NodeType, NodeType>::GetNodeSize();
		}

	INT32 GetNumberOfPayloads()
		{ 
			return OpSplayTree<NodeType, NodeType>::GetNumberOfPayloads();
		}

	INT32 GetTreeSize() const
		{ 
			return OpSplayTree<NodeType, NodeType>::GetTreeSize();
		}

#endif // HISTORY_DEBUG

protected:

	virtual OP_STATUS AddInternalNodeAsLeaf(NodeType* item,
											OpVector<NodeType> &in_vector) const
	{
		return item ? in_vector.Add(item) : OpStatus::OK;
	}
};

#endif // HISTORY_SUPPORT
#endif // OP_SPLAY_TREE_H
