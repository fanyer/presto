#!/usr/bin/pike
/* -*- Mode: pike; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

int compress, quiet;


void output( string fmt, mixed ... args )
{
	if( !quiet )
		write( fmt, @args );
}

// On-disk format:
//   ntables <2bytes>
//   ntabels * [
//     <4bytes tablesize>
//     <1byte namesize>
//     <namesize bytes name> 
//     <4bytes offset>
//  the various tables have different internal formats.

#ifndef BIGENDIAN
#  define BIGENDIAN 0
#endif

string UNICHAR = "%-2c";
string INTEGER = "%-4c";

void EXIT( int e, mixed ... a )
{
	if( sizeof( a ) ) werror(@a);
	exit(e);
}


class Table( string name, int size, int offset )
{
    string orig_name;
    enum TableFormat { FLAT,MAP };
    int orig_size;
    int first, row_cell;
    array|mapping table;
    TableFormat format;
    string data;

    array not_in_rev=({});
    array extra_2=({});

    string revbuild()
    {
		string res = "";
		if( name == "big5" ) name = "big5-table";
		else if( name == "gbk" ) name = "gbk-table";
	
		if( sizeof( not_in_rev ) || sizeof(extra_2) )
		{
			res = sprintf("%c%s",strlen(name),name);
			res += sprintf("%c", sizeof(not_in_rev));
			foreach(not_in_rev, [int i, int v] )
				res += sprintf(UNICHAR + UNICHAR, i, v);
			res += sprintf("%c", sizeof(extra_2));
			foreach(extra_2, [int i, int v] )
			{
				res += sprintf(UNICHAR + UNICHAR,i,v);
			}
		}
		return res;
    }

    static int `<( Table x )
    {
		if( !objectp(x) )
			return -1;
		return x->name < name;
    }

    static int `>( Table x )
    {
		return x->name > name;
    }

    void read_ushorts( int|void rev )
    {
		format = FLAT;
		fd->seek( offset );
		string data = fd->read( size );
		table = allocate(size/2);
		if( rev )
			for( int i = 0; i<size/2; i++ )
				table[i] = (data[i*2+1]+(data[i*2]<<8));
		else
			for( int i = 0; i<size/2; i++ )
				table[i] = (data[i*2]+(data[i*2+1]<<8));
    }
        
    void read_byte_ushorts( )
    {
		format = MAP;
		fd->seek( offset );
		string data = fd->read( size );
		table = ([]);
		for( int i = 0; i<size/3; i++ )
#if BIGENDIAN = 1
			table[ data[i*3+1]+(data[i*3]<<8) ] = data[i*3+2];
#else
	    table[ data[i*3]+(data[i*3+1]<<8) ] = data[i*3+2];
#endif
    }

    void read_ushort_ushorts(int|void rev)
    {
		format = MAP;
		fd->seek( offset );
		string data = fd->read( size );
		table = ([]);

		int add1=0, add2=1, add3=2, add4=3;
		if( rev & 1 ) {
			add1=1;add2=0;
		}
		if( rev & 2 ) {
			add3=3;add4=2;
		}
		for( int i = 0; i<size/4; i++ )
			table[data[i*4+add1]+(data[i*4+add2]<<8)] = (data[i*4+add3]+(data[i*4+add4]<<8));
    }
        
    int populate( )
    {
		if( table ) 
			return 1;

		mkdir("split");
		fd->seek( offset );
		Stdio.write_file( "split/"+name, fd->read( size ) );
		orig_name = name;
		switch( name )
		{
			case "ksc5601-rev-table-1":
				first=0xac00;
				read_ushorts( BIGENDIAN );
				trim();
				return 1;
			case "big5-rev-table-1":
			case "cns11643-rev-table-1":
			case "gbk-rev-table-1":
			case "jis-0208-rev-1":
			case "jis-0212-rev-1":
				first=0x4e00;
				read_ushorts( BIGENDIAN );
				trim();
				return 1;
			case "big5-rev-table-2":
			case "ksc5601-rev-table-2":
			case "cns11643-rev-table-2":
			case "gbk-rev-table-2":
			case "jis-0208-rev-2":
			case "jis-0212-rev-2":
			case "hkscs-plane-0":
			case "hkscs-plane-2":
				read_ushort_ushorts( BIGENDIAN );
				return 1;
			case "koi8-r-rev":
				read_byte_ushorts( );
				return 1;
			case "gbk-table":
				read_ushorts( BIGENDIAN );
				row_cell=5;
				return 1;
			case "cns11643-table":
				row_cell=4;
				read_ushorts( BIGENDIAN );
				return 1;
			case "ksc5601-table":
				row_cell=2;
				first=0;
				read_ushorts( BIGENDIAN );
				return 1;
			case "big5-table":
				row_cell=3;
				first=0;
				read_ushorts( BIGENDIAN );
				return 1;
			case "jis-0208":
			case "jis-0212":
				row_cell=1;
				read_ushorts( BIGENDIAN );
				return 1;
		}
		if( size == 256 || size == 512 )
		{
			read_ushorts( BIGENDIAN );
			if( size == 256 )
				first=128;
			else
				first=0;
			return 1;
		}
		if( size < 1024 )
		{
			read_byte_ushorts( );
			return 1;
		}
		output("Don't know the format for '%s'. Skipping.\n", name );
    }

    int code_for_ind( int n )
    {
		if( !table )
		{
			populate();
			if( !table )
				table = ({});
			format = FLAT;
		}

		if( row_cell && format == FLAT)
		{
			int cell, row, mult, sub;
			switch( row_cell )
			{
				case 1: // JIS
					cell = n % 94;
					row = n / 94;
					cell += 0x21;
					row += 0x21;
					return (row<<8) | cell;
				case 2: // BIG5 KSCS
					if( n < 12460 )
					{
						cell = n%178;
						row = n / 178;
						if( cell > 51 ) cell += 6;
						if( cell > 25 ) cell += 6;
						cell += 0x41;
						row += 0x81;
					}
					else
					{
						n -= 12460;
						cell = n % 94;
						row = n / 94;
						row += 0xc7;
						cell += 0xa1;
					}
					return (row<<8) | cell;
				case 3: // BIG5
					cell = n % 191;
					row = n / 191;
					cell += 0x40;
					row += 0xa1;
					return (row<<8) | cell;
				case 4: // EUC-TW
					row = n / 94;
					cell = n % 94;
					int plane = (row / 93)+1;
					row %= 93;
					row += 0x21;
					cell += 0x21;
					return (plane<<14)|(row<<7)|cell;
				case 5: // GBK
					row = n / 191;
					cell = n % 191;
					row += 0x81;
					cell += 0x40;
					return (row <<8 )|cell;
			}
		}
		if( format == FLAT )
			return n+first;
		return n;
    }

    int get( int n )
    {
		if( !table )
		{
			populate();
			if( !table )
				table = ({});
			format = FLAT;
		}

		if( row_cell && format == FLAT)
		{
			int cell, row, mult, sub;
			switch( row_cell )
			{
				case 1: // JIS
					cell = (n&255)-0x21;
					row = (n>>8)-0x21;
					mult=94;
					break;

				case 2: // BIG5 KSCS
					row = (n>>8);
					cell = (n&255);

					if( row < 0xc7 )
					{
						row-=0x81;
						if( cell > 0x80 ) 
							cell -= 6;
						if( cell > 0x60 ) 
							cell -= 6;
						cell -= 0x41;
						mult = 178;
					}
					else
					{
						sub = -12460;
						row -= 0xc7;
						cell -= 0xa1;
						mult=94;
					}
					break;
				case 3: // BIG5
					cell = (n&255);
					row = (n>>8);
					mult=191;
					row-=0xa1;
					cell -= 0x40;
					break;
				case 4: // EUC-TW (CNS-11643)
					int plane = n >> 14;
					row = ((n >> 7) & (0x7F)) - 0x21;
					cell = (n & 0x7F) - 0x21;
					row += (plane-1) * 93;
					mult=94;
					break;
				case 5: // GBK
					row = (n>>8)-0x81;
					cell = (n&255)-0x40;
					mult = 191;
					break;
			}
			n = cell + (row * mult) - sub;
		}
	    
		if( format == FLAT )
		{
			n-=first;
			if( n < sizeof(table) && n >= 0 )
				return table[n];
			return -1;
		}
		else
			return table[n];
    }

    void debug_print()
    {
		if( !table ) 
			write("NIL\n");
		else if( format == MAP )
			foreach( sort(indices(table)), int n )
				write( "%x -> %x\n", n, table[n] );
		else
			for( int n=0; n<sizeof(table); n++ )
				write( "%x -> %x\n", n+first, table[n] );
    }

    void check_fwd_2( Table ... rev )
    {
		if( format == MAP )
			EXIT(1,"MAP not expected here\n");
		rev->populate();
		for( int n = 0; n<sizeof(table); n++ )
		{
			if( table[n] != 65533 )
			{
				int ok=0;
				foreach( rev, Table r )
				{
					if( r->get( table[n] ) == code_for_ind( n ) )
						ok=1;
				}
				if( !ok )
				{
					if( !sizeof( not_in_rev ) )
						not_in_rev += ({ ({ n, n }) });
					else
						if( not_in_rev[-1][-1] == n-1 )
							not_in_rev[-1][-1]++;
						else
							not_in_rev += ({ ({ n, n }) });
				}
			}
		}
		Table rev2;
		if( sizeof( rev ) > 1 )
			rev2 = rev[1];
		certain-=sizeof(not_in_rev)*4;
		build_rev( rev[0], rev2 );
    }

    void check_fwd( )
    {
		populate();
		if( !table )
			return;
	
		if( tables[name+"-rev"] )
			check_fwd_2( tables[name+"-rev"] );
		else
		{
			sscanf( name, "%s-table", name );
			array t = ({ tables[name+"-rev-table-1"],tables[name+"-rev-table-2"]});
			t += ({ tables[name+"-rev-1"],tables[name+"-rev-2"]});
			t-= ({0});
			if( sizeof(t)==2 )
				check_fwd_2( @t );
// 			else if( !(<"charsets","uniblocks">)[name] )
// 				werror("Nothing to compare %s with\n", name);
		}
		name = orig_name;
    }

    mixed build_rev( Table check1, Table check2 )
    {
		mapping|array rev1;
		mapping rev2 = ([]);
		int cutoff, rev_start;

		if( check1->format == MAP )
		{
			cutoff = 0xfeeeeeeee;
			rev_start=0;
			rev1 = ([]);
		}
		else
		{
			rev_start = check1->first;
			cutoff = rev_start+sizeof(check1->table);
			rev1 = allocate( cutoff-rev_start );
		}
// 	if( cutoff != 0xfeeeeeeee )
// 	    werror("Generating rev for %s: %x-%x in first table; other in second\n", 
// 		   name, rev_start, cutoff);
// 	else
// 	    werror("Generating rev for %s: all in first table [map]\n",name);

		int cc;
		for( int n = 0; n<sizeof(table); n++ )
		{
			while( cc < sizeof(not_in_rev) && not_in_rev[cc][1] < n )
				cc++;
			if( cc < sizeof( not_in_rev ) && not_in_rev[cc][0] <= n && not_in_rev[cc][1] >= n )
			{
				continue;
			}
			int cp = code_for_ind(n);
			if( table[n] < cutoff && table[n] >= rev_start && table[n] != 65533 )
			{
				if( mappingp( rev1 ) && !n && !table[n] )
				{
					continue;
				}
				rev1[ table[n] - rev_start ] = cp;
			}
			else if( table[n] != 65533 )
			{
				if( !check2 )
					EXIT(1,"We should not be here. %d %d %d %d\n", n,table[n],cutoff,rev_start);
				rev2[table[n]]=cp;
			}
		}
		int miss;
		if( !equal( check1->table, rev1 ) && ((rev1[0]=0),!equal(check1->table, rev1 )))
		{
			write("%s: Table 1 differs\n", name);
			if( mappingp( rev1 ) )
			{
				write("[[\n");
				foreach( rev1; int i; int v )
					if( !check1->table[i] || check1->table[i] != v )
						write("** SHOULD NOT HAPPEN: Extra %x: %x<>%x\n", i, v, check1->table[i] );
				foreach( check1->table; int i; int v )
					if( !rev1[i] )
						write("Missing %x: %x<>%x\n", i, v, rev1[i] );
				write("]]\n");
			}
			else
				werror("Start: %O\nShould be: %O\n", rev1[..10], check1->table[..10] );
			miss++;
		}
		else
		{
// 	    werror("ALL OK for %s-1\n", name );
			check1->table = rev1;
			check1->check_encode();
		}

		if( check2 )
		{
			if( !equal( rev2, check2->table ) )
			{
// 				write("%s: Table 2 differs <saveable>\n", name);
				foreach( rev2; int i; int v )
					if( !check2->table[i] || check2->table[i] != v )
						EXIT(1,"** SHOULD NOT HAPPEN: Extra %x: %x<>%x\n", i, v, check2->table[i] );
				foreach( check2->table; int i; int v )
				{
					if( !rev2[i] )
					{
						if( i < cutoff && i >= rev_start )
							EXIT(1,"Hm. Seen 'inline' in 'outline' table.\n");
						else 
							extra_2 += ({ ({ i, v }) });
					}
				}
				check2->check_encode();
			}
			else
			{
// 				werror("ALL OK for %s-2\n", name );
				check2->table = rev2;
				check2->check_encode();
			}
		}

		if( miss ) // No need to save not-in-rev for this case.
			not_in_rev=({});
    }

    int nosave;
    void check_encode()
    {
		// for now
		nosave=1;
    }

    void trim()
    {
		int trimmed;
		if( name[..2] == "iso" ) return;
		if( name[..6] == "windows" ) return;
		if( format == FLAT && table && sizeof(table))
		{
			while( table[-1] == 65533 && table[-1])
			{
				table = table[..sizeof(table)-2];
				size-=2;
				trimmed+=2;
			}
		}
		if( trimmed ) {
			mkdir("split");
			fd->seek( offset );
			Stdio.write_file( "split/"+name, fd->read( size ) );
		}
    }

    void check_rev( )
    {
		populate();
		if( !table )
			return;
		string rname = name;
		sscanf( rname, "%s-rev", rname );
		Table inv = tables[rname];
		if( !inv && !has_value( rname, "table" ) )
		{
			rname += "-table";
			inv = tables[rname];
		}
		if( inv )
		{
			if( !inv->populate() )
			{
				werror("can not read %s\n", inv->name);
				return;
			}
			if( format == MAP )
			{
				int ok = 1;
				foreach( sort(indices(table)), int n )
				{
					if( inv->get( table[n] ) != n )
					{
// 			werror("%s:%x:%x // <-> %x\n", name, n, table[n], inv->get(table[n]) );
						certain-=4;
					}
				}
				certain += size;
			}
			else
			{
				int ok = 1;
				for(int n=0; n<sizeof(table); n++ )
				{
					if( table[n] && (inv->get( table[n] ) != n+first) )
					{
// 			werror("%s:%x:%x // <-> %x\n", name, n+first, table[n], inv->get(table[n]) );
// 			if( search( inv->table, n+first ) != -1 )
// 			{
// 			    werror("Found as %x in %s\n", search( inv->table, n+first ), inv->name);
// 			}
						certain-=4;
					}
				}
				certain += size;
			}
		}
		else
			werror("No forward table for %s\n", name );
    }

    static string _sprintf( int flag, mapping options )
    {
		switch( flag )
		{
			case 't':
				return "Table";
			case 'O':
				return sprintf( "%t(%O %d %d)", this, name, size, offset );
		}
    }
}

#define COMPRESS

int certain, rev_remain;
mapping(string:Table) tables = ([]);
Stdio.File fd;

string table_data = "";

string fmtsz( int x )
{
	if( x > 1024 )
		return sprintf("%6.2fK", x/1024.0 );
	else
		return sprintf("%7d", x);
}

int find_data_offset( string x, Table tbl )
{
	if( compress /*&& tbl->name != "charsets"*/ )
    {
		// Lets try delta compression.
		string res = "";
		int old_code, new_code, too_large;
		for( int i=0; i<strlen(x)/2; i++ )
		{
			int new_code = array_sscanf( x[i*2..i*2+1], UNICHAR )[0];
			res += sprintf("%3c", new_code-old_code);
			if( new_code-old_code > 32767 || new_code-old_code < -32768 )
				too_large++;
			old_code = new_code;
		}
		string a = Gz.deflate( 9 )->deflate( res );
		string b = Gz.deflate( 9 )->deflate( x );
		if( strlen( a )+50 < strlen( x ) )
		{
			output( "%-20s %7s   %7s (%3d%%)\n", tbl->name,
					fmtsz(strlen(x)), fmtsz(strlen(a)),strlen(a)*100/strlen(x) );
			tbl->data = a;
			tbl->orig_size = strlen(x);
			x = a;
			tbl->size = strlen(a);
		}
		else
			output( "%-20s %7s   %7s (100%%)\n", tbl->name,fmtsz(strlen(tbl->data)),fmtsz(strlen(tbl->data)) );
    }
	else
		output( "%-20s %7s   %7s (100%%)\n", tbl->name,fmtsz(strlen(tbl->data)),fmtsz(strlen(tbl->data)) );



//     int off;
//     if( (off=search( table_data, x )) != -1 )
//     {
// 	werror("%d bytes found inside current table data\n", strlen( x ) );
// 	return off;
//     }
//     for( int i=strlen(x)-1; i>=0; i-- )
// 	if( has_suffix( table_data, x[..i] ) )
// 	{
// //  	    write("Table data ends with %d bytes (%O vs %O)\n", i+1, 
// //  		  table_data[sizeof(table_data)-1-i..],x[..i]);
// 	    table_data += x[i+1..];
// 	    return strlen(table_data)-strlen(x)-i-1;
// 	}
    int o = strlen(table_data);
    table_data += x;
    return o;
}

int write_tables( string n )
{
    Stdio.File out = Stdio.File(n, "wct" );
    int offset;
    out->write( "\0\0" );
    string revbuild_table = "";
    mapping offsets = ([]);

    // Calculate offsets.
	output("=============================================\n");
	output("Table               Original       Compressed\n");
	output("=============================================\n");
	array q = values(tables);
	sort( q->size, q );
	int left, right;
    foreach( reverse(q), Table tbl )
    {
		tbl->trim();
		fd->seek( tbl->offset );
		string tdata = fd->read( tbl->size );
		tbl->data = tdata;
		left += strlen(tdata);
		revbuild_table += tbl->revbuild();
		if( !tbl->nosave )
		{
			offsets[tbl->name] = find_data_offset( tdata, tbl );
			right += strlen(tbl->data);
		}
		else
			output( "%-20s %7s           --\n", tbl->name,fmtsz(strlen(tbl->data)) );
			
    }
	output( "%-20s      --   %s (----)\n", "zz-revbuild", fmtsz(strlen(revbuild_table)));

	output("=============================================\n");
	output( "%-20s %6.1fK %8.1fK (%3d%%)\n", "Total", left/1024.0, (right+strlen(revbuild_table))/1024.0,
			(right*100)/left);
	output("=============================================\n");

	output("\n");
	output("Actual filesize (including headers):\n");
	
    // Write index.
    foreach( sort(indices(tables)), string x )
    {
		Table tbl = tables[x];
		// Copy selected tables only.
		if( !tbl->nosave )
		{
			tbl->offset = offsets[tbl->name];
			if( glob( "*-rev*", x ) )
			{
				write("Need to fix: %s (%d bytes)\n", x, sizeof(tbl->data) );
				rev_remain += strlen(tbl->data);
			}
			if( tbl->orig_size )
			{
				out->write( INTEGER, (strlen(tbl->data)<<16) | tbl->orig_size );
			}
			else
			{
				out->write( INTEGER, strlen(tbl->data) );
			}
			out->write( "%c%s", strlen(tbl->name), tbl->name );
			out->write( INTEGER, tbl->offset );
		}
		else
		{
			out->write( INTEGER, 0 );
			out->write( "%c%s", strlen(tbl->name), tbl->name );
			out->write( INTEGER, 0 );
		}
    }

    Stdio.write_file( "split/zz-revbuild", revbuild_table );
    out->write( INTEGER, strlen(revbuild_table) );
    out->write( "%c%s", strlen("zz-revbuild"), "zz-revbuild" );
    out->write( INTEGER, strlen(table_data) );

    // Write actual data.
    int ntables=sizeof(tables)+1;
    out->write( table_data );
    out->write( revbuild_table );
    out->seek(0);
    out->write( UNICHAR, ntables );
    return out->stat()->size;
}

int get_uint( Stdio.File f )
{
	string q = f->read( 4 );
	int res;
	sscanf( q, INTEGER, res );
	return res;
}

void main( int argc, array argv )
{
	if( argc<2 || has_value( argv, "--help" ) || has_value( argv, "-?" ) || has_value( argv, "-h"))
	{
		write(
#"This program removes the reverse tables from a chartablefile and in
the process generates a table that is needed to build them
dynamically.

It can also compress the tables.

Syntax: %s [--help] [--compress] [--quiet] chartables-file

      --help:       This information
      --compress:   Compress the remaining tables
      --quiet:      No output
      --be:         Big endian

");
		EXIT(0);
	}


	quiet = has_value( argv, "--quiet" );
	compress = has_value( argv, "--compress" );

	if( has_value( argv, "--be" ) )
	{
		INTEGER = "%4c";
		UNICHAR = "%2c";
	}

	argv -= ({ "--quiet", "--compress", "--be" });

    fd = Stdio.File( );
	if( !fd->open(argv[1], "r" ) )
	{
		werror( "Failed to open %O\n", argv[1] );
		EXIT(1);
	}
	int total  = fd->stat()->size;
    string q = fd->read( 2 );
    int ntables;
    sscanf( q, UNICHAR, ntables );

	output("The file %O has %d table%s\n", argv[1], ntables, ntables!=1?"s":"");

    for( int i=0; i<ntables; i++ )
    {
		int size, offset;
		string name;
		size = get_uint(fd);
		name = fd->read( fd->read(1)[0] );
		offset = get_uint(fd);
		tables[name] = Table( name, size, offset );
    }

    foreach( values(tables), object t )
		t->offset += fd->tell();

    int rev_total, rev_large, fwd_large;
    foreach( sort(values(tables)), Table t )
    {
		if( glob( "*-rev*", t->name ) )
		{
			t->check_rev( );
			rev_total += t->size;
			if( t->size > rev_large )
				rev_large = t->size;
		}
		else
		{
			if( t->size > fwd_large )
				fwd_large = t->size;
			t->check_fwd();
		}
    }

    int newsz = write_tables( argv[1]+".out" );
    output( "%.2fk left, %d%% (%.2fk) gone (%s compression)\n",
			newsz/1024.0, (total-newsz)*100/total, (total-newsz)/1024.0,(compress?"with":"without"));
}
