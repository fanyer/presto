#!/usr/bin/pike
/* -*- Mode: pike; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
// CSS2 5.12.2: Anything in the Ps, Pe and Po are punctuation
#define SUPPORT_CHARACTER_CLASSES
// #define UNICODE_BUILD_TABLES
#include "../unicode.h"

// Map from UnicodeData class name to our enum name and check that
// it is defined in unicode.h
string get_cls( string cls) 
{
	if (!this["CC_"+cls])
		werror("*** ERROR ***: Unknown class: "+cls+"\n");
	return "CC_"+cls;
}

// Map unicode point to plane
int get_plane(int codepoint)
{
	return (codepoint & 0xff0000) >> 16;
}

void main()
{
	array table = ({});
	string lbt = "\0"*131072;
	array(string) cls_flat = ({ "CC_Unknown" })*256;

	// Read UnicodeData.txt
//	foreach(Stdio.read_file("../data/UnicodeData.txt") / "\n"; int lno; string line) 
	foreach(Stdio.stdin.line_iterator(); int lno; string line) 
	{
		// Strip comments and split into semi-colon separated fields
		array(string) info = String.trim_all_whites((line/"#")[0])/";";
		if (sizeof( info) != 15)
			continue;

		// Fetch character class
		string cls = get_cls(info[2]);

		// Fetch codepoint
		int c;
		sscanf(info[0], "%x", c);

		// Check if that is an end of some codepoint's range
		int is_range_end = search(info[1], "Last>") >= 0;
		
		// Skip f and 10, not much interesting there anyway.
		if (get_plane(c) > 0xE)
			continue;

		// Move the whitespace out of the Control-character group.
		switch (c)
		{
			case 9..13: cls = "CC_Zs"; break;
		}

		if (c < 131072) // p0/p1 selftest data
			lbt[c]=this[cls];
		if (c < 256)
		{
			// Store in the flat table
			cls_flat[c]=cls;
		}
		else
		{
			// some ranges in UnicodeData.txt are defined by providing the first and the last codepoints e.g.:
			//	3400;<CJK Ideograph Extension A, First>;Lo;0;L;;;;;N;;;;;
			//	4DB5;<CJK Ideograph Extension A, Last>;Lo;0;L;;;;;N;;;;;
			// and we should not treat the block between those codepoints as CC_Unknown
			if (sizeof(table) && is_range_end)
			{
				table[-1][2] = c-table[-1][0]+1;
				continue;
			}
			// do not filter out CC_Unknown class - that information is also needed e.g. in IDNALabelValidator::IsUnassigned()
			if (sizeof(table) && c != table[-1][0]+table[-1][2])
			{
				int next = table[-1][0]+table[-1][2];
				table += ({ ({ next, 0, c-next }) }); // undefined
			}
			if (sizeof(table) && cls == table[-1][1] &&  c == table[-1][0]+table[-1][2])
			{
				// Same as previous codepoint, run-length encode
				table[-1][2]++;
			}
			else 
			{
				// Create a new range
				table += ({ ({ c, cls, 1 }) });
			}
		}
	}

	// Add some extras to the table
	table += ({
		({ 0xE0000, 0, 1 }),                         // Undefined
		({ 0xE01F0, 0, 0xEFFFF-0xE01F0 })
	});

	for (int i = 0x3400; i<0x4DB6; i++) lbt[i]=this[get_cls("Lo")]; // CJK Ideograph Extension A
	for (int i = 0x4E00; i<0x9FCC; i++) lbt[i]=this[get_cls("Lo")]; // CJK Ideograph
	for (int i = 0xAC00; i<0xD7A4; i++) lbt[i]=this[get_cls("Lo")]; // Hangul Syllable
	for (int i = 0xD800; i<0xe000; i++) lbt[i]=this[get_cls("Cs")]; // Private use surrogate
	for (int i = 0xE000; i<0xf900; i++) lbt[i]=this[get_cls("Co")]; // Private use

	// Add plane sentinels
	table += ({
		({ 0xffff, 0, 1 }), 
		({ 0x1ffff, 0, 1 }), 
		({ 0x2ffff, 0, 1 }), 
		({ 0xeffff, 0, 1 })
	});

	table = sort(table);
	
	// Write selftest data
	Stdio.write_file( "cls.dat", lbt);
	
	// Range-optimize the table "table" by joining adjacent intervals,
	// exploiting the fact that there are holes in the Unicode character
	// database.
	array nt = ({table[0]});
	for (int i = 1; i < sizeof(table); i ++)
	{
		if (table[i][1] == nt[-1][1] && get_plane(table[i][0]) == get_plane(nt[-1][0])
			&& (table[i][0]&0xffff) != 0xffff) // do not join plane sentinels
			nt[-1][2] = (table[i][0]+table[i][2])-nt[-1][0];
		else
			nt += ({ table[i] });
	}
	table = nt;

	// Find plane start points
	array planes = ({({"0", "0"})});
	for (int i=0; i<14; i++)
		planes+= ({({"-1", "-1"})});
	int p = 0, prevp = 0, mark = 0;
	foreach (table, array t)
	{
		if (get_plane(t[0]) > p)
		{
			p = get_plane(t[0]);
			planes[p][0] = (string)mark;
			planes[prevp][1] = (string)(mark-1);
			prevp = p;
		}
		mark++;
	}
	planes[-1][1] = (string)(sizeof(table)-1);

	int ent, lbl;
	string data = "", tbl="";

	// Create the run-length encoded tables C++ code (one with indices,
	// the other with data)
	foreach (table, array t)
	{
		if (ent ++)
		{
			data += ", ";
			tbl  += ", ";
		}
		if (!((ent - 1) & 7))
		{
			data += "\n\t";
			tbl  += "\n\t";
		}

		tbl += sprintf("0x%04x",t[0]&0xffff);
		data += t[1];
	}

	// Now write everything to the data file
	werror("%O/%O entries\n", ent, sizeof(table));
	write( #"/** @file charclass.inl
 * This file is auto-generated by modules/unicode/scripts/make_charclass.pike.
 * DO NOT EDIT THIS FILE MANUALLY.
 */\n
#ifdef USE_UNICODE_INC_DATA\n");
	write( "static const short cls_planes[][2] = {\n\t");
	foreach (planes, array p) 
		write( "{" + p*"," + "},");
	write("\n};\n");
	write( "static const uni_char cls_chars[] = {");
	write( tbl+"\n};\n");

	write( "static const char cls_data_flat[] = {\n\t"+
	 (cls_flat*",")+"\n};\n");
	write( "static const char cls_data[] = {");
	write( data+"};\n");
	write( "#endif // USE_UNICODE_INC_DATA\n");
}