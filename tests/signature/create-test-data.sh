#!/bin/sh
#############################################################################
##
## Copyright (C) 2015 Pelagicore AG
## Contact: http://www.pelagicore.com/
##
## This file is part of the Pelagicore Application Manager.
##
## SPDX-License-Identifier: GPL-3.0
##
## $QT_BEGIN_LICENSE:GPL3$
## Commercial License Usage
## Licensees holding valid commercial Pelagicore Application Manager
## licenses may use this file in accordance with the commercial license
## agreement provided with the Software or, alternatively, in accordance
## with the terms contained in a written agreement between you and
## Pelagicore. For licensing terms and conditions, contact us at:
## http://www.pelagicore.com.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation and appearing in the file LICENSE.GPLv3 included in the
## packaging of this file. Please review the following information to
## ensure the GNU General Public License version 3 requirements will be
## met: http://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

echo "Recreating test data"

certdir="../data/certificates/"

[ -f $certdir/dev1.p12 ] || { echo "Please generate test certificates in $certdir first"; exit 1; }

cp $certdir/dev1.p12 signing.p12
openssl pkcs12 -export -out signing-no-key.p12 -password pass:password -inkey $certdir/dev1-priv.key -nodes -certfile $certdir/ca.crt -in $certdir/dev1.crt -name "Developer 1 Certificate (no key)" -nokeys
cat $certdir/ca.crt $certdir/devca.crt >verifying.crt
