#!/bin/sh

if [ $# -lt 2 ]
then
    echo "Usage: $0 assertionID testName [-u]"
    echo "  -u  update signatures"
    exit;
fi

ID=w3c-testsuite-id
ID=$ID-$1-$2

toolsdir=$(readlink -f `dirname $0`)
suitedir=`dirname $toolsdir`

casedir="$suitedir/test-cases"
testdir="$casedir/$1/$2"

if [ ! -d "$testdir" ]
then
    echo "Error: No directory $testdir"
    exit
fi

echo "* Removing widget..."
cd $testdir
rm $2.wgt

if [ "$3" = "-u" ]
then
    echo "* Updating widget signatures..."
    for i in $(find . -name "author-signature.xml")
    do
        $toolsdir/sign-widget.sh --pkcs12 "$suitedir/keys/3.rsa.p12" --pwd secret -x -u -a -o $i $testdir
    done
    for i in $(find . -name 'signature*.xml')
    do
        $toolsdir/sign-widget.sh --pkcs12 "$suitedir/keys/3.rsa.p12" --pwd secret -x -u -o $i $testdir
    done
    echo
fi

echo -n "* Validating... "
validatecmd="$toolsdir/validate-widget.sh --trusted-pem $suitedir/keys/root.cert.pem --pwd secret $testdir"
$validatecmd  > /dev/null 2> /dev/null

if [ "$?" -ne 0 ]
then
    echo "FAILED with command:"
    echo "$validatecmd"
    echo
else
    echo "SUCCESS with command:"
    echo "$validatecmd"
    echo
fi

echo "* Zipping widget..."
zip -r $2.wgt `ls | grep -v CVS`
cd -
echo

echo "Done."
