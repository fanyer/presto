/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_RANGE_H
#define DOM_RANGE_H

#ifdef DOM2_RANGE

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domobj.h"

class DOM_Node;
class DOM_RangeState;
#ifdef DOM_SELECTION_SUPPORT
class DOM_WindowSelection;
#endif // DOM_SELECTION_SUPPORT

/*
  The DOM_Range class is used to represent a Range object as specified in the
  DOMCore spec http://www.w3.org/TR/dom/#interface-range ("the spec") (latest
  editors draft can be found at
  http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#interface-range )

  A boundary point is a tuple of (node, offset), where node is any given
  node and offset indicates either the child node at that index if node has
  children or the code point in the data of the node for character data nodes.

  A range is the set of nodes and data found between the start boundary point
  and the end boundary point of the range.

  Any node, except doctypes, can be a part of a range. That includes parts
  of character data nodes, nodes not visible to the user and nodes not
  attached to the main document tree.

  Any operation, on the range or not, that removes, inserts or alters the data
  of nodes close to the boundary points of the range, can result in a change of
  the boundary points of the range.
 */

class DOM_Range
	: public DOM_Object
{
public:
	class BoundaryPoint
	{
	public:
		BoundaryPoint() : node(NULL), offset(0), unit(NULL) {}
		BoundaryPoint(DOM_Node *node, unsigned offset, DOM_Node *unit) : node(node), offset(offset), unit(unit) {}

		DOM_Node *node;
		/**< The node of the boundary point. */
		unsigned offset;
		/**< The offset into node of the boundary point */
		DOM_Node *unit;
		/**< The child of node with the index matching offset. Can be NULL. */

		BOOL operator== (const BoundaryPoint &other) const { return node == other.node && offset == other.offset; }
		BOOL operator!= (const BoundaryPoint &other) const { return node != other.node || offset != other.offset; }

		OP_STATUS CalculateUnit();
	};
	/**< Used for storing boundary points as described in the spec */

private:
	class DOM_RangeMutationListener
	: public DOM_MutationListener
	{
	protected:
		DOM_Range *range;

	public:
		DOM_RangeMutationListener(DOM_Range *range)
			: range(range)
		{
		}

		virtual void OnTreeDestroyed(HTML_Element *tree_root);
		virtual OP_STATUS OnAfterInsert(HTML_Element *child, DOM_Runtime *origining_runtime);
		virtual OP_STATUS OnBeforeRemove(HTML_Element *child, DOM_Runtime *origining_runtime);
		virtual OP_STATUS OnAfterValueModified(HTML_Element *element, const uni_char *new_value, ValueModificationType type, unsigned offset, unsigned length1, unsigned length2, DOM_Runtime *origining_runtime);
	};

	void SetStartInternal(BoundaryPoint &new_bp, HTML_Element *new_root);
	void SetEndInternal(BoundaryPoint &new_bp, HTML_Element *new_root);

	void UnregisterMutationListener();
	void ValidateMutationListener();
	/**< Validates if the mutation listener is registered in a proper environment i.e.
	     the environment the endpoints nodes live in.
	     If the listener is registered in some other environment it's unregistered
	     from it and is registered in the node's one. */

	OP_STATUS AdjustBoundaryPoint(DOM_Node *parent, DOM_Node *child, BoundaryPoint &point, unsigned &childs_index, unsigned &moved_count);

#ifdef DOM_SELECTION_SUPPORT
	friend class DOM_WindowSelection;
	DOM_WindowSelection* selection;
#endif // DOM_SELECTION_SUPPORT

protected:
	DOM_Range(DOM_Document *document, DOM_Node* initial_unit);
	virtual ~DOM_Range();

	BoundaryPoint start;
	/**< The start point of the range as specified in the spec */
	BoundaryPoint end;
	/**< The end point of the range as specified in the spec */
	DOM_Node *common_ancestor;
	/**< The common ancestor node of the range as per the spec. */
	BOOL detached;
	/**< TRUE if the range is currently detached as per the spec. */
	BOOL busy;
	/**< TRUE if the range is currently undergoing a multi stage operation
	     like surroundContents() or cloning, which should block other changes. */
	BOOL abort;
	/**< TRUE if the range is currently detached as per the spec */
	HTML_Element *tree_root;
	/**< The root of the tree or fragment the endpoints of the range
	     belongs to. Both endpoints must always have the same root. */
	DOM_RangeMutationListener listener;
	/**< Mutation listener which will adjust the range when the nodes
	     within it are changed in ways specified in the spec. */
	DOM_EnvironmentImpl *listeners_environment;
	/**< The environment the mutation listener was registered in. */

	friend class DOM_RangeMutationListener;

public:
	enum
	{
		START_TO_START = 0,
		START_TO_END = 1,
		END_TO_END = 2,
		END_TO_START = 3
	};

	static OP_STATUS Make(DOM_Range *&range, DOM_Document *document);

	static void ConstructRangeObjectL(ES_Object *object, DOM_Runtime *runtime);
	static OP_STATUS ConstructRangeObject(ES_Object *object, DOM_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_RANGE || DOM_Object::IsA(type); }
	virtual void GCTrace();

	BOOL StartCall(int &return_code, ES_Value *return_value, DOM_Runtime *origining_runtime);
	/**< Returns TRUE if the call is valid, otherwise return_code should be returned. */

	BOOL Reset();
	/**< Returns TRUE if abort was FALSE. */

	void Detach();
	void SetBusy(BOOL new_busy) { busy = new_busy; }

	BOOL IsDetached() { return detached; }

	BOOL IsCollapsed() { return start == end; }

	DOM_Document *GetOwnerDocument();
	HTML_Element *GetTreeRoot() { return tree_root; }
	static HTML_Element* FindTreeRoot(DOM_Node *node);
	/**< Traverse up the parent chain to find the root of nodes subtree.

	     @return The root of the subtree node belongs to. Can be NULL for
	             document nodes. */
	static HTML_Element *GetAssociatedElement(DOM_Node *node);
	/**< Gets the HTML_Element associated with this node.

	     @return The HTML_Element representing the node. Can be NULL for
	             document nodes. */

	static OP_STATUS CountChildUnits(unsigned &child_units, DOM_Node *node);
	/**< Enumerates the number of uni_chars for CharacterData nodes, or the
	     number of children for other node types.

	     @return The length property from the spec. */
	static OP_STATUS GetOffsetNode(DOM_Node *&node, BoundaryPoint &bp);
	/**< Gets the node with index equal to the offset specified by bp.

	     @param[out] node The node that has the index specified by bps offset.
	     @param[in] bp The boundary point which offset to be used.
	     @return Normal OOM values. */
	static unsigned CalculateOffset(DOM_Node *node);
	/**< Calculates the index, per the spec, of node.

	     @return The index of node. */
	static unsigned CalculateOffset(HTML_Element *element);
	/**< Calculates the index, per the spec, of element.

	     @return The index of element. */
	OP_STATUS UpdateCommonAncestor();
	/**< Called to update the common ancestor node after setting new
	     boundary points.

	     @return Normal OOM values. */
	static int CompareBoundaryPoints(BoundaryPoint &first, BoundaryPoint &second);
	/**< Compares the first and second boundary points as per the spec.

	     @param first[in] First boundary point.
	     @param second[in] Second boundary point.
	     @return -1 if first is before second, 0 if they are equal, 1 if after. */

	OP_STATUS SetStart(BoundaryPoint &newBP, DOM_Object::DOMException &exception);
	/**< Sets the start boundary point of the range. Adjusting the end point if
	     it is before the start, or in another subtree.

	     @param[in] newBP The new start point.
	     @param[out] exception If not DOM_Object::DOMEXCEPTION_NO_ERR after the
		                       call, an exception matching the value should be
		                       thrown from the caller.
	     @return Normal OOM values. */
	OP_STATUS SetEnd(BoundaryPoint &newBP, DOM_Object::DOMException &exception);
	/**< Sets the end boundary point of the range. Adjusting the start point if
	     it is after the end, or in another subtree.

	     @param[in] newBP The new end point.
	     @param[out] exception If not DOM_Object::DOMEXCEPTION_NO_ERR after the
		                       call, an exception matching the value should be
		                       thrown from the caller.
	     @return Normal OOM values. */
	OP_STATUS SetStartAndEnd(BoundaryPoint &newStartBP, BoundaryPoint &newEndBP, DOM_Object::DOMException &exception);
	/**< Sets both the start and end boundary points of the range. Adjusting
	     each end point if it is on the wrong side of the other, or in another
	     subtree.

	     @param[in] newStartBP The new start point.
	     @param[in] newEndBP The new end point.
	     @param[out] exception If not DOM_Object::DOMEXCEPTION_NO_ERR after the
		                       call, an exception matching the value should be
		                       thrown from the caller.
	     @return Normal OOM values. */

	void GetEndPoints(BoundaryPoint &start_out, BoundaryPoint &end_out) { start_out = start; end_out = end; }

	OP_STATUS HandleNodeInserted(DOM_Node *parent, DOM_Node *child);
	/**< Called when any nodes are inserted. Will adjust the boundary points
	     of the range if they are effected by the change.

	     @param[in] parent The parent of the node inserted.
	     @param[in] child The inserted node.
	     @return Normal OOM values. */
	OP_STATUS HandleRemovingNode(DOM_Node *parent, DOM_Node *child);
	/**< Called before any nodes are removed. Will adjust the boundary points
	     of the range if they are effected by the change.

	     @param parent The parent of the node removed.
	     @param child The removed node.
	     @return Normal OOM values. */
	OP_STATUS HandleNodeValueModified(DOM_Node *node, const uni_char *value, DOM_MutationListener::ValueModificationType type, unsigned offset, unsigned length1, unsigned length2);
	/**< Called when any CharacterData node changes its value. Will adjust the
	     boundary points of the range if they are effected by the change.

	     @param[in] node The node which value is changed.
	     @param[in] value The new value.
	     @param[in] type The type of value modification.
	     @param[in] offset The start offset into the old value of the value change.
	     @param[in] length1 The length, in uni_chars, of the removed content.
	     @param[in] length2 The length, in uni_chars, of the inserted content.
	     @return Normal OOM values. */

	/*
	 * The following 7 methods are used to perform the complex operations of
	 * deleting, extracting or cloning the contents within the range or
	 * inserting nodes into the range at a certain point. They all schedule
	 * various sub-operations in the state object for each of the nodes in
	 * the part of the subtree that is fully or partially contained within
	 * the boundary points of the range.
	 *
	 * The Extract* methods are used to delete, extract or clone the contents
	 * within the range.
	 *
	 * The Insert* methods are used for inserting a node at a certain point
	 * in the range.
	 */

	void ExtractStartParentChainL(DOM_RangeState *state, DOM_Node *container);
	/**< Used to recursively schedule sub-operations for the chain of parent
	     nodes from the common ancestor of the range down to the start node.

	     Can LEAVE with normal OOM values, or OpStatus::ERR_NOT_SUPPORTED
	     if a doctype is found among the contained nodes.

	     @param state The state object of the operation.
	     @param container The current node in the traversed chain. */
	void ExtractStartBranchL(DOM_RangeState *state, DOM_Node *container, DOM_Node *unit);
	/**< Used to recursively schedule sub-operations for the set of nodes fully
	     or partially included in the subtree under common ancestor that start
	     point belongs to, including the start point itself and its children.

	     Can LEAVE with normal OOM values, or OpStatus::ERR_NOT_SUPPORTED
	     if a doctype is found among the contained nodes.

	     @param state The state object of the operation.
	     @param container The current node in the traversed chain.
	     @param unit The first child of container that is fully or partially
	                 contained in the range. Can be NULL. */
	void ExtractEndBranchL(DOM_RangeState *state, DOM_Node *container, DOM_Node *unit);
	/**< Used to recursively schedule sub-operations for the set of nodes fully
	     or partially included in the subtree under common ancestor that end
	     point belongs to, including the end point itself and its children.

	     Can LEAVE with normal OOM values, or OpStatus::ERR_NOT_SUPPORTED
	     if a doctype is found among the contained nodes.

	     @param state The state object of the operation.
	     @param container The current node in the traversed chain.
	     @param unit The first child of container that is fully or partially
	                 contained in the range. Can be NULL. */
	void ExtractContentsL(DOM_RangeState *state);
	/**< Used to schedule sub-operations for the set of nodes fully or
	     partially included in the range between the start and end point.

	     Can LEAVE with normal OOM values, or OpStatus::ERR_NOT_SUPPORTED
	     if a doctype is found among the contained nodes.

	     @param state The state object of the operation. */
	OP_STATUS ExtractContents(DOM_RangeState *state);
	/**< Used to TRAP the call to ExtractContentsL()

	     @return Normal OOM values or OpStatus::ERR_NOT_SUPPORTED if a doctype
	             is found among the nodes contained in the range. */

	void InsertNodeL(DOM_RangeState *state, DOM_Node *newNode);
	/**< Used to schedule sub-operations for inserting a node at the start
	     point of the range.

		 Can LEAVE with normal OOM values.

		 @param state The state object of the operation.
		 @param newNode The node to insert. */
	OP_STATUS InsertNode(DOM_RangeState *state, DOM_Node *newNode);
	/**< Used to TRAP the call to InsertNodeL()

	     @return Normal OOM values. */

#ifdef _DEBUG
	void DebugCheckState(BOOL inside_tree_operation = FALSE);
#endif // _DEBUG

	DOM_DECLARE_FUNCTION(collapse);
	DOM_DECLARE_FUNCTION(selectNode);
	DOM_DECLARE_FUNCTION(selectNodeContents);
	DOM_DECLARE_FUNCTION(compareBoundaryPoints);
	DOM_DECLARE_FUNCTION(insertNode);
	DOM_DECLARE_FUNCTION(surroundContents);
	DOM_DECLARE_FUNCTION(cloneRange);
	DOM_DECLARE_FUNCTION(toString);
	DOM_DECLARE_FUNCTION(detach);
	DOM_DECLARE_FUNCTION(createContextualFragment);
	DOM_DECLARE_FUNCTION(intersectsNode);
	DOM_DECLARE_FUNCTION(getClientRects);
	DOM_DECLARE_FUNCTION(getBoundingClientRect);
	enum { FUNCTIONS_ARRAY_SIZE = 14 };

	DOM_DECLARE_FUNCTION_WITH_DATA(setStartOrEnd);
	DOM_DECLARE_FUNCTION_WITH_DATA(setStartOrEndBeforeOrAfter);
	DOM_DECLARE_FUNCTION_WITH_DATA(deleteOrExtractOrCloneContents);
	DOM_DECLARE_FUNCTION_WITH_DATA(comparePoint);
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 12 };
};

#endif // DOM2_RANGE
#endif // DOM_RANGE_H
