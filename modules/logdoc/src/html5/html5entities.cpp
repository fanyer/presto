// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#include "modules/logdoc/src/html5/html5entities.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opfolder.h"

#ifdef HTML5_STANDALONE
HTML5EntityStates* g_html5_entity_states = NULL;
#endif

#define ENTITY_INIT_FILE_STR "html5_entity_init.dat"
#define ENTITY_INIT_FILE UNI_L(ENTITY_INIT_FILE_STR)
#define ZIPPED_ENTITY_INIT_FILE_STR (ENTITY_INIT_FILE_STR PATHSEP ENTITY_INIT_FILE_STR)

#ifdef _WINDOWS // without turning off optimization for this it will compile _really_ slow
# pragma optimize("", off)
#endif // _WINDOWS

#ifdef HAS_COMPLEX_GLOBALS
# define REPLACEMENT_CHAR_TABLE_START() static const HTML5ReplacementCharacter entity_replacement_characters[] = {
# define REPLACEMENT_CHAR_TABLE_ENTRY(codepoint1, codepoint2)  HTML5ReplacementCharacter(codepoint1, codepoint2),
# define REPLACEMENT_CHAR_TABLE_END() };
#else // HAS_COMPLEX_GLOBALS
# define REPLACEMENT_CHAR_TABLE_START() static void InitReplacementCharactersTable() {\
										int i = 0; HTML5ReplacementCharacter* tmp = g_opera->logdoc_module.m_entity_replacement_characters;
# define REPLACEMENT_CHAR_TABLE_ENTRY(codepoint1, codepoint2)  tmp[i++].Set(codepoint1, codepoint2);
# define REPLACEMENT_CHAR_TABLE_END() };

#define entity_replacement_characters g_opera->logdoc_module.m_entity_replacement_characters
#endif // HAS_COMPLEX_GLOBALS

struct HTML5ReplacementCharacter
{
	HTML5ReplacementCharacter() {}
	HTML5ReplacementCharacter(unsigned long codepoint1, unsigned long codepoint2) { Set(codepoint1, codepoint2); }

	void Set(unsigned long codepoint1, unsigned long codepoint2)
	{
		HTML5EntityMatcher::GetUnicodeSurrogates(codepoint1, chars[0], chars[1]);
		HTML5EntityMatcher::GetUnicodeSurrogates(codepoint2, chars[2], chars[3]);
	}

	uni_char chars[4];  /* ARRAY OK 2011-11-09 arneh */
};

#include "modules/logdoc/src/html5/html5entity_nodes_init.inl"

void HTML5EntityNode::GetReplacementCharacters(const uni_char *&replacements) const
{
	if (IsTerminatingNode())
		replacements = entity_replacement_characters[replacement_char_index].chars; 
	else
		OP_ASSERT(FALSE);  // Shouldn't call this function unless IsTerminatingNode returns TRUE
}

const UINT16* HTML5EntityNode::GetMultipleChildrenIndex() const
{
	OP_ASSERT(nchildren > 2);
	return &entity_node_children[multiple_children_index];
}

void HTML5EntityStates::InitL()
{
	OP_ASSERT(g_html5_entity_states == NULL);

	OpStackAutoPtr<HTML5EntityStates> html5_entity_states(OP_NEW_L(HTML5EntityStates, ()));

#ifndef HAS_COMPLEX_GLOBALS
	entity_replacement_characters = OP_NEWA_L(HTML5ReplacementCharacter, NUM_REPLACEMENT_CHARS);
	InitReplacementCharactersTable();
#endif // HAS_COMPLEX_GLOBALS

	html5_entity_states->m_num_state_nodes = NUM_ENTITY_NODES;
	html5_entity_states->m_state_nodes = OP_NEWA_L(HTML5EntityNode, NUM_ENTITY_NODES);

	ReadEntityNodesL(html5_entity_states->m_state_nodes);

	g_html5_entity_states = html5_entity_states.release();
}

#ifdef _WINDOWS
# pragma optimize("", on)
#endif // _WINDOWS

/* static */ void HTML5EntityStates::ReadEntityNodesL(HTML5EntityNode* nodes)
{
	OpFile *entity_file = NULL;
	OpFile plain_file; ANCHOR(OpFile, plain_file);

	OpString file_path; ANCHOR(OpString, file_path);
	OpFile zipped_file; ANCHOR(OpFile, zipped_file);

	file_path.SetL(ZIPPED_ENTITY_INIT_FILE_STR);
	LEAVE_IF_ERROR(zipped_file.Construct(file_path.CStr(), OPFILE_RESOURCES_FOLDER, OPFILE_FLAGS_ASSUME_ZIP));
	if (OpStatus::IsSuccess(zipped_file.Open(OPFILE_READ)))
		entity_file = &zipped_file;

	if (!entity_file)
	{
		LEAVE_IF_ERROR(plain_file.Construct(ENTITY_INIT_FILE, OPFILE_RESOURCES_FOLDER));
		OP_STATUS status = plain_file.Open(OPFILE_READ|OPFILE_TEXT);

		if (OpStatus::IsError(status))
		{
			OP_ASSERT(!"Make sure html5_entity_init.dat exists in OPFILE_RESOURCES_FOLDER");
			LEAVE(status);
		}

		entity_file = &plain_file;
	}

	OpString8 line; ANCHOR(OpString8, line);
	LEAVE_IF_ERROR(entity_file->ReadLine(line));
	if (line.Compare("#", 1) != 0)
	{
		OP_ASSERT(FALSE);  // Wrong file format
		LEAVE(OpStatus::ERR);
	}

	int nentries;
	LEAVE_IF_ERROR(entity_file->ReadLine(line));
	if (op_sscanf(line.CStr(), "%d", &nentries) != 1 || nentries != NUM_ENTITY_NODES)
	{
		OP_ASSERT(FALSE);  // Wrong file format
		LEAVE(OpStatus::ERR);
	}

	for (int n = 0; n < nentries; n++)
	{
		OP_ASSERT(!entity_file->Eof());

		LEAVE_IF_ERROR(entity_file->ReadLine(line));

		char c;
		int replacement_idx, nchildren, child1, child2, nvalues;
		nvalues = op_sscanf(line.CStr(), "%c %d %d %d %d", &c, &replacement_idx, &nchildren, &child1, &child2);

		if (nvalues < 4 || (nchildren < 2 && nvalues < 5) ||
			replacement_idx < -1 || replacement_idx >= NUM_REPLACEMENT_CHARS ||
			(nchildren > 0 && (child1 < 0 || child1 >= NUM_ENTITY_NODES)) ||
			(nchildren == 2 && (child2 < 0 || child2 >= NUM_ENTITY_NODES)))
		{
			OP_ASSERT(FALSE);  // Wrong file format
			LEAVE(OpStatus::ERR);
		}

		if (nchildren > 2)
			nodes[n] = HTML5EntityNode(c, nchildren, child1, replacement_idx);
		else
			nodes[n] = HTML5EntityNode(c, nchildren, child1, child2, replacement_idx);
	}

	LEAVE_IF_ERROR(entity_file->Close());
}

void HTML5EntityStates::Destroy()
{
	OP_DELETE(g_html5_entity_states);
#ifndef HAS_COMPLEX_GLOBALS
	OP_DELETEA(entity_replacement_characters);
#endif // HAS_COMPLEX_GLOBALS
}

void HTML5EntityMatcher::GetUnicodeSurrogates(unsigned long codepoint, uni_char& high, uni_char& low)
{
	if (codepoint < 0x10000)  // can use one regular uni_char
	{
		OP_ASSERT(!Unicode::IsSurrogate(codepoint));  // should have been replaced in CheckAndReplaceInvalidUnicode if it was

		low = static_cast<uni_char>(codepoint);
		high = 0;
	}
	else  // need to make surrogate pair
		Unicode::MakeSurrogate(codepoint, high, low);
}

BOOL HTML5EntityMatcher::MatchNextChar(uni_char next_char)
{
	// A quick cut off for characters which will never be in an entity name
	if ((next_char < '0' || next_char > 'z') && next_char != ';')
		return FALSE;

	const UINT16* children = m_current_state->GetChildrenIndex();
	for (unsigned i = 0; i < m_current_state->GetChildrenCount(); i++)
	{
		const HTML5EntityNode* node = GetNode(children[i]);
		if (node->GetCharacter() == next_char)
		{
			m_current_state = node;

			return TRUE;
		}
	}

	return FALSE;
}
