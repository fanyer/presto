/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef HTML5_ENTITIES_H_
#define HTML5_ENTITIES_H_

class HTML5EntityStates;

#ifdef HTML5_STANDALONE
extern HTML5EntityStates* g_html5_entity_states;
#endif

/**
 * For parsing named tokens fast we use a finite state machine.  Each node(state)
 * for each character in the input, and if the node has a replacement character 
 * then it's valid to end the entity after this character.
 */
class HTML5EntityNode
{
public:

	HTML5EntityNode() : multiple_children_index(-1), replacement_char_index(-1), chr(0), nchildren(0) { }
	/**
	 * Constructor used to initialize a node with more than two children.
	 * @param c  The character which takes us to this node.
	 * @param nchild  The number of child nodes.
	 * @param multi_child_idx  The index into the children array where this nodes children are found.
	 * @param replace_chr  If this node is a valid end to the entity, this lists the character we should replace the entity with.
	 */
	HTML5EntityNode(char c, unsigned nchild, INT32 multi_child_idx, INT16 replace_chr) : 
		multiple_children_index(multi_child_idx), replacement_char_index(replace_chr), chr(c), nchildren(nchild) { }
	/**
	 * Constructor used to initialize a node with two or less children.
	 * @param c  The character which takes us to this node.
	 * @param nchild  The number of child nodes.
	 * @param child1_idx  The index (into the nodes array) of the first child.  0 if the node has no children.
	 * @param child1_idx  The index (into the nodes array) of the first child.  0 if the node has less than 2 children.
	 * @param replace_chr  If this node is a valid end to the entity, this lists the character we should replace the entity with.
	 */
	HTML5EntityNode(char c, unsigned nchild, INT16 child1_idx, INT16 child2_idx, INT16 replace_chr) : 
		replacement_char_index(replace_chr), chr(c), nchildren(nchild) { children[0] = child1_idx, children[1] = child2_idx; }
	~HTML5EntityNode() {}

	unsigned GetChildrenCount() const { return nchildren; }
	const UINT16* GetChildrenIndex() const { if (nchildren <= 2) return children; else return GetMultipleChildrenIndex(); }
	BOOL	IsTerminatingNode() const { return replacement_char_index != -1; }

	/** The unicode characters this entity can be replaced with. May be up to
		two unicode characters, each needing two uni_char surrogate
		characters. Should only be called if IsTerminatingNode() returns TRUE */
	void	GetReplacementCharacters(const uni_char *&replacements) const;

	/** The character which brings us to this state. */
	char	GetCharacter() const { return chr; }
private:
	const UINT16* GetMultipleChildrenIndex() const;
	/**
	 * Following states.  Index into the children table;  from that index and nchildren
	 * elements on is this node's children.  If this node has just one or two children then
	 * they are stored directly in the node.
	 * The UINT16 is an index into the entity_nodes table where the actual node exists.
	 */
	union
	{
		UINT16 children[2];
		INT32 multiple_children_index;
	};

	/**
	 *  The unicode characters this entity can be replaced with.  Index into the 
	 *  table where the replacement characters are stored.
	 */
	INT16 replacement_char_index;

	/** The character which brings us to this state. */
	char chr;

	/** Number of valid states from this node (i.e. which characters which may
		follow it and lead to an entity in the end). */
	unsigned nchildren : 8;
};

/**
 * Class for representing the Entity states.  Singleton.
 */
class HTML5EntityStates
{
public:
	~HTML5EntityStates() { OP_DELETEA(m_state_nodes); }

	static void InitL();
	static void Destroy();

	/// Get's the root start, where we start the parsing from (actually returns the same as GetAllNodes, since the root always is first node)
	const HTML5EntityNode*	GetRootState() { return &m_state_nodes[0]; }
	/// Get's all nodes(states)
	const HTML5EntityNode*	GetAllNodes() { return m_state_nodes; }
	unsigned				GetNumberOfStateNodes() { return m_num_state_nodes; }
private:
	HTML5EntityStates() : m_state_nodes(NULL), m_num_state_nodes(0) {}

	static void				ReadEntityNodesL(HTML5EntityNode* nodes);

	HTML5EntityNode*		m_state_nodes;   ///< this array stores all states
	unsigned				m_num_state_nodes;
};

/**
 * Class for matching input and deciding which entity it is.
 * First call Reset(), then call MatchNextChar() with subsequent input characters
 * until it returns FALSE.
 * The uni_chars GetUnicodeSurrogates() returns is the actual match.   Only valid
 * when IsTerminatingState is TRUE, and it's the longest match which is valid,
 * if IsTerminatingState is TRUE several times use the last.
 */
class HTML5EntityMatcher
{
public:
	HTML5EntityMatcher() { Reset(); }
	
	void		Reset() { m_current_state = GetStartNode(); }
	
	/// Call repeatedly until function returns FALSE
	BOOL		MatchNextChar(uni_char next_char);
	/// Returns TRUE if this is a valid entity after last character input to MatchNextChar()
	BOOL		IsTerminatingState() { return m_current_state->IsTerminatingNode(); }
	/// Get the characters to replace the entity with (only valid if IsTerminatingState() returned TRUE for last call to MatchNextChar())
	unsigned	GetReplacementCharacters(const uni_char *&replacements)
	{
		m_current_state->GetReplacementCharacters(replacements);
		return kReplacementCount;
	}

	/// helper function which returns uni_char surrogates for a unicode codepoint
	static void	GetUnicodeSurrogates(unsigned long codepoint, uni_char& high, uni_char& low);
private:
	const HTML5EntityNode*	GetStartNode() { return g_html5_entity_states->GetRootState(); }
	const HTML5EntityNode*	GetNode(unsigned index) { OP_ASSERT(index < g_html5_entity_states->GetNumberOfStateNodes()); return &g_html5_entity_states->GetAllNodes()[index]; }

	/// The number of uni_chars returned by a entity reference match.
	static const unsigned	kReplacementCount = 4;

	const HTML5EntityNode*	m_current_state;
};

#endif /* HTML5_ENTITIES_H_ */
