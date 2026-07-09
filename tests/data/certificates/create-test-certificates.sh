#!/bin/bash
# Copyright (C) 2021 The Qt Company Ltd.
# Copyright (C) 2019 Luxoft Sweden AB
# Copyright (C) 2018 Pelagicore AG
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#set -x
set -e

# check basic requirement
[ ! -e openssl-root-ca.cnf ] && { echo "Please cd to the tests/data/certificates directory before running this script"; exit 1; }

. ../utilities.sh

# By default, existing CA directories are preserved so that leaf certs can be
# re-issued against the same root/sub-CA. This keeps checked-in .p7 files
# (like signature-wincrypt.p7) valid when leaf certs are regenerated. Pass
# --force to wipe everything and rebuild from scratch (this WILL invalidate
# any signature files whose trust chain terminates at the old root CA).
FORCE=0
if [ "$1" = "--force" ]; then
    FORCE=1
fi

if [ $FORCE -eq 1 ]; then
    for i in root dev store other; do
        rm -rf $i-ca $i-certs
    done
fi

# Leaf certs are always regenerated - cheap, and the user typically edits a leaf
# cnf and re-runs the script. unique_subject=no in the CA cnfs allows re-signing.
for i in dev store other; do
    rm -rf $i-certs
done

echo "OpenSSL installation check:"

# cater for the most common settings in the CI
SSL_BIN_PATH=""
if [ -n "$OPENSSL_DIR" ]; then
  SSL_BIN_PATH="$OPENSSL_DIR/bin/"
elif [ -n "$OPENSSL_HOME" ]; then
  SSL_BIN_PATH="$OPENSSL_HOME/bin/"
fi

echo " * Using openssl at ${SSL_BIN_PATH:-`which openssl`}"

# try to execute and extract the major version number
SSL_VERSION=$(${SSL_BIN_PATH}openssl version 2>/dev/null | cut -d' ' -f2 || true)
if [ -z "$SSL_VERSION" ]; then
  echo -e "$R Failed$W to run or parse the output of$G openssl version$W".
  exit 1
fi

echo " * Version: ${SSL_VERSION}"
echo

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

# CA state is only initialized if the CA directory is absent - preserves
# root/sub-CAs and their index.txt/serial.txt across re-runs. Leaf certs are
# regenerated further below. index.txt.attr is rewritten every time because
# openssl caches unique_subject there and the value overrides the cnf.
for i in root dev store other; do
    if [ ! -d $i-ca ]; then
        mkdir -p $i-ca/new-certs
        touch $i-ca/index.txt
        echo '01' > $i-ca/serial.txt
        echo '01' > $i-ca/crlnumber.txt
    fi
    echo 'unique_subject = no' > $i-ca/index.txt.attr
done

for i in dev store other; do
    mkdir $i-certs
done

if [ ! -f root-ca/root-ca.crt ]; then
    info "Generating root CA"
    # the -days parameter is needed due to an openssl bug: having -x509 on the
    # command-line makes it ignore the the default_days option in the config file
    runSSL req -config openssl-root-ca.cnf -x509 -new -days 3650 -newkey rsa:2048 -nodes -keyout root-ca/root-ca-priv.key -out root-ca/root-ca.crt
fi

if [ ! -f dev-ca/dev-ca.crt ]; then
    info "Generating the developer sub-CA"
    runSSL req -config openssl-dev-ca.cnf -newkey rsa:2048 -nodes -keyout dev-ca/dev-ca-priv.key -out dev-ca/dev-ca.csr
    runSSL ca -batch -config openssl-root-ca.cnf -policy signing_policy -extensions root_ca_extensions -out dev-ca/dev-ca.crt -infiles dev-ca/dev-ca.csr
fi

if [ ! -f store-ca/store-ca.crt ]; then
    info "Generating the store sub-CA"
    runSSL req -config openssl-store-ca.cnf -newkey rsa:2048 -nodes -keyout store-ca/store-ca-priv.key -out store-ca/store-ca.csr
    runSSL ca -batch -config openssl-root-ca.cnf -policy signing_policy -extensions root_ca_extensions -out store-ca/store-ca.crt -infiles store-ca/store-ca.csr
fi

info "Generating, signing and exporting the developer certificate #1"
runSSL req -config openssl-dev-1.cnf -newkey rsa:2048 -nodes -keyout dev-certs/dev-1-priv.key -out dev-certs/dev-1.csr
runSSL ca -batch -config openssl-dev-ca.cnf -policy signing_policy -extensions signing_req -out dev-certs/dev-1.crt -infiles dev-certs/dev-1.csr
runSSL pkcs12 -export -out dev-certs/dev-1.p12 -password pass:password -inkey dev-certs/dev-1-priv.key -nodes -in dev-certs/dev-1.crt -name "Developer 1 Certificate"

info "Generating, signing and exporting the developer certificate #2"
runSSL req -config openssl-dev-2.cnf -newkey rsa:2048 -nodes -keyout dev-certs/dev-2-priv.key -out dev-certs/dev-2.csr
runSSL ca -batch -config openssl-dev-ca.cnf -policy signing_policy -extensions signing_req -out dev-certs/dev-2.crt -infiles dev-certs/dev-2.csr
runSSL pkcs12 -export -out dev-certs/dev-2.p12 -password pass:password -inkey dev-certs/dev-2-priv.key -nodes -in dev-certs/dev-2.crt -name "Developer 2 Certificate"

info "Generating, signing and exporting the \"narrow\" developer certificate"
runSSL req -config openssl-dev-narrow.cnf -newkey rsa:2048 -nodes -keyout dev-certs/dev-narrow-priv.key -out dev-certs/dev-narrow.csr
runSSL ca -batch -config openssl-dev-ca.cnf -policy signing_policy -extensions signing_req -out dev-certs/dev-narrow.crt -infiles dev-certs/dev-narrow.csr
runSSL pkcs12 -export -out dev-certs/dev-narrow.p12 -password pass:password -inkey dev-certs/dev-narrow-priv.key -nodes -in dev-certs/dev-narrow.crt -name "Narrow Developer Certificate"

info "Generating, signing and exporting the \"huge\" developer certificate"
runSSL req -config openssl-dev-huge.cnf -newkey rsa:2048 -nodes -keyout dev-certs/dev-huge-priv.key -out dev-certs/dev-huge.csr
runSSL ca -batch -config openssl-dev-ca.cnf -policy signing_policy -extensions signing_req -out dev-certs/dev-huge.crt -infiles dev-certs/dev-huge.csr
runSSL pkcs12 -export -out dev-certs/dev-huge.p12 -password pass:password -inkey dev-certs/dev-huge-priv.key -nodes -in dev-certs/dev-huge.crt -name "Huge Developer Certificate"

info "Generating, signing and exporting the \"legacy\" (pre-6.11) developer certificate"
runSSL req -config openssl-dev-legacy.cnf -newkey rsa:2048 -nodes -keyout dev-certs/dev-legacy-priv.key -out dev-certs/dev-legacy.csr
runSSL ca -batch -config openssl-dev-ca.cnf -policy signing_policy -extensions signing_req_legacy -out dev-certs/dev-legacy.crt -infiles dev-certs/dev-legacy.csr
runSSL pkcs12 -export -out dev-certs/dev-legacy.p12 -password pass:password -inkey dev-certs/dev-legacy-priv.key -nodes -in dev-certs/dev-legacy.crt -name "Legacy Developer Certificate"

info "Generating, signing and exporting the \"pkgid-only\" (6.11-era) developer certificate"
runSSL req -config openssl-dev-pkgidonly.cnf -newkey rsa:2048 -nodes -keyout dev-certs/dev-pkgidonly-priv.key -out dev-certs/dev-pkgidonly.csr
runSSL ca -batch -config openssl-dev-ca.cnf -policy signing_policy -extensions signing_req -out dev-certs/dev-pkgidonly.crt -infiles dev-certs/dev-pkgidonly.csr
runSSL pkcs12 -export -out dev-certs/dev-pkgidonly.p12 -password pass:password -inkey dev-certs/dev-pkgidonly-priv.key -nodes -in dev-certs/dev-pkgidonly.crt -name "Pkgid-only Developer Certificate"

info "Generating, signing and exporting the \"multiversion\" (malformed) developer certificate"
runSSL req -config openssl-dev-multiversion.cnf -newkey rsa:2048 -nodes -keyout dev-certs/dev-multiversion-priv.key -out dev-certs/dev-multiversion.csr
runSSL ca -batch -config openssl-dev-ca.cnf -policy signing_policy -extensions signing_req -out dev-certs/dev-multiversion.crt -infiles dev-certs/dev-multiversion.csr
runSSL pkcs12 -export -out dev-certs/dev-multiversion.p12 -password pass:password -inkey dev-certs/dev-multiversion-priv.key -nodes -in dev-certs/dev-multiversion.crt -name "Multiversion Developer Certificate"

info "Generating, signing and exporting the \"revoked\" developer certificate"
runSSL req -config openssl-dev-revoked.cnf -newkey rsa:2048 -nodes -keyout dev-certs/dev-revoked-priv.key -out dev-certs/dev-revoked.csr
runSSL ca -batch -config openssl-dev-ca.cnf -policy signing_policy -extensions signing_req -out dev-certs/dev-revoked.crt -infiles dev-certs/dev-revoked.csr
runSSL pkcs12 -export -out dev-certs/dev-revoked.p12 -password pass:password -inkey dev-certs/dev-revoked-priv.key -nodes -in dev-certs/dev-revoked.crt -name "Revoked Developer"
runSSL ca -config openssl-dev-ca.cnf -crl_reason keyCompromise -revoke dev-certs/dev-revoked.crt

info "Generating, signing and exporting the \"expired\" developer certificate"
runSSL req -config openssl-dev-expired.cnf -newkey rsa:2048 -nodes -keyout dev-certs/dev-expired-priv.key -out dev-certs/dev-expired.csr
runSSL ca -startdate 20250101000000Z -enddate 20250131235959Z -batch -config openssl-dev-ca.cnf -policy signing_policy -extensions signing_req -out dev-certs/dev-expired.crt -infiles dev-certs/dev-expired.csr
runSSL pkcs12 -export -out dev-certs/dev-expired.p12 -password pass:password -inkey dev-certs/dev-expired-priv.key -nodes -in dev-certs/dev-expired.crt -name "Expired Developer"

info "Generating, signing and exporting the store certificate"
runSSL req -config openssl-store.cnf -newkey rsa:2048 -nodes -keyout store-certs/store-priv.key -out store-certs/store.csr
runSSL ca -batch -config openssl-store-ca.cnf -policy signing_policy -extensions signing_req -out store-certs/store.crt -infiles store-certs/store.csr
runSSL pkcs12 -export -password pass:password -out store-certs/store.p12 -inkey store-certs/store-priv.key -nodes -in store-certs/store.crt -name "Pelagicore App Store"


if [ ! -f other-ca/other-ca.crt ]; then
    info "Generating the \"other\" CA"
    runSSL req -config openssl-other-ca.cnf -x509 -new -days 3650 -newkey rsa:2048 -nodes -keyout other-ca/other-ca-priv.key -out other-ca/other-ca.crt
fi

info "Generating signing and exporting the \"other\" certificate"
runSSL req -config openssl-other.cnf -newkey rsa:2048 -nodes -keyout other-certs/other-priv.key -out other-certs/other.csr
runSSL ca -batch -config openssl-other-ca.cnf -policy signing_policy -extensions signing_req -out other-certs/other.crt -infiles other-certs/other.csr
# this one includes the other-ca.crt root CA on purpose
runSSL pkcs12 -export -out other-certs/other.p12 -password pass:password -inkey other-certs/other-priv.key -nodes -certfile other-ca/other-ca.crt -in other-certs/other.crt -name "Other Certificate"

info "Exporting all CRLs"
for i in root dev store other; do
  runSSL ca -batch -config openssl-$i-ca.cnf -gencrl -crldays 365 -out $i-ca/$i-ca.crl
done

echo -e "$G All test certificates have been created successfully$W"
echo

exit 0
