#!/usr/bin/pike
/* -*- Mode: pike; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

enum CaseInfoType { NONE,UPPERDELTA, LOWERDELTA, CASEBIT, BITOFFSET, BOTHDELTA, LARGEUPPER, LARGELOWER };

mapping upper = ([]);
mapping lower = ([]);
mapping upper_nonbmp = ([]);
mapping lower_nonbmp = ([]);
int nonbmp_highest = 0;
int nonbmp_lowest = 0x10FFFF;

int main(int argc, array(string) argv)
{
    int lineno;
    array(array(int)) ci = ({({0,NONE,0})}); /* Information for each codepoint */
    int prevchar = 0; /*255*/;

    string data = Stdio.stdin.read();

	// Read the Unicode character database (UnicodeData.txt)
    foreach(data/"\n", string line)
    {
		lineno++;
		line -= "\r";

		// Split line into semi-colon separaterd fields
		array(string) info = line/";";

		if (!sizeof(line)) continue;

		// Check that the file format hasn't changed
		if (sizeof(info) != 15)
		{
			werror(sprintf("Syntax error on line %d: "
						   "Bad number of fields:%d (expected 15)\n"
						   "%O\n",
						   lineno, sizeof(info), line));
			exit(1);
		}

		// The first field contains the codepoint value
		int char;
		sscanf(info[0], "%x", char);

		// Ignore characters below U+0080
		if (char < 128)
			continue;

		// Simple handling of non-BMP codepoints (since there aren't that many
		// of them that have case mappings
		if (char > 65535)
		{
			// If Simple_Lowercase_Mapping contains data, this is an uppercase
			// character. Use the lowercase mapping value.
			if (sizeof(info[13]))
			{
				int d;
				sscanf(info[13], "%x", d);
				lower_nonbmp[char] = d;
				if (sizeof(info[12]))
				{
					werror("Non-BMP titlecase not supported: U+%x", char);
					exit(1);
				}

				if (char > nonbmp_highest)
				{
					nonbmp_highest = char;
				}
				if (char < nonbmp_lowest)
				{
					nonbmp_lowest = char;
				}
			}

			// If Simple_Uppercase_Mapping contains data, this is a lowercase
			// character. Use the uppercase mapping value.
			if (sizeof(info[12]))
			{
				int d;
				sscanf(info[12], "%x", d);
				upper_nonbmp[char] = d;

				if (char > nonbmp_highest)
				{
					nonbmp_highest = char;
				}
				if (char < nonbmp_lowest)
				{
					nonbmp_lowest = char;
				}
			}

			continue;
		}

		CaseInfoType mode = NONE;
		int deltaval;

		// Remember default self mappings
		upper[char] = sprintf("%c", char);
		lower[char] = sprintf("%c", char);

		// If Simple_Lowercase_Mapping contains data, this is an uppercase
		// character. Use the lowercase mapping value.
		if (sizeof(info[13]))
		{
			// Default to using delta
			mode = UPPERDELTA;
			int d;
			sscanf(info[13], "%x", d);
			lower[char] = sprintf("%c", d );
			int delta = d - char;

			if (!sizeof(info[12]))
			{
				// See if we can optimize
				if (!(delta & (delta - 1)) && (delta > 0))
				{
					if (d & delta)
					{
						// Delta is a simple bit flip
						mode = CASEBIT;
					}
					else
					{
						// Delta is an offset bit flip
						mode = BITOFFSET;
					}
				}
			}
			if (delta < -32768 || delta > 32767)
			{
				if (UPPERDELTA == mode && !(delta & 1))
				{
					mode = LARGEUPPER;
				}
				else if (LOWERDELTA == mode && !(delta & 1))
				{
					mode = LARGELOWER;
				}
				else
				{
					werror("Delta too large not handled for mode %d (U+%x: %d)\n", mode, char, delta);
				}
				delta /= 2;
			}
			deltaval = delta;
		}

		// If Simple_Uppercase_Mapping contains data, this is a lowercase
		// character. Use the uppercase mapping value.
		if (sizeof(info[12]))
		{
			int d;
			sscanf(info[12], "%x", d);
			upper[char] = sprintf("%c", d);
			int delta = char - d;

			if (mode != NONE)
			{
				// This is a titlecase letter, and has both lowercase
				// and uppercase mappings. For all such characters in
				// Unicode 5.0, the delta for upper and lowercase
				// conversions are opposite (all are -1 and +1), so we
				// use a special "double delta" type
				if (deltaval != delta)
				{
					exit(1, "U+%04X has different deltas for lowercase and uppercase mappings (%d vs %d)\n",
					     char, deltaval, delta);
				}

				mode = BOTHDELTA;
			}
			else
			{
				// Default to using delta
				mode = LOWERDELTA;

				// See if we can optimize
				if (!(delta & (delta - 1)) && (delta > 0))
				{
					if (char & delta)
					{
						// Delta is a simple bit flip
						mode = CASEBIT;
					}
					else
					{
						// Delta is an offset bit flip
						mode = BITOFFSET;
					}
				}
				if (delta < -32768 || delta > 32767)
				{
					if (UPPERDELTA == mode && !(delta & 1))
					{
						mode = LARGEUPPER;
					}
					else if (LOWERDELTA == mode && !(delta & 1))
					{
						mode = LARGELOWER;
					}
					else
					{
						werror("Delta too large not handled for mode %d (U+%x: %d)\n", mode, char, delta);
					}
					delta /= 2;
				}
				deltaval = delta;
			}
		}

		// Store the information for this codepoint

		if ((char > prevchar+1) && (ci[-1][1] != NONE))
		{
			// Add a NONE-range.
			ci += ({({ prevchar+1, NONE, 0 })});
		}

		if ((ci[-1][1] != mode) || (ci[-1][2] != deltaval))
		{
			// New range for new delta value
			ci += ({({ char, mode, deltaval })});
		}

		// Remember this character
		prevchar = char;
    }

	// Write information for the selftest test suite
    string upper_table="", lower_table="";
    for( int i=0; i<65536; i++ )
    {
		upper_table += (upper[ i ]||"\0");
		lower_table += (lower[ i ]||"\0");
    }
    for( int i=65536; i<131072; i++ )
    {
		// All SMP codepoints map to inside the SMP, so only store the low
		// 16 bits.
		upper_table += (sprintf("%c", upper_nonbmp[i] ? (upper_nonbmp[i] & 0xFFFF) : '\0'));
		lower_table += (sprintf("%c", lower_nonbmp[i] ? (lower_nonbmp[i] & 0xFFFF) : '\0'));
    }

    Stdio.write_file( "lower.dat", string_to_unicode( lower_table ) );
    Stdio.write_file( "upper.dat", string_to_unicode( upper_table ) );

	// Creat the indirect data table
    array(int) table = allocate(sizeof(ci));
    mapping ci_table = ([]);
    mapping ci_rev_table = ([]);
    int next_entry=1, next_ci_entry=1;

	// Look up the codepoint information in ci (an entry from the
	// global ci table) to find if it is already present in the
	// ci_rev_table table (target
    int unique_case_info(array ci)
    {
		if(!ci[1])
			return 0;

		string x = sprintf("%d%d", ci[1], ci[2]);

		if (!ci_table[x])
		{
			ci_rev_table[next_ci_entry] = ci;
			return (ci_table[x] = ({ next_ci_entry++, ci }))[0];
		}

		return ci_table[x][0];
    };

    write( #"/** @file caseinfo.inl
 * This file is auto-generated by modules/unicode/scripts/make_caseinfo.pike.
 * DO NOT EDIT THIS FILE MANUALLY.
 */

#ifdef USE_UNICODE_INC_DATA
enum CaseInfoType {
	UPPERDELTA = 0,
	LOWERDELTA = 1,
	CASEBIT = 2,
	BITOFFSET = 3,
	BOTHDELTA = 4,
	LARGEUPPER = 5,
	LARGELOWER = 6,
	NONBMP = 7
};

static const uni_char case_info_char[] = {");

	// Remember max indices used for certain codepoints
    int max_1024, max_256;

	// Write codepoint index table
	int entries;
    for (int i = 0; i < sizeof (ci); i++)
	{
		if (entries ++)
			write(", ");
		if (!(i & 7))
			write("\n\t");

		write("0x%04x", ci[i][0] );
		if (!max_1024 && ci[i][0] >= 1024)
		{
			max_1024 = i;
		}

		if (!max_256 && ci[i][0] >= 256)
		{
			max_256 = i;
		}

		// Find index for the indirect lookup table, this is stored
		// in the companion table
		table[i] = unique_case_info(ci[i]);
    }
    write("\n};\n\n" );
    write("#define CI_MAX_256 "+max_256+"\n");
    write("#define CI_MAX_1024 "+max_1024+"\n\n");

	// Write the indirect lookup table. This table has as many entries
	// as there are codepoint ranges defined by the first table, and
	// contain indices for the casing information table.
    write("static const unsigned char case_info_ind[] = {");
	entries = 0;
    for( int i=0; i<sizeof(table); i++ )
    {
		if (entries ++)
			write(", ");
		if (!(i & 15))
			write("\n\t");
		write("%d", table[i]);

    }
    write("\n};\n\n" );

    if( next_ci_entry > 255 )
    {
		werror("Too many case info indexes.\n");
		exit(1);
    }

//     werror("%O different case infos\n", next_ci_entry-1);

	// data_table is a mapping from the indirect table to the index of
	// the delta values
    mapping data_table = ([]);

	// Find the index in the generated case_info_data_table that correspond
	// to delta value x and advance the counter if we need to create a new
	// entry
    int get_data_table(int delta)
    {
		if (zero_type(data_table[delta]))
		{
			return data_table[delta] = next_entry++;
		}
		return data_table[delta];
    };

	// Set up the data table for the delta values by counting
	// the number of unique entries
    for (int i = 1; i < next_ci_entry; i ++)
	{
		get_data_table(ci_rev_table[i][2]);
	}

	// Find out how many bits are needed for the indices to the third
	// table (the bitmask/offset table)
	int case_info_bits = (int)(log((float)next_entry)/log(2.0)+1);
	string type = "char";

	if (case_info_bits > 6)
	{
		type = "short";
	}
	if (case_info_bits > 14)
	{
		type = "int";
	}
	if (case_info_bits > 30)
	{
		werror("Too many case infos.\n");
		exit(1);
	}

	// Write the case information table. This table contains the indices
	// for the bitmask/offset table, and information on how to use them.
    write( #"
#define CASE_INFO_BITS %d
static const unsigned %s case_info[] = {", case_info_bits, type);

	entries = 0;
    for (int i = 1; i < next_ci_entry; i ++)
    {
		if (entries ++)
			write(", ");
		if (!((i - 1) & 7))
			write("\n\t");

		// Since we have already run through the loop above, get_data_table
		// should not create any more indices. Find the index to the
		// generated case_info_data_table by looking up the index stored
		// int the data_table mapping, and add the mapping type bitmask
		// to it.
		int c =
			((ci_rev_table[i][1] - 1) << case_info_bits) |
			get_data_table(ci_rev_table[i][2]);
		write("%d", c);

    }
    write("\n};\n\n" );

	// Write the offset/bitmask table
    write("static const short case_info_data_table[] = {");
	entries = 0;
    for (int i = 0; i < next_entry; i ++)
    {
		if (entries ++)
			write(", ");
		if (!(i & 7))
			write("\n\t");

		// Find the value for this index. data_table is an inverse lookup
		// table, in which we store the deltas mapped to the first index
		// they occur for.
		int value = search(data_table, i);
		if (value > -32768 && value < 32768)
		{
			write("%d", search(data_table, i));
		}
		else
		{
			werror("Too large deltas between upper and lowercase (%d).\n", value);
			exit(1);
		}
    }
    write("\n};\n" );

	// Write the non-BMP mapping table; since the characters are so rare and
	// the number of mappings so small, keep it simple
	write("\n#define NONBMP_CASE_LOWEST 0x%X\n#define NONBMP_CASE_HIGHEST 0x%X\n",
	      nonbmp_lowest, nonbmp_highest);
	write("static const struct NonBMPCase nonbmp_case[] = {\n");
	entries = 0;
    for (int i = nonbmp_lowest; i <= nonbmp_highest; i++)
	{
		// Sort on the lowercase codepoint
		if (upper_nonbmp[i])
		{
			// Ensure that the two-way map is consistent.
			if (lower_nonbmp[upper_nonbmp[i]] != i)
			{
				werror("Uppercase mapping for U+%x missing lowercase mapping for U+%x\n", i, upper_nonbmp[i]);
				exit(1);
			}
			if (entries ++)
			{
				write(",\n");
			}
			write("\t{ 0x%08X, 0x%08X }", i, upper_nonbmp[i]);
		}

		// Ensure twoway map
		if (lower_nonbmp[i])
		{
			if (upper_nonbmp[lower_nonbmp[i]] != i)
			{
				werror("Lowercase mapping for U+%x missing uppercase mapping for U+%x\n", i, upper_nonbmp[i]);
				exit(1);
			}
		}
	}
	write("\n};\n#endif // USE_UNICODE_INC_DATA\n");

    exit(0);
}
