#!/bin/bash
# Copyright (C) 2021 The Qt Company Ltd.
# Copyright (C) 2019 Luxoft Sweden AB
# Copyright (C) 2018 Pelagicore AG
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

echo "Recreating test data"

certdir="../../data/certificates/"

if [ ! -f $certdir/dev1.p12 ]; then
  if [ -n "$BUILD_DIR" ] && [ -f "$BUILD_DIR/tests/data/certificates/dev1.p12" ]; then
    certdir="$BUILD_DIR/tests/data/certificates/"
  else
    echo "Please generate test certificates in $certdir first or set"
    echo "  \$BUILD_DIR to point to your build folder."
    exit 1
  fi
fi

cp $certdir/dev1.p12 signing.p12
openssl pkcs12 -export -out signing-no-key.p12 -password pass:password \
               -inkey $certdir/dev1-priv.key -certfile $certdir/ca.crt \
               -in $certdir/dev1.crt -name "Developer 1 Certificate (no key)" -nokeys
cat $certdir/ca.crt $certdir/devca.crt >verifying.crt
openssl x509 -in $certdir/dev1.crt -noout -fingerprint -sha1 -dates \
             -serial -issuer -subject >dev.info
openssl x509 -in $certdir/dev1.crt -noout -fingerprint -sha256 >>dev.info
openssl x509 -in $certdir/ca.crt -noout -fingerprint -sha1 -dates \
             -serial -issuer -subject >root-ca.info
openssl x509 -in $certdir/ca.crt -noout -fingerprint -sha256 >>root-ca.info
openssl x509 -in $certdir/devca.crt -noout -fingerprint -sha1 -dates \
             -serial -issuer -subject >dev-ca.info
openssl x509 -in $certdir/devca.crt -noout -fingerprint -sha256 >>dev-ca.info
openssl cms -sign -signer $certdir/dev1.crt -md sha1 -binary -outform DER \
            -in <(echo "p7Hash" | base64 - | tr -d '\n') \
            -inkey $certdir/dev1-priv.key -out signature-legacy-sha1.p7
