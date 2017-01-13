#!/bin/bash
#############################################################################
##
## Copyright (C) 2017 Pelagicore AG
## Contact: https://www.qt.io/licensing/
##
## This file is part of the Pelagicore Application Manager.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT-QTAS$
## Commercial License Usage
## Licensees holding valid commercial Qt Automotive Suite licenses may use
## this file in accordance with the commercial license agreement provided
## with the Software or, alternatively, in accordance with the terms
## contained in a written agreement between you and The Qt Company.  For
## licensing terms and conditions see https://www.qt.io/terms-conditions.
## For further information use the contact form at https://www.qt.io/contact-us.
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

runSSL()
{
  sslOutput=`openssl "$@" 2>&1`
  sslResult=$?
  if [ $sslResult -ne 0 ]; then
    echo -e "The openssl$R failed with exit code $sslResult$W. The executed command was:"
    echo
    echo -e "   $G openssl $@$W"
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
runSSL pkcs12 -export -password pass:password -out store.p12 -inkey store-priv.key -nodes -in store.crt -name "Pelagicore App Store"

info "Generating the developer sub-CA"
runSSL req -config openssl-devca.cnf -newkey rsa:2048 -nodes -keyout devca-priv.key -out devca.csr
runSSL ca -batch -config openssl-ca.cnf -policy signing_policy -extensions ca_extensions -out devca.crt -infiles devca.csr
touch dev-index.txt
echo '01' > dev-serial.txt

info "Generating, signing and exporting the developer certificate #1"
runSSL req -config openssl-dev1.cnf -newkey rsa:2048 -nodes -keyout dev1-priv.key -out dev1.csr
runSSL ca -batch -config openssl-devca.cnf -policy signing_policy -extensions signing_req -out dev1.crt -infiles dev1.csr
runSSL pkcs12 -export -out dev1.p12 -password pass:password -inkey dev1-priv.key -nodes -certfile devca.crt -in dev1.crt -name "Developer 1 Certificate"

info "Generating, signing and exporting the developer certificate #2"
runSSL req -config openssl-dev2.cnf -newkey rsa:2048 -nodes -keyout dev2-priv.key -out dev2.csr
runSSL ca -batch -config openssl-devca.cnf -policy signing_policy -extensions signing_req -out dev2.crt -infiles dev2.csr
runSSL pkcs12 -export -out dev2.p12 -password pass:password -inkey dev2-priv.key -nodes -certfile devca.crt -in dev2.crt -name "Developer 2 Certificate"

info "Generating the \"other\" CA"
# generate self-signed CA cert
# the -days parameter is needed due to an openssl bug: having -x509 on the
# command-line makes it ignore the the default_days option in the config
runSSL req -config openssl-other-ca.cnf -x509 -days 3650 -new -newkey rsa:2048 -nodes -keyout other-ca-priv.key -out other-ca.crt
touch other-index.txt
echo '01' > other-serial.txt
runSSL req -batch -subj '/C=DE/ST=Foo/L=Bar/CN=www.other.com' -newkey rsa:2048 -nodes -keyout other-priv.key -out other.csr
runSSL ca -batch -config openssl-other-ca.cnf -policy signing_policy -extensions signing_req -out other.crt -infiles other.csr
runSSL pkcs12 -export -out other.p12 -password pass:password -inkey other-priv.key -nodes -certfile other-ca.crt -in other.crt -name "Other Certificate"

echo -e "$G All test certificated have been created successfully$W"
echo

exit 0
