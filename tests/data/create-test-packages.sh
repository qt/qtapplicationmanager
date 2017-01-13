#!/bin/bash
#############################################################################
##
## Copyright (C) 2016 Pelagicore AG
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

#set -x

# check basic requirement
[ "$(uname)" != "Darwin" ] && [ "${LANG%%.UTF-8}" = "$LANG" ] && ( echo "The application-packager needs to be run with UTF-8 locale variant"; exit 1; )
[ ! -d certificates ] && ( echo "Please cd to the tests/data directory before running this script"; exit 1; )

# having $LC_ALL set to "C" will screw us big time - especially since QtCreator sets this
# unconditionally in the build environment, overriding a potentially valid $LANG setting.
[ "$LC_ALL" = "C" ] && { echo "WARNING: unsetting \$LC_ALL, since it is set to \"C\" (most likely by a wrapper script or QtCreator)"; unset LC_ALL; }

. utilities.sh

usage()
{
     echo "create-test-packages.sh <appman-packager binary>"
     exit 1
}

[ "$#" -lt 1 ] && usage
PACKAGER="${@: -1}"
[ ! -x "$PACKAGER" ] && usage
eval ${@:1:$# - 1}
"$PACKAGER" --version 2>/dev/null | grep -qsi "Packager" || usage

( cd certificates && ./create-test-certificates.sh )

dst="packages"
src="source-tmp.$$"

removeSrc() { rm -rf "$src"; }
trap removeSrc INT QUIT 0
removeSrc
mkdir "$src"

packager()
{
  packagerOutput=`"$PACKAGER" "$@" 2>&1`
  packagerResult=$?
  if [ $packagerResult -ne 0 ]; then
    echo -e "`basename $PACKAGER`$R failed with exit code $packagerResult$W. The executed command was:"
    echo
    echo -e "   $G $PACKAGER $@$W"
    echo
    echo "The command's output was:"
    echo
    echo "$packagerOutput"
    echo
    exit $packagerResult
  fi
}

echo "Generating test packages:"

### normal packages

mkdir -p "$dst"
cp info.yaml "$src"
cp icon.png "$src"
echo "test" >"$src/test"
echo "test with umlaut" >"$src/täst"

info "Create package"
packager create-package "$dst/test.appkg" "$src"

info "Dev-sign package"
packager dev-sign-package "$dst/test.appkg" "$dst/test-dev-signed.appkg" certificates/dev1.p12 password

### v2 packages for testing updates

echo "test update" >"$src/test"

info "Create update package"
packager create-package "$dst/test-update.appkg" "$src"

info "Dev-sign update package"
packager dev-sign-package "$dst/test-update.appkg" "$dst/test-update-dev-signed.appkg" certificates/dev2.p12 password

echo "test" >"$src/test"

###  big packages

cp info-big.yaml "$src/info.yaml"
dd if=/dev/zero of="$src/bigtest" bs=1048576 count=5 >/dev/null 2>&1

info "Create big package"
packager create-package "$dst/bigtest.appkg" "$src"

info "Dev-sign big package"
packager dev-sign-package "$dst/bigtest.appkg" "$dst/bigtest-dev-signed.appkg" certificates/dev1.p12 password

cp info.yaml "$src"
rm "$src/bigtest"

### create invalid packages

tar -C "$src" -xof "$dst/test.appkg" -- --PACKAGE-HEADER-- --PACKAGE-FOOTER--

info "Create a package with invalid format"
echo "invalid" >"$dst/test-invalid-format.appkg"

info "Create a package with an invalid formatVersion header field"
mv "$src"/--PACKAGE-HEADER--{,.orig}
echo '{formatType: "am-package-header", formatVersion: 2}' >$src/--PACKAGE-HEADER--
tar -C "$src" -cf "$dst/test-invalid-header-formatversion.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-HEADER--{.orig,}

info "Create a package with a 0 diskSpaceUsed header field"
mv "$src"/--PACKAGE-HEADER--{,.orig}
sed <"$src/--PACKAGE-HEADER--.orig" >"$src/--PACKAGE-HEADER--" 's/diskSpaceUsed: [0-9]*/diskSpaceUsed: 0/'
tar -C "$src" -cf "$dst/test-invalid-header-diskspaceused.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-HEADER--{.orig,}

info "Create a package with an invalid id header field"
mv "$src"/--PACKAGE-HEADER--{,.orig}
sed <"$src/--PACKAGE-HEADER--.orig" >"$src/--PACKAGE-HEADER--" "s/applicationId: '[a-z0-9.-]*'/applicationId: 'invalid'/"
tar -C "$src" -cf "$dst/test-invalid-header-id.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-HEADER--{.orig,}

info "Create a package with an invalid info.yaml id"
mv "$src"/info.yaml{,.orig}
sed <"$src/info.yaml.orig" >"$src/info.yaml" 's/id: "[a-z0-9.-]*"/id: "invalid"/'
tar -C "$src" -cf "$dst/test-invalid-info-id.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/info.yaml{.orig,}

info "Create a package with an invalid info.yaml file"
mv "$src"/info.yaml{,.orig}
sed <"$src/info.yaml.orig" >"$src/info.yaml" 's/code: "test"$/: "test"/'
tar -C "$src" -cf "$dst/test-invalid-info.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/info.yaml{.orig,}

info "Create a package with an invalid file order"
tar -C "$src" -cf "$dst/test-invalid-file-order.appkg" -- --PACKAGE-HEADER-- info.yaml test icon.png --PACKAGE-FOOTER--

info "Create a package with an invalid digest"
mv "$src"/--PACKAGE-FOOTER--{,.orig}
tr <"$src/--PACKAGE-FOOTER--.orig" >"$src/--PACKAGE-FOOTER--" 3 0
tar -C "$src" -cf "$dst/test-invalid-footer-digest.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-FOOTER--{.orig,}

info "Create a package with an invalid signature"
packager dev-sign-package "$dst/test.appkg" "$dst/test-invalid-footer-signature.appkg" certificates/other.p12 password

info "Create a package with an invalid entry path"
tar -C "$src" -P -cf "$dst/test-invalid-path.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png ../create-test-packages.sh test --PACKAGE-FOOTER--

echo -e "$G All test packages have been created successfully$W"
echo

exit 0
