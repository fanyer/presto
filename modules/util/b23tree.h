/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_B23TREE_H
#define MODULES_UTIL_B23TREE_H

class B23Tree_Node;
class B23Tree_Item;
class B23Tree_Store;

typedef int (*B23_CompareFunction)(const B23Tree_Item *first, const B23Tree_Item *second)  ;


class B23Tree_Item
{
	friend class B23Tree_Node;
	friend class B23Tree_Store;
private:
	B23Tree_Node *parent;

public:
	B23Tree_Item() : parent(NULL) {}
	virtual ~B23Tree_Item() { parent = NULL; }

	B23Tree_Node *GetParent() const{return parent;}
	void SetParent(B23Tree_Node *prnt){parent = prnt;}

	/** return:
	 *		negative if search item is less than this object
	 *		zero(0) if search item is equal to this object
	 *		positive if search item is greater than this object
	 */
	virtual int SearchCompare(void* /*search_param*/) { return 0; /* always equal */ }
	/** Return TRUE if travers should continue with next element */
	virtual BOOL TraverseActionL(uint32 /*action*/, void* /*params*/) { return TRUE; /* no action */ }

    B23Tree_Item*	PrevLeaf();
    B23Tree_Item*	NextLeaf();
    B23Tree_Item*	Prev(){return PrevLeaf();}
    B23Tree_Item*	Suc(){return NextLeaf();}

    void				Out();

    void				Into(B23Tree_Store* list);
    void				IntoStart(B23Tree_Store* list){Into(list);}

    void				Follow(B23Tree_Item* link);
    void				Precede(B23Tree_Item* link){Follow(link);};

    BOOL				InList() { return parent != NULL; };

private:
	B23Tree_Store	*GetMasterStore() const;
};

class B23Tree_Node
{
	friend class B23Tree_Store;
private:
	B23Tree_Node *parent;
	B23Tree_Store *parent_store;
	B23Tree_Item *items[2];
	B23Tree_Node *next[3];

	B23_CompareFunction compare;

protected:
	B23Tree_Node *ConstructNodeL();
	OP_STATUS ConstructNode(B23Tree_Node*& new_node);

public:

	B23Tree_Node(B23_CompareFunction comp);
	~B23Tree_Node();

	BOOL TraverseL(uint32 action, void *params=NULL);

	B23Tree_Node *GetParent() const{return parent;}
	void SetParent(B23Tree_Node *prnt) {parent = prnt;}
	void UpdateParents();

	B23Tree_Store *GetParentStore() const{return parent_store;}
	void SetParentStore(B23Tree_Store *prnt){parent_store = prnt;}

	B23Tree_Item *AccessItem(int i){return (i>= 0 && i<=1 ? items[i] : NULL);}
	
	B23Tree_Node *AccessSubNode(int i){return (i>= 0 && i<=2 ? next[i] : NULL);}

	B23Tree_Node *InsertL(B23Tree_Item *item);
	B23Tree_Node *Delete(B23Tree_Item *item, BOOL delete_item=TRUE);
	B23Tree_Item *Search(void *search_param);

    B23Tree_Item*	LastLeaf();
    B23Tree_Item*	FirstLeaf();
    B23Tree_Item*	Last(){return LastLeaf();}
    B23Tree_Item*	First(){return FirstLeaf();}

private:
	B23Tree_Node *adjustTree(B23Tree_Item *&target, BOOL delete_item);
	B23Tree_Node *adjustNode(B23Tree_Node *temp_node, int index);
};

class B23Tree_Store
{
private:
	B23Tree_Node *top;

protected:
	B23_CompareFunction compare;
	B23Tree_Node *ConstructNodeL();

public:
	B23Tree_Store(B23_CompareFunction comp);
	~B23Tree_Store();

	void InsertL(B23Tree_Item *item);
	void Delete(B23Tree_Item *item, BOOL delete_item=TRUE);

	void TraverseL(uint32 action, void *params=NULL);
	B23Tree_Item *Search(void *search_param);

	B23Tree_Item*	FirstLeaf();
    B23Tree_Item*	LastLeaf();
    B23Tree_Item*	Last(){return LastLeaf();}
    B23Tree_Item*	First(){return FirstLeaf();}

	BOOL Empty();
    void			Clear();
};

#endif // !MODULES_UTIL_B23TREE_H
