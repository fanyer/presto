#!/usr/bin/perl
#
# Parse memory allocation logs (primarily from Linux[*] at the moment) of
# the form:
#
#    MEM: %p memop(args) ...
#
# Which will be output to stdout when TWEAK_MEMORY_LOG_ALLOCATIONS is
# applied.  Lines not relevant for this tool are just printed.
#
# The output is some statistics on stderr, and a graph on stdout that
# can be fed directly to the 'xgraph' program.
#
# This anayser is by no means complete, so other utilities for inspecting
# fragmentation, object life expectancy etc. can be analysed.
# handled.
#
# Example:
#   $ lingogi > memrun1.log
#   $ .../memlogparser.pl < memrun1.log | xgraph
#
# If you want to look at a specific call-site, this can be done by giving
# the call-site address as arguments, e.g.:
#
#   $ .../memlogparser.pl 0x08c8e4b2 < memrun1.log | xgraph
#
#
# [*] Wingogi does not have a good way of capturing system debugging output,
#     so a different logging facility will probably have to be used.
#

print "TitleText: Memory consumption\n";
&initgraph(0, "Top-of-heap");
&initgraph(1, "Allocated");
&initgraph(2, "Num objects");

$gn = 3;
while ( $#ARGV >= 0 ) {
    $site = shift(@ARGV);
    print STDERR "Looking at allocation site $site\n";
    $site2graph{$site} = $gn;
    &initgraph($gn, "Site $site");
    $gn++;
}

$basemem = 0;

while (<STDIN>) {
    $line = $_;
    if ( $line =~ /^MEM:\s(0x\S+)\srealloc\((0x[^,]+),(\d+)\)\s=>\s(0x\S+)/ ) {
	$site = $1;
	$oldptr = $2;
	$size = $3;
	$ptr = $4;
	&realloc($site, $oldptr, $size, $ptr);
    } elsif ( $line =~ /^MEM:\s(0x\S+)\smalloc\((\d+)\)\s=>\s(0x\S+)/ ) {
	$site = $1;
	$size = $2;
	$ptr = $3;
	&malloc($site, $size, $ptr);
    } elsif ( $line =~ /^MEM:\s(0x\S+)\sfree\((0x[^\)]+)\)/ ) {
	$site = $1;
	$ptr = $2;
	&free($site, $ptr);
    } else {
	print STDERR $line;
    }
}

&finishgraphs();
&findmaxsites();
&findbadalloc();

exit 0;

sub malloc {
    my($site, $size, $ptr) = @_;
    my($addr) = eval($ptr);
    $isallocated{$ptr} = 1;
    $alloc2size{$ptr} = $size;
    $alloc2site{$ptr} = $site;
    $site2size{$site} += $size;
    if ( $site2size{$site} > $site2maxsize{$site} ) {
	$site2maxsize{$site} = $site2size{$site};
    }
    $totalsize += $size;
    $totalcount++;
    if ( $basemem == 0 ) {
	$basemem = $addr;
    }
    if ( $topofheap - $size < $addr ) {
	# Caused a heap growth
	$site2heapgrowthcount{$site}++;
	$site2heapgrowthsize{$site} += $size;
	$topofheap = $addr + $size;
    }
    &plot();
}

sub free {
    my($site, $ptr) = @_;
    if ( ! $isallocated{$ptr} ) {
	print STDERR "Freeing unallocated data at $ptr from $site\n";
	return;
    }
    $totalsize -= $alloc2size{$ptr};
    $totalcount--;
    $site2size{$alloc2site{$ptr}} -= $alloc2size{$ptr};
    $isallocated{$ptr} = 0;
    $alloc2size{$ptr} = 0;
    &plot();
}

sub realloc {
    my($site, $oldptr, $size, $ptr) = @_;
    &free($site, $oldptr);
    &malloc($site, $size, $ptr);
    &plot();
}

sub plot {
    if ( ++$plotiterator % 1000 == 0 ) {
	plotgraph(0, $allocnum, $topofheap - $basemem);
	plotgraph(1, $allocnum, $totalsize);
	plotgraph(2, $allocnum, $totalcount * 100);  # 10M => 100k objects
	# print $allocnum++, " ", eval($topofheap), "\n";

	foreach $site ( keys %site2graph ) {
	    &plotgraph($site2graph{$site}, $allocnum, $site2size{$site} + 0);
	}
    }
    $allocnum++;
}

sub initgraph {
    my($graphnum, $name) = @_;
    $graph[$graphnum] .= "\"$name\n";
}

sub plotgraph {
    my($graphnum, $x, $y) = @_;
    $graph[$graphnum] .= "$x $y\n";
}

sub finishgraphs {
    for ( $t = 0; $t < 100; $t++ ) {
	if ( $graph[$t] ne '' ) {
	    print $graph[$t], "\n";
	}
    }
}

sub findbadalloc {
    for ( $k = 0; $k < 15; $k++ ) {
	$foundmax = 0;
	foreach $site ( keys %site2heapgrowthsize ) {
	    if ( $site2heapgrowthsize{$site} > $foundmax ) {
		$foundsite = $site;
		$foundmax = $site2heapgrowthsize{$site};
	    }
	}
	print STDERR "HEAP: $foundsite grew $foundmax bytes in ";
	print STDERR $site2heapgrowthcount{$foundsite}, " allocs\n";
	$site2heapgrowthsize{$foundsite} = 0;
    }
}

sub findmaxsites {
    for ( $k = 0; $k < 15; $k++ ) {
	$foundmax = 0;
	foreach $site ( keys %site2maxsize ) {
	    if ( $site2maxsize{$site} > $foundmax ) {
		$foundsite = $site;
		$foundmax = $site2maxsize{$site};
	    }
	}
	print STDERR "SITE: $foundsite has $foundmax bytes maximum\n";
	$site2maxsize{$foundsite} = 0;
    }
}
