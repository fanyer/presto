#!/usr/bin/pike

void main()
{
    foreach( Stdio.stdin.line_iterator(); ; string line )
	if( has_value( line, ";" ) )
	    write( String.trim_whites((line/"#")[0])+"\n" );
	else
	    write( line+"\n" );
}
