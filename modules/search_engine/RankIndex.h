/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef RANKINDEX_H
#define RANKINDEX_H

#include "modules/search_engine/Cursor.h"
#include "modules/search_engine/BlockStorage.h"
#include "modules/search_engine/ACT.h"
#include "modules/search_engine/SingleBTree.h"

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
#include "modules/search_engine/log/Log.h"
#endif


//#define RANKID_RANK 0
#define RANKID_ID   1

struct RankId
{
//	float  rank;
//	UINT32 id;
	union {
		float  rank;
		UINT32 data[2];  // pragma pack is not recognized on a lot of platforms
	};

	RankId(void) {rank = 0.0; data[RANKID_ID] = 0;}
	RankId(float r) {rank = r; data[RANKID_ID] = 0;}
	RankId(UINT32 i) {rank = 0.0; data[RANKID_ID] = i;}
	RankId(float r, UINT32 i) {rank = r; data[RANKID_ID] = i;}

/*	BOOL operator<(const RankId &right) const
	{
		return (double)this->id + this->rank < (double)right.id + right.rank;  // rank is in open interval (0,1)
	}*/

	static BOOL CompareRank(const void *left, const void *right)
	{
		float s, a;

		s = ((RankId *)left)->rank - ((RankId *)right)->rank;
		a = (((RankId *)left)->rank + ((RankId *)right)->rank) / 100000.0F;
		if (s < a && s > -a)
			return ((RankId *)left)->data[RANKID_ID] < ((RankId *)right)->data[RANKID_ID];

		return s < 0.0;
	}

	static BOOL CompareId(const void *left, const void *right)
	{
		return ((RankId *)left)->data[RANKID_ID] < ((RankId *)right)->data[RANKID_ID];  // id is unique
	}
};

#define IDTIME_VISITED 0
#define IDTIME_ID      1

struct IdTime
{
//	UINT32 visited;
//	UINT32 id;
	UINT32 data[2];  // pragma pack is not recognized on a lot of platforms

	IdTime(void) {data[IDTIME_VISITED] = 0; data[IDTIME_ID] = 0;}
	IdTime(time_t visited, UINT32 id) {data[IDTIME_VISITED] = (UINT32)visited; data[IDTIME_ID] = id;}

	BOOL operator<(const IdTime &right) const
	{
		if (this->data[IDTIME_VISITED] == right.data[IDTIME_VISITED])
			return this->data[IDTIME_ID] > right.data[IDTIME_ID];

		return this->data[IDTIME_VISITED] > right.data[IDTIME_VISITED];  // last time first
	}
};

/**
 * When initializing the vps folder is scanned for all subdirectories, sorted in order by date of last modification of directory using fstat.
 * Oldest directory by date is deleted first, directory 0-9 is always used but can be less or more. 
 */
struct RankIndex : public NonCopyable
{
	unsigned short m_id; //name of directory where files are stored.
	ACT m_act;
	BlockStorage m_wordbag; //unsorted vector of document IDs and rank.
	BlockStorage m_metadata; //contains full plain text, title, url, time, links to previous occurence of url if previously indexed, hash of text.
	SingleBTree<IdTime> m_alldoc; //list of all url documents indexed in this directory. Allows for searching by date.
								  //Also used for stop words. These are identified by whenever a word is in half the documents and number of documents is > 500.
	ACT m_url;
	unsigned m_doc_count;
	
	RankIndex() : m_id(0), m_doc_count(0)
	{
		m_act.GroupWith(m_wordbag);
		m_act.GroupWith(m_metadata);
		m_act.GroupWith(m_alldoc);
		m_act.GroupWith(m_url);
	}

	~RankIndex(void)
	{
		Close();
	}

	CHECK_RESULT(OP_STATUS Open(const uni_char *path, unsigned short id));
	void Close(void);

	/**
	 * @return actual size of the files on disk, any buffered data is not counted in
	 */
	OpFileLength Size();

	/**
	 * @return last modification time of the RabkIndex's directory or -1 on error
	 */
	time_t ModifTime(void);

	/**
	 * close and delete all the files; RankIndex remains opened on error
	 */
	CHECK_RESULT(OP_STATUS Clear(void));

	/**
	 * abort the pending transaction
	 */
	CHECK_RESULT(OP_STATUS Rollback(void));

	/**
	 * setup a cursor for the metadata file previously described at m_metadata.
	 */
	CHECK_RESULT(static OP_STATUS SetupCursor(BSCursor &cursor));

	/**
	 * callback for tail-compressed URLs
	 * Tail compressed urls are used for deleting searched urls.
	 */
	CHECK_RESULT(static OP_STATUS GetTail(char **stored_value, ACT::WordID id, void *usr_val));

	/**
	 * to find an id in Vector
	 */
	static BOOL CompareId(const void *left, const void *right)
	{
		return (*(RankIndex **)left)->m_id < (*(RankIndex **)right)->m_id;
	}

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	CHECK_RESULT(static OP_STATUS LogFile(OutputLogDevice *log, const uni_char *path, unsigned short id, const uni_char *fname, const uni_char *suffix = NULL));
	CHECK_RESULT(static OP_STATUS LogSubDir(OutputLogDevice *log, const uni_char *path, unsigned short id));
#endif
};

#define FNAME_ACT   "w.axx"
#define FNAME_WB    "wb.vx"
#define FNAME_META  "md.dat"
#define FNAME_BTREE "adoc.bx"
#define FNAME_URL   "url.axx"

#endif  // RANKINDEX_H

