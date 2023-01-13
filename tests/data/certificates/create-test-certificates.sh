#!/bin/bash
# Copyright (C) 2021 The Qt Company Ltd.
# Copyright (C) 2019 Luxoft Sweden AB
# Copyright (C) 2018 Pelagicore AG
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#set -x
set -e

# check basic requirement
[ ! -e openssl-ca.cnf ] && { echo "Please cd to the tests/data/certificates directory before running this script"; exit 1; }

. ../utilities.sh

rm -f index.txt* serial.txt*
rm -f dev-index.txt* dev-serial.txt*
rm -f ca-priv.key ca.crt
rm -f store.csr store.crt store-priv.key store.p12
rm -f devca.csr devca.crt devca-priv.key
rm -f dev1.csr dev1.crt dev1-priv.key dev1.p12
rm -f dev2.csr dev2.crt dev2-priv.key dev2.p12
rm -f 01.pem 02.pem
rm -f other-index.txt* other-serial.txt*
rm -f other-ca-priv.key other-ca.crt
rm -f other.csr other.crt other-priv.key other.p12


echo "OpenSSL installation check:"

# cater for the most common settings in the CI
SSL_BIN_PATH=""
if [ -n "$OPENSSL_DIR" ]; then
  SSL_BIN_PATH="$OPENSSL_DIR/bin/"
elif [ -n "$OPENSSL_HOME" ]; then
  SSL_BIN_PATH="$OPENSSL_HOME/bin/"
fi

echo " * using openssl at ${SSL_BIN_PATH:-`which openssl`}"

# try to execute and extract the major version number
SSL_VERSION=$(${SSL_BIN_PATH}openssl version 2>/dev/null || true)
SSL_MAJOR=$(echo "$SSL_VERSION" | cut -d' ' -f 2 | cut -d'.' -f 1)
if [ -z "$SSL_VERSION" ] || [ -z "$SSL_MAJOR" ] || ! [ "$SSL_MAJOR" -eq "$SSL_MAJOR" ] 2>/dev/null; then
  echo -e "$R Failed$W to run or parse the output of$G openssl version$W".
  exit 1
fi

echo " * version $SSL_MAJOR (${SSL_VERSION})"
echo

SSL_PKCS12_EXTRA=''
if [ "${SSL_MAJOR}" -ge 3 ]; then
  # if we don't do this, then macOS' SecurityFramework cannot load the PKCS#12 files
  if [ "$isMac" = "1" ]; then
    SSL_PKCS12_EXTRA="-legacy"
    echo " * using -legacy mode for PKCS#12 files for SecurityFramework compatibility"
  fi
fi

runSSL()
{
  set +e
  sslOutput=`${SSL_BIN_PATH}openssl "$@" 2>&1`
  sslResult=$?
  set -e
  if [ $sslResult -ne 0 ]; then
    echo -e "Running openssl $R failed with exit code $sslResult$W. The executed command was:"
    echo
    echo -e "   $G ${SSL_BIN_PATH}openssl $@$W"
    echo
    echo "The command's output was:"
    echo
    echo "$sslOutput"
    echo
    exit $sslResult
  fi
}

echo "Generating test certificates:"

info "Generating root CA"
# generate self-signed CA cert
# the -days parameter is needed due to an openssl bug: having -x509 on the
# command-line makes it ignore the the default_days option in the config file
runSSL req -config openssl-ca.cnf -days 3650 -x509 -new  -newkey rsa:2048 -nodes -keyout ca-priv.key -out ca.crt
touch index.txt
echo '01' > serial.txt

info "Generating, signing and exporting the store certificate"
runSSL req -config openssl-store.cnf -newkey rsa:2048 -nodes -keyout store-priv.key -out store.csr
runSSL ca -batch -config openssl-ca.cnf -policy signing_policy -extensions signing_req -out store.crt -infiles store.csr
runSSL pkcs12 ${SSL_PKCS12_EXTRA} -export -password pass:password -out store.p12 -inkey store-priv.key -nodes -in store.crt -name "Pelagicore App Store"

info "Generating the developer sub-CA"
runSSL req -config openssl-devca.cnf -newkey rsa:2048 -nodes -keyout devca-priv.key -out devca.csr
runSSL ca -batch -config openssl-ca.cnf -policy signing_policy -extensions ca_extensions -out devca.crt -infiles devca.csr
touch dev-index.txt
echo '01' > dev-serial.txt

info "Generating, signing and exporting the developer certificate #1"
runSSL req -config openssl-dev1.cnf -newkey rsa:2048 -nodes -keyout dev1-priv.key -out dev1.csr
runSSL ca -batch -config openssl-devca.cnf -policy signing_policy -extensions signing_req -out dev1.crt -infiles dev1.csr
runSSL pkcs12 ${SSL_PKCS12_EXTRA} -export -out dev1.p12 -password pass:password -inkey dev1-priv.key -nodes -certfile devca.crt -in dev1.crt -name "Developer 1 Certificate"

info "Generating, signing and exporting the developer certificate #2"
runSSL req -config openssl-dev2.cnf -newkey rsa:2048 -nodes -keyout dev2-priv.key -out dev2.csr
runSSL ca -batch -config openssl-devca.cnf -policy signing_policy -extensions signing_req -out dev2.crt -infiles dev2.csr
runSSL pkcs12 ${SSL_PKCS12_EXTRA} -export -out dev2.p12 -password pass:password -inkey dev2-priv.key -nodes -certfile devca.crt -in dev2.crt -name "Developer 2 Certificate"

info "Generating the \"other\" CA"
# generate self-signed CA cert
# the -days parameter is needed due to an openssl bug: having -x509 on the
# command-line makes it ignore the the default_days option in the config
runSSL req -config openssl-other-ca.cnf -x509 -days 3650 -new -newkey rsa:2048 -nodes -keyout other-ca-priv.key -out other-ca.crt
touch other-index.txt
echo '01' > other-serial.txt
# the double // is needed to get around MSYS hardwired path replacement
runSSL req -config openssl-other-ca.cnf -batch -subj '//C=DE/ST=Foo/L=Bar/CN=www.other.com' -newkey rsa:2048 -nodes -keyout other-priv.key -out other.csr
runSSL ca -batch -config openssl-other-ca.cnf -policy signing_policy -extensions signing_req -out other.crt -infiles other.csr
runSSL pkcs12 ${SSL_PKCS12_EXTRA} -export -out other.p12 -password pass:password -inkey other-priv.key -nodes -certfile other-ca.crt -in other.crt -name "Other Certificate"

echo -e "$G All test certificates have been created successfully$W"
echo

exit 0
