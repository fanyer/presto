#!/bin/bash
#
# This script along with "openssl.cnf" file from this folder creates
# a chain of three certificates containing RSA 1024 keys:
#	root (root) - root CA certificate (self signed).
#	2.rsa.cert (2.rsa.key) - second level CA certificate (signed with root/root)
#	cert3 (3.rsa.key) - signature/encryption certificate (signed with 2.rsa.key/2.rsa.cert)
# All the private keys are encrypted with password "secret".
#
export CA_TOP=./testCA
export CA_PWD=secret

CONFIG_FILE=../tools/openssl.cnf
cd `dirname $0`/../keys

function pkcs12 {
    echo "* Convert all private keys pkcs12 format"
    for cert in `find . -name "*.cert.pem"`
    do
        name=`basename $cert .cert.pem`
        echo " $name"
        openssl pkcs12 -passin pass:$CA_PWD -passout pass:$CA_PWD -export -in $cert -inkey $name.key.pem -name $name -out $name.p12
    done
}

function verify {
    echo "* Test certificates"
    openssl verify -CAfile root.cert.pem 2.rsa.cert.pem
    for cert in `find . -name "*.cert.pem"`
    do
        openssl verify -CAfile root.cert.pem -untrusted 2.rsa.cert.pem $cert
    done
}

if [ $# -eq 0 ]
then
    echo "Usage:"
    echo "$0 new"
    echo "	Wipes all keys and certificates and starts again"
    echo "$0 add <name> <cert_title> <openssl_command>"
    echo "	creates a new key/cert called <name>, with the comment <cert_title>,"
    echo "	generated using <openssl_command>"
    echo "	Make sure the openssl_command produces a file called <name>.key.pem"
    exit;
fi

if [ "$1" = "new" ]
then
    echo "WARNING: THIS WILL WIPE ALL EXISTING KEYS AND CERTIFICATES!"
    read -p "Do you wish to continue? (y/[n]) " yn
    if [ "$yn" == "y" ]
    then
        echo "REALLY?! ALL WIDGETS SIGNED USING THESE KEYS AND CERTS WILL NEED TO BE RESIGNED!"
        read -p "I have made backups of these files and/or know what I'm doing. (y/[n]) " yn
        if [ "$yn" == "y" ]
        then
            echo "Continuing..."
        else
            exit
        fi
    else
        exit
    fi

    echo "* Remove old files"
    rm -rf "$CA_TOP" *.pem *.der *.p12 *.req

    echo "* Create CA folders structure"
    mkdir "$CA_TOP"
    mkdir "${CA_TOP}/certs"
    mkdir "${CA_TOP}/crl"
    mkdir "${CA_TOP}/newcerts"
    mkdir "${CA_TOP}/private"
    echo "01" > "$CA_TOP/serial"
    touch "$CA_TOP/index.txt"

    echo "* Create root key and certificate"
    export CERT_NAME="w3c-widgets-digsig-testsuite root certificate"
    openssl genrsa -out root.key.pem 2048
    openssl req -config $CONFIG_FILE -batch -new -x509 -days 7300 -key root.key.pem -out root.cert.pem

    echo "* Generate RSA key and second level certificate"
    export CERT_NAME="w3c-widgets-digsig-testsuite second level certificate"
    openssl genrsa -out 2.rsa.key.pem 2048
    openssl req -config $CONFIG_FILE -batch -new -key 2.rsa.key.pem -out 2.rsa.req
    openssl ca  -config $CONFIG_FILE -passin pass:$CA_PWD -batch -extensions v3_ca -cert root.cert.pem -keyfile root.key.pem -out 2.rsa.cert.pem -infiles 2.rsa.req

    echo "* Generate another RSA key and third level certificate"
    export CERT_NAME="w3c-widgets-digsig-testsuite sig and encryption certificate"
    openssl genrsa -out 3.rsa.key.pem 2048
    openssl req -config $CONFIG_FILE -batch -new -key 3.rsa.key.pem -out 3.rsa.req
    openssl ca  -config $CONFIG_FILE -passin pass:$CA_PWD -batch -cert 2.rsa.cert.pem -keyfile 2.rsa.key.pem -out 3.rsa.cert.pem -infiles 3.rsa.req

    echo "* Generate a DSA certificate"
    export CERT_NAME="w3c-widgets-digsig-testsuite dsa certificate"
    openssl dsaparam -genkey -out 3.dsa.key.pem 2048
    openssl req -config $CONFIG_FILE -batch -new -key 3.dsa.key.pem -out 3.dsa.req
    openssl ca  -config $CONFIG_FILE -passin pass:$CA_PWD -batch -cert 2.rsa.cert.pem -keyfile 2.rsa.key.pem -out 3.dsa.cert.pem -infiles 3.dsa.req

    echo "* Generate a certificate that is too short"
    export CERT_NAME="w3c-widgets-digsig-testsuite too short certificate"
    openssl genrsa -out 512.rsa.key.pem 512
    openssl req -config $CONFIG_FILE -batch -new -key 512.rsa.key.pem -out 512.rsa.req
    openssl ca  -config $CONFIG_FILE -passin pass:$CA_PWD -batch -cert 2.rsa.cert.pem -keyfile 2.rsa.key.pem -out 512.rsa.cert.pem -infiles 512.rsa.req

    echo "* Generate a ECDSA certificate"
    export CERT_NAME="w3c-widgets-digsig-testsuite ecdsa certificate"
    openssl ecparam -genkey -name prime256v1 -out ecdsa.key.pem
    openssl req -config $CONFIG_FILE -batch -new -key ecdsa.key.pem -out ecdsa.rsa.req
    openssl ca  -config $CONFIG_FILE -passin pass:$CA_PWD -batch -cert 2.rsa.cert.pem -keyfile 2.rsa.key.pem -out ecdsa.cert.pem -infiles ecdsa.req

    pkcs12
    verify

    echo "* Cleanup"
    rm *.req

    exit;
elif [ "$1" = "add" ]
then
    name=$2
    cert_title=$3
    openssl_command=$4

    if [ "$openssl_command" != *$name.key.pem* ]
    then
        echo "Warning: openssl_command does not contain '$name.key.pem'"
        read -p "Do you wish to continue? [n] " yn
        if [ "$yn" == "y" ]
        then
            echo "Continuing..."
        else
            exit
        fi
    fi

    export CERT_NAME="w3c-widgets-digsig-testsuite $cert_title"
    eval "$openssl_command"
    openssl req -config $CONFIG_FILE -batch -new -key $name.key.pem -out $name.req
    openssl ca  -config $CONFIG_FILE -passin pass:$CA_PWD -batch -cert 2.rsa.cert.pem -keyfile 2.rsa.key.pem -out $name.cert.pem -infiles $name.req

    pkcs12
    verify

    echo "* Cleanup"
    rm *.req
else
    echo "Invalid command"
    exit;
fi
