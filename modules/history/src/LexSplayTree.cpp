/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 * psmaas - Patricia Aas
 */

#include "core/pch.h"

#ifdef HISTORY_SUPPORT

#include "modules/history/LexSplayTree.h"

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
LexSplayTree::LexSplayTree()
{
    root = 0;
    num_of_nodes = 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void LexSplayTree::replaceSubtree(LexSplayNode* parent,
								  LexSplayNode* current,
								  LexSplayNode* new_child)
{
    if(current == root)
    {
		root  = new_child;
    }
    else if(isLeft(current, parent))
    {
		setLeft(new_child, parent);
    }
    else if(isRight(current, parent))
    {
		setRight(new_child, parent);
    }
    else if(isCenter(current, parent))
    {
		setCenter(new_child, parent);
    }
    else
    {
		//Child should be left, right or center
		OP_ASSERT(FALSE);
    }
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void LexSplayTree::rotateLeft(LexSplayNode* node)
{
    LexSplayNode* top = node->m_right;
    LexSplayNode* p   = parent(node);
    
    if(p)
		replaceSubtree(p, node, top);
    else
		top->m_parent = 0;
    
    setRight(top->m_left, node);
    setLeft(node, top);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void LexSplayTree::rotateRight(LexSplayNode* node)
{
    LexSplayNode* top = node->m_left;
    LexSplayNode* p   = parent(node);
    
    if(p)
		replaceSubtree(p, node, top);
    else
		top->m_parent = 0;
    
    setLeft(top->m_right, node);
    setRight(node, top);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void LexSplayTree::access(const uni_char* c,
						  LexSplayNode* node)
{
    if(!node)
		return;
    
    LexSplayNode* subtree    = FindFirst(node, c, TRUE);
    LexSplayNode* splay_node = subtree;
    BOOL done = FALSE;
    
    if(subtree)
    {
		while(!done)
		{
			UINT32 inc = 0;
			subtree = NextHelper(subtree, c, &inc, done, PREFIX_MATCH, TRUE);
			c+=inc;
	    
			if(subtree)
				splay_node = subtree;
		}
    }
    
    splay(splay_node);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void LexSplayTree::splay(LexSplayNode* node)
{
    while(parent(node))
    {
		if(isLeft(node, parent(node)))
		{
			if(grandparent(node) == 0)
			{
				rotateRight(parent(node));
			}
			else if(isLeft(parent(node), grandparent(node)))
			{
				rotateRight(grandparent(node));
				rotateRight(parent(node));
			}
			else if(isRight(parent(node), grandparent(node)))
			{
				rotateRight(parent(node));
				rotateLeft(parent(node));
			}
			else if(isCenter(parent(node), grandparent(node)))
			{
				rotateRight(parent(node));
			}
			else
			{
				//Parent-grandparent link broken
				OP_ASSERT(FALSE);
			}
		}
		else if(isRight(node, parent(node)))
		{
	    
			if(grandparent(node) == 0)
			{
				rotateLeft(parent(node));
			}
			else if(isRight(parent(node), grandparent(node)))
			{
				rotateLeft(grandparent(node));
				rotateLeft(parent(node));
			}
			else if(isLeft(parent(node), grandparent(node)))
			{
				rotateLeft(parent(node));
				rotateRight(parent(node));
			}
			else if(isCenter(parent(node), grandparent(node)))
			{
				rotateLeft(parent(node));
			}
			else
			{
				//Parent-grandparent link broken
				OP_ASSERT(FALSE);
			}
		}
		else if(isCenter(node, parent(node)))
		{
			node = parent(node);
		}
		else
		{
			//Child is not child of parent
			OP_ASSERT(FALSE);
		}
	
		if(parent(node) == 0)
		{
			root = node;
			break;
		}
    }
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL LexSplayTree::EmptyLeaf(LexSplayNode* subtree) const
{
	return !subtree->m_left && !subtree->m_center && !subtree->m_right && !HasPayloadContent(subtree);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
LexSplayTree::LexSplayNode* LexSplayTree::FindFirst(LexSplayNode* current_node,
													const uni_char* c,
													BOOL include_failed_node) const
{
    if(!current_node)
		return 0;
    
    LexSplayNode* subtree = current_node;
    
    while(1)
    {
		if((subtree->m_left) && (*c < subtree->m_char))
		{
			subtree = subtree->m_left;
		}
		else if((subtree->m_right) && (*c > subtree->m_char))
		{
			subtree = subtree->m_right;
		}
		else if(*c == subtree->m_char)
		{
			break;
		}
		else
		{
			if(!include_failed_node)
				subtree = 0;
			break;
		}
    }
    
    return subtree;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
LexSplayTree::LexSplayNode* LexSplayTree::makeNode(const uni_char* c)
{
    LexSplayNode* node = OP_NEW(LexSplayNode, (*c));
    
    if(!node)
		return 0;
    
    num_of_nodes++;
    
    return node;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
LexSplayTree::LexSplayNode* LexSplayTree::makeCompressedNode(const uni_char* c)
{
    LexSplayNode* subtree = makeNode(c);
    
    if(!subtree)
		return 0; //Out of memory
    
    SetKey(subtree, (uni_char*) c);
    
    return subtree;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL LexSplayTree::IsEqual(LexSplayNode* node,
						   const uni_char* c) const
{
    if(!node)
		return FALSE;
    
    if( !IsCompressed(node) )
		return FALSE;
    
    const uni_char* key = GetKey(node);
    
    return uni_strcmp(key, c) == 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL LexSplayTree::StartsWith(LexSplayNode* node,
							  const uni_char* c) const
{
    if(!node)
		return FALSE;
    
    if( !IsCompressed(node) )
		return FALSE;
    
    const uni_char* key = GetKey(node);
    
    size_t len = uni_strlen(c);
    
    return uni_strncmp(key, c, len) == 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL LexSplayTree::Matches(LexSplayNode* node,
						   const uni_char* c) const
{
    if(!node)
		return FALSE;
    
    if( !IsCompressed(node) )
		return FALSE;
    
    const uni_char* key = GetKey(node);
    
    size_t len = uni_strlen(key);
    
    return uni_strncmp(c, key, len) == 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL LexSplayTree::InsertChild(LexSplayNode* child, LexSplayNode* parent)
{
    if(!child || !parent)
		return FALSE;
    
    if(child->m_char < parent->m_char)
    {
		setLeft(child, parent);
	
		if(!parent->m_left)
		{
			PruneBranch(parent);
			return FALSE;
		}
    }
    else if (child->m_char > parent->m_char)
    {
		setRight(child, parent);
	
		if(!parent->m_right)
		{
			PruneBranch(parent);
			return FALSE;
		}
    }
    else
    {
		OP_ASSERT(FALSE);
		return FALSE;
    }
    
    return TRUE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL LexSplayTree::InsertCenterChild(LexSplayNode* child, LexSplayNode* parent)
{
    if(!child || !parent)
		return FALSE;
    
    // Make the split node :
    setCenter(child, parent);
    
    if(!parent->m_center)
    {
		PruneBranch(parent);
		return FALSE;
    }
    
    return TRUE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void LexSplayTree::DeleteNode(LexSplayNode* node)
{
    if(node->m_left)   DeleteNode(node->m_left);
    if(node->m_center) DeleteNode(node->m_center);
    if(node->m_right)  DeleteNode(node->m_right);
    
    num_of_nodes--;
    
    DeletePayload(node);
    delete node;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void LexSplayTree::DeleteTree()
{
    if(root)
		DeleteNode(root);
    root = 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void LexSplayTree::PruneBranch(LexSplayNode* subtree)
{
    LexSplayNode* p = 0;
    
    while(subtree)
    {
		if(!EmptyLeaf(subtree)) break;
	
		p = parent(subtree);
	
		if(p)
			replaceSubtree(p, subtree, 0);
		else if(subtree == root) //No parent - could be root
			root = 0;
	
		DeleteNode(subtree);
	
		subtree = p;
    }
}


/***********************************************************************************
 **
 **
 **
 ** Find lex node for final char in c (if present) - if not return 0
 **
 ** I think maybe the last else should always be done - but I'm not sure if it will work
 **
 **                             Default
 ** @param node                -       - The node to start searching from
 ** @param c                   -       - The string we are looking for
 ** @param match_type          -       - The type of match we want: 
 **                                      LONGEST_PREFIX_MATCH  - the longest string found in the
 **                                                     tree that is a prefix of the string we are
 **                                                     searching with (that is the string c)
 **                                      c : abcdef
 **                                      s : abc
 **                                 	 PERFECT_MATCH - the string found is a perfect match to the
 **                                                     string we are searching with - no more no less
 **                                      c : abcdef
 **                                      s : abcdef
 **                                      PREFIX_MATCH - if the string found starts with the string
 **                                                     we are searching with. That is that c is a
 **                                                     prefix of the string found.
 **                                      c : abc
 **                                      s : abcedf
 ** @param keep_last_node      - FALSE -
 ** @param keep_only_internal_nodes   - FALSE -
 ** @param last_char           - 0     - The last character processed in c
 **
 **
 ***********************************************************************************/
LexSplayTree::LexSplayNode* LexSplayTree::Find(LexSplayNode* node,
											   const uni_char* c,
											   MatchType match_type,
											   BOOL keep_last_node,
											   BOOL keep_only_internal_nodes,
											   uni_char** last_char) const
{
    if(!c)
		return 0;
    
    BOOL done = FALSE;
    LexSplayNode* subtree = FindFirst(node, c);
    LexSplayNode* return_node = subtree;
    
    if(subtree)
    {
		if(!LastNode(subtree, c))
		{
			while(!done)
			{
				UINT32 inc = 0;
				subtree = NextHelper(subtree, c, &inc, done, match_type, FALSE);
				
				if((subtree || !keep_last_node) &&
				   (HasInternalNode(subtree) || !keep_only_internal_nodes))
				{
					if(match_type == PERFECT_MATCH && !PerfectMatch(subtree, c))
						return_node = 0;
					else
						return_node = subtree;
				}
				
				c+=inc;
			}
		}
		else
		{
			switch(match_type)
			{
			case PERFECT_MATCH:
				if(!PerfectMatch(subtree, c))
					return_node = 0;
				break;

			case LONGEST_PREFIX_MATCH:
				if(!Matches(subtree, c))
					return_node = 0;
				break;

			case PREFIX_MATCH:
				if(IsCompressed(subtree) && !StartsWith(subtree, c))
					return_node = 0;
				break;
			}
		}
	}
    
    if(last_char && !LastNode(return_node, c))
		*last_char = (uni_char*) c;
    
    return return_node;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
LexSplayTree::LexSplayNode* LexSplayTree::NextHelper(LexSplayNode* current_node,
													 const uni_char* c,
													 UINT32* inc,
													 BOOL & done,
													 MatchType match_type,
													 BOOL include_failed_node) const
{
    done = FALSE;
    LexSplayNode* subtree = current_node;
    *inc = 0;
    
	if(*c != subtree->m_char)
	{
		// This subtree is not a match
		done = TRUE;
		return include_failed_node ? subtree : 0;
	}


    //The subtree is a match

    if(IsCompressed(subtree))
    {
		/*
		  This is hard :
		  for auto completion you want abc* so StartWith makes sense
		  for find longest matching prefix you want to see if key is
		  a prefix of c
		*/
		
		if( IsEqual(subtree, c)                                          ||
			match_type == PREFIX_MATCH         && StartsWith(subtree, c) ||
			match_type == LONGEST_PREFIX_MATCH && Matches(subtree, c))
		{
			*inc = uni_strlen(c);
			
			OP_ASSERT(*(c+*inc) == '\0');
			
			done = TRUE;
			return subtree;
		}
    }
    
    if(!(subtree->m_center) || *(c+1) == '\0')
    {
		done = TRUE;
		return subtree;
    }
    
    if(subtree->m_center)
    {
		subtree = subtree->m_center;
		*inc = 1;
		c++;
		
		while(1)
		{
			if(subtree->m_left && *c < subtree->m_char)
			{
				subtree = subtree->m_left;
			}
			else if(subtree->m_right && *c > subtree->m_char)
			{
				subtree = subtree->m_right;
			}
			else
				break;
		}
		
		if(!OnPath(subtree, c, match_type))
		{
			done = TRUE;
			
			if(!include_failed_node)
				subtree = 0;
		}
    }
    else
    {
		done = TRUE;
		
		if(!include_failed_node)
			subtree = 0;
    }
    
    return subtree;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
LexSplayTree::LexSplayNode* LexSplayTree::insertHelper(const uni_char* c)
{
    LexSplayNode* left = 0;
    LexSplayNode* right = 0;
    
    if(root) //Split tree for possible new root
    {
		access(c, root); //Will splay first char in c to root if it is in the tree
	
		OP_ASSERT( (!root->m_left)  || (root->m_left  && (root->m_left->m_char  < root->m_char) ) );
		OP_ASSERT( (!root->m_right) || (root->m_right && (root->m_right->m_char > root->m_char) ) );
	
		if(*c == root->m_char)//Char was in tree leave root as is
		{
			left  = root->m_left;
			right = root->m_right;
		}
		else if(*c < root->m_char)//Char not in tree and less - make room for new root
		{
			left  = root->m_left;
			right = root;
	    
			root->m_left = 0;
			root = 0;
		}
		else if(*c > root->m_char)//Char not in tree and more - make room for new root
		{
			left  = root;
			right = root->m_right;
	    
			root->m_right = 0;
			root = 0;
		}
		else
		{
			//Comparison not less, greater or equal
			OP_ASSERT(FALSE);
		}
    }
    
    BOOL inserted_at_root = FALSE;
    
    //If Char was not in tree - make new root
    if(!root)
    {
		root = makeCompressedNode(c);
	
		if(!root)
			return 0;
	
		inserted_at_root = TRUE;
    }
    
    //Join together new tree
    setLeft(left, root);
    setRight(right, root);
    
    if(inserted_at_root)
		return root;
    
    //Make new branch - reuse if possible existing branch
    LexSplayNode* subtree = root;
    
    while(*c != '\0')
    {
		if(*c == subtree->m_char)
		{
			if(IsCompressed(subtree))
			{
				if(!IsEqual(subtree, c))
				{
					subtree = BranchOff(subtree, c);
				}
		
				break;
			}
	    
			c++;
	    
			if(*c == '\0')
				break;
	    
			/*
			  Three cases to consider:
			  1) There is no center
			  Insert a tail compressed node.
			*/
	    
			if(!subtree->m_center)
			{
				BOOL successful = InsertCenterChild(makeCompressedNode(c), subtree);
		
				if(!successful)
				{
					OP_ASSERT(FALSE);
					return 0;
				}
		
				subtree = subtree->m_center;
				break;
			}
	    
			/*
			  2) The center is a tail compression
			  If it is the same - don't insert
			  If it's not uncompress as long as neccesary
			  and insert both as compressed nodes
	      
			  -- This will we caught when we loop
	      
			  3) The center is a normal node
			  continue as normal
			*/
	    
			else if( subtree->m_center)
			{
				subtree = subtree->m_center;
			}
			else
			{
				//Empty else in insertHelper
				OP_ASSERT(FALSE);
			}
		}
		else if(*c < subtree->m_char)
		{
			if(!subtree->m_left)
			{
				BOOL successful = InsertChild(makeCompressedNode(c), subtree);
		
				if(!successful)
				{
					OP_ASSERT(FALSE);
					return 0;
				}
		
				subtree = subtree->m_left;
				break;
			}
	    
			subtree = subtree->m_left;
		}
		else if(*c > subtree->m_char)
		{
			if(!subtree->m_right)
			{
				BOOL successful = InsertChild(makeCompressedNode(c), subtree);
		
				if(!successful)
				{
					OP_ASSERT(FALSE);
					return 0;
				}
		
				subtree = subtree->m_right;
				break;
			}
	    
			subtree = subtree->m_right;
		}
		else
		{
			//Empty else in insertHelper
			OP_ASSERT(FALSE);
		}
    }
    
    return subtree;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
LexSplayTree::LexSplayNode* LexSplayTree::BranchOff(LexSplayNode* node,
													const uni_char* c)
{
    if(!node) //This shouldn't happen
    {
		OP_ASSERT(FALSE);
		return 0;
    }
    
    const uni_char* key = GetKey(node);
    
    //Note it is important to keep the const contract even though we override it here!
    uni_char* c_ptr   = (uni_char*) c;
    uni_char* key_ptr = (uni_char*) key;
    
    if(!c_ptr || !key_ptr)
    {
		OP_ASSERT(FALSE);
		return 0;
    }
    
    BOOL node_replaced = FALSE;
    LexSplayNode* subtree = node;
    
    if(*c_ptr == *key_ptr)
    {
		/*
		  Replace node in the branch - this will make sure it is
		  fresh and unreferenced and hopefully make it easier to
		  see if this breaks.
		*/
	
		node_replaced = TRUE;
	
		// Make a new node to replace node :
		LexSplayNode* replace_node = makeNode(&node->m_char);
	
		if(!replace_node)
			return 0; // TODO : Maybe we should prune the branch ?
	
		LexSplayNode* left  = node->m_left;
		LexSplayNode* right = node->m_right;
	
		/*
		  Join together new tree
		  You don't have to worry about the center - node
		  was collapsed - it doesn't have a center.
		*/
		setLeft(left, replace_node);
		setRight(right, replace_node);
	
		// Disjoin node :
		node->m_left  = 0;
		node->m_right = 0;
	
		// This is our starting point
		subtree = replace_node;
	
		// If node was root then that also has to be fixed :
		if(node == root)
		{
			root = replace_node; //Replace root
		}
		else //If it's not root it has a parent
		{
			replaceSubtree(node->m_parent,
						   node,
						   subtree);
		}
	
		node->m_center = 0;
		node->m_parent = 0;
	
		// -----------------------------------------------------------------------
		// Expand their common path :
	
		// Since we have copied node - skip to next :
		c_ptr++;
		key_ptr++;
	
		for( ; *c_ptr && *key_ptr && *c_ptr == *key_ptr; c_ptr++, key_ptr++)
		{
			BOOL successful = InsertCenterChild(makeNode(c_ptr), subtree);
	    
			if(!successful)
			{
				OP_ASSERT(FALSE);
				return 0;
			}
	    
			subtree = subtree->m_center;
		}
    }
    
    // Note : Subtree is now the last node they have in common
    // -----------------------------------------------------------------------
    
    // -----------------------------------------------------------------------
    // Error handling :
    if(*c_ptr == '\0' && *key_ptr == '\0')
    {
		//This is an error !
	
		OP_ASSERT(FALSE);
	
		PruneBranch(subtree);
		return subtree == node ? NULL : node; //Recovery : you shouldn't have tried to make a branch
    }
    // -----------------------------------------------------------------------
    
    /*
      ------------------------------------------------------------------------
      Three remaining cases :
      
      1. *c_ptr == '\0' and *key_ptr != '\0'
      new item should be inserted here, but nodes items needs to be below
      
      2. *key_ptr == '\0' and *c_ptr != '\0'
      nodes items should be moved here, but new item must be inserted below
      
      3. *c_ptr != *key_ptr
      This is where their paths split. Make a compressed node for both and
      copy nodes items to it's compressed node.
      
      All of these have the following in common:
      
      c_ptr should have a compressed node (called insert_node) from the center
      of subtree and key_ptr should be a left/right child of insert_node.
      
      Make the two nodes (insert_node and new_node) and copy the items from
      node to new_node. Return insert_node.
      
      ------------------------------------------------------------------------
    */
    
    /*
      Set the new node here since it c_ptr might compress and move the subtree
      pointer - and we might need it if key_ptr is '\0'.
    */
    LexSplayNode* new_node = subtree;
    
    /*
      In both cases where *c_ptr != '\0' (that is case 2 and 3) the new item
      should be inserted as the center child of subtree.
    */
    if(*c_ptr != '\0')
    {
		BOOL successful = InsertCenterChild(makeCompressedNode(c_ptr), subtree);
	
		if(!successful)
		{
			OP_ASSERT(FALSE);
			return 0;
		}
	
		subtree = subtree->m_center;
    }
    
    /*
      If key_ptr != '\0' then there needs to be made a compressed node for
      the items still in node. If not then these items can be copied directly
      into new_node.
      
      If key_ptr == '\0' then new_node will be the last node they had in
      common.
      
      Now we have to see if we are in case 1 or 3:
      
      If we are in 1 then the new node should be a center child of subtree.
      If we are in 3 then the new node should be a right/left child of subtree.
    */
    if(*key_ptr != '\0')
    {
		new_node = makeCompressedNode(key_ptr);
	
		if(!new_node)
		{
			OP_ASSERT(FALSE);
			return 0;
		}
	
		BOOL successful = FALSE;
	
		if(*c_ptr != '\0')
			successful = InsertChild(new_node, subtree);
		else
			successful = InsertCenterChild(new_node, subtree);
	
		if(!successful)
		{
			OP_ASSERT(FALSE);
			return 0;
		}
    }
    
    /*
      The items from node now has to be moved from node to new_node.
    */
    CopyItems(node, new_node);
    
    // Insertion is done - now node might need to be deleted
    // -----------------------------------------------------------------------
    
    if(node_replaced)
    {
		OP_ASSERT(node != subtree);
		OP_ASSERT(node != new_node);
	
		OP_ASSERT(node->m_left   == 0);
		OP_ASSERT(node->m_right  == 0);
		OP_ASSERT(node->m_center == 0);
		OP_ASSERT(node->m_parent == 0);
	
		DeleteNode(node);
    }
    
    return subtree;
}

#endif // HISTORY_SUPPORT
