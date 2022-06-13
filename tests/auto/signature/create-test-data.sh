#!/bin/sh
#############################################################################
##
## Copyright (C) 2021 The Qt Company Ltd.
## Copyright (C) 2019 Luxoft Sweden AB
## Copyright (C) 2018 Pelagicore AG
## Contact: https://www.qt.io/licensing/
##
## This file is part of the QtApplicationManager module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

echo "Recreating test data"

certdir="../data/certificates/"

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
openssl pkcs12 -export -out signing-no-key.p12 -password pass:password -inkey $certdir/dev1-priv.key -nodes -certfile $certdir/ca.crt -in $certdir/dev1.crt -name "Developer 1 Certificate (no key)" -nokeys
cat $certdir/ca.crt $certdir/devca.crt >verifying.crt
