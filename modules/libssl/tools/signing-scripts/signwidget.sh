#!/bin/bash
have_args=
while getopts 'r:x:p:h' OPTION
do
  case $OPTION in
  r)	rsakey="$OPTARG" ;
  	have_args=1;  
  		;;
  p)	rsakeypass="$OPTARG" 
  	have_args=1;  
		;;
  x)	x509cert="$OPTARG"
    	have_args=1;
		;;
  h|?|*) have_args=0;        
         break;
		;;
  esac
done

shift $(($OPTIND - 1))

wdigetdir="$*"

if [ ${#wdigetdir} = 0 ]
then
	have_args=0;    
fi


if [ "$have_args" != 1 ]   
then
	echo "usage: signwidget -r <rsakey> -p <rsakey password> -x <x509cert> <widgetpath>" ; 
	echo "rsakey and x509cert must be in pem format";
	exit 1;
fi

slash="/"

if [ "$slash" = ${wdigetdir:0:1} ]
then

	widgetdir="$*"
else
	widgetdir="$PWD"/"$*"	
fi


rm "$widgetdir"/signature.xml
old_pwd="$PWD"
cd "$widgetdir"

echo Files to sign in "$widgetdir":

find . -type f -name "*"
echo '<?xml version="1.0" encoding="UTF-8"?>'> /tmp/signature_template.xml

echo '<Signature xmlns="http://www.w3.org/2000/09/xmldsig#">' >> /tmp/signature_template.xml
echo '    <SignedInfo>' >> /tmp/signature_template.xml
echo '        <CanonicalizationMethod'  >> /tmp/signature_template.xml
echo '            Algorithm="http://www.w3.org/TR/2001/REC-xml-c14n-20010315"/>'>> /tmp/signature_template.xml
echo '        <SignatureMethod'  >> /tmp/signature_template.xml
echo '            Algorithm="http://www.w3.org/2000/09/xmldsig#rsa-sha1" />'>> /tmp/signature_template.xml
 
for file in ` find . -type f -name "*"` ; do
	len=${#file}
	echo '        <Reference URI="'${file:2:len}'">' >> /tmp/signature_template.xml
	echo '            <DigestMethod Algorithm="http://www.w3.org/2000/09/xmldsig#sha1"/>' >> /tmp/signature_template.xml
	echo '            <DigestValue> </DigestValue>' >> /tmp/signature_template.xml
	echo '        </Reference>' >> /tmp/signature_template.xml
done

echo '    </SignedInfo>' >> /tmp/signature_template.xml
echo '    <SignatureValue> </SignatureValue>' >> /tmp/signature_template.xml
echo '</Signature>' >> /tmp/signature_template.xml 
cd "$old_pwd"

if [ -d "$widgetdir" ]
then 
	cp /tmp/signature_template.xml "$widgetdir"/__signature_template.xml
	templatesign -r "$rsakey" "$rsakeypass"  -x "$x509cert" "$widgetdir"/__signature_template.xml > "$widgetdir"/signature.xml
	rm  -f "$widgetdir"/__signature_template.xml
else
    echo "$widgetdir" : No such directory
fi        

echo done
