#!/bin/sh -v

rm -rf ca-data keys reqs certs crls
mkdir  ca-data keys reqs certs crls
mkdir  ca-data/certs

echo -n   >ca-data/index.txt
echo "01" >ca-data/serial
echo "01" >ca-data/crlnumber

openssl genrsa -out ca-data/cakey.pem 2048
openssl req -new -x509 -subj "/CN=Libopeay test CA/O=Opera/ST=Oslo/C=NO" -days 3650 -key ca-data/cakey.pem -sha1 -out ca-data/cacert.pem -text
