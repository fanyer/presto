#!/bin/bash

printtime=1
path=""
reps=5

path=""
list=""
help=0
exename="./fast_jsshell"

usage="Usage: "$(basename ${0})$" [options]\n\nOptions:\n\
 -h\t\tshow this help message and exit\n\
 -f FILE\tfile containing list of benchmarks to run\n\
 -p PATH\tpath to directory containing sunspider tests\n\
 -v n\t\tset verbosity level (0,2) [default n=1]\n\
 -x EXECUTABLE\tjssehll to use [default ./fast_jsshell]\
 "  

while [ $# -ge 1 ]; do
    case $1 in
    "-p")
            path=$2
            shift
            ;;
    "-f")
            list=$2
            shift
            ;;
    "-v")
            printtime=$2
            shift
            ;;
    "-x")
            exename=$2
            shift
            ;;
    "-h")
            help=1
            ;;
    *)
            echo 1>&2 Strange argument
            echo -e 1>&2 $usage
            exit 1
            ;;
    esac
    shift
done

# handle errors

if [ $help -eq 1 ]; then
    echo -e 1>&2 $usage
    exit 1
fi
if [ -z $path ]; then
    echo 1>&2 Path empty
    echo -e 1>&2 $usage
    exit 1
fi
if [ ! -d $path ]; then
    echo 1>&2 Path $path doesnt exist
    echo -e 1>&2 $usage
    exit 1
fi
if [ $printtime -lt 0 -o $printtime -gt 2 ]; then
    echo 1>&2 Wrong verbosity level $printtime
    echo -e 1>&2 $usage
    exit 1
fi
if [ ! -x $exename ]; then
    echo 1>&2 Executable $exename doesnt exist
    echo -e 1>&2 $usage
    exit 1
fi

res="http://qa/tools/sunspider/sunspider-results.html?%7B"
comma=0
if [ -z $list ]; then
    list=$path/*
else
    tmplist=$(cat $list)
    list=""
    for f in $tmplist; do
        list=${list}" "${path}"/"${f}
    done
fi

for i in $list; do
    if [[ $i == *.es || $i == *.js ]]; then
        if [ $comma -eq 0 ]; then
            comma=1
        else
            res=${res}","
        fi
        ctr=0
        testname=${i##*/}
        testname=${testname:0: ${#testname} - 3}
        if [ $printtime -gt 0 ]; then
            echo -n $testname:" "
        fi
        res=${res}"%22"$testname"%22:%5B"
        while [ $ctr -lt $reps ]; do
            t=$( $exename $i | grep "Execution time" | sed -e 's/[^0-9]*//' | sed -e 's/\..*//' )
            if [ $printtime -eq 1 ]; then
                echo -n ". "
            elif [ $printtime -eq 2 ]; then
                echo -n $t" "
            fi
            echo -n "${p}"
            res=${res}${t}
            ctr=$((ctr+1))
            if [ $ctr -lt $reps ]; then
                res=${res}","
            fi
        done
        if [ $printtime -gt 0 ]; then
            echo ""
        fi
        res=${res}"%5D"
    fi
done
if [ $printtime -gt 0 ]; then
    echo ""
    echo Sunspider url:
fi
res=${res}"%7D"
echo $res

