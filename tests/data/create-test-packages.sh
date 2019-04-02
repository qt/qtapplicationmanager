#!/bin/bash
#############################################################################
##
## Copyright (C) 2019 Luxoft Sweden AB
## Copyright (C) 2018 Pelagicore AG
## Contact: https://www.qt.io/licensing/
##
## This file is part of the Luxoft Application Manager.
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
set -e

isWin=0
isMac=0
[ "$OS" == "Windows_NT" ] && isWin=1
[ "$(uname)" == "Darwin" ] && isMac=1

# check basic requirement
[ ! -d certificates ] && { echo "Please cd to the tests/data directory before running this script"; exit 1; }

# having $LC_ALL set to "C" will screw us big time - especially since QtCreator sets this
# unconditionally in the build environment, overriding a potentially valid $LANG setting.
[ "$LC_ALL" = "C" ] && { echo "WARNING: unsetting \$LC_ALL, since it is set to \"C\" (most likely by a wrapper script or QtCreator)"; unset LC_ALL; }

# let the c-library resolve all the indirect settings
eval "x$(locale | grep LC_CTYPE= | head -n1)"

# now check for character encoding
[ "${xLC_CTYPE%%UTF-8}" = "$xLC_CTYPE" ] && { echo "The appman-packager needs to be run within an UTF-8 locale variant"; exit 1; }

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
tmp=$(mktemp -d "${TMPDIR:-/tmp/}$(basename $0).XXXXXXXXXXXX")
src="$tmp/source"

removeTmp() { rm -rf "$tmp"; }
trap removeTmp INT QUIT 0

mkdir -p "$dst"
mkdir -p "$src"
[ "$isWin" = "1" ] && src=$(cygpath -m "$src")

packager()
{
  set +e
  packagerOutput=`"$PACKAGER" "$@" 2>&1`
  packagerResult=$?
  set -e
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

cp info.yaml "$src"
cp icon.png "$src"
echo "test" >"$src/test"
if [ "$isMac" = "1" ]; then
  # macOS shells create filenames with unicode characters in pre-composed UTF form, which is
  # non-standard on macOS. Qt's internal QFileSystemIterator class on the other hand is ignoring
  # these filenames as being invalid. A workaround is to convert the name to de-composed form
  # already in the shell environment:
  echo "test with umlaut" >"$src/$(iconv -f utf-8 -t utf-8-mac <<< täst)"
else
  echo "test with umlaut" >"$src/täst"
fi

info "Create package"
packager create-package "$dst/test.appkg" "$src"

info "Dev-sign package"
packager dev-sign-package "$dst/test.appkg" "$dst/test-dev-signed.appkg" certificates/dev1.p12 password

info "Store-sign package"
packager store-sign-package "$dst/test.appkg" "$dst/test-store-signed.appkg" certificates/store.p12 password "foobar"

info "Store-sign dev package"
packager store-sign-package "$dst/test-dev-signed.appkg" "$dst/test-store-dev-signed.appkg" certificates/store.p12 password "foobar"

info "Create package with extra meta-data"
cat >"$tmp/exmd" <<EOT
array:
- 1
- 2
EOT
cat >"$tmp/exmd2" <<EOT
key: value
EOT
cat >"$tmp/exmds" <<EOT
signed-object:
  k1: v1
  k2: v2
EOT
cat >"$tmp/exmds2" <<EOT
signed-key: signed-value
EOT

packager create-package "$dst/test-extra.appkg" "$src" \
  -m '{ "foo": "bar" }' -m '{ "foo2": "bar2" }' -M "$src/../exmd"  -M "$src/../exmd2" \
  -s '{ "sfoo": "sbar" }' -s '{ "sfoo2": "sbar2" }' -S "$src/../exmds" -S "$src/../exmds2"

info "Dev-sign package with extra meta-data"
packager dev-sign-package "$dst/test-extra.appkg" "$dst/test-extra-dev-signed.appkg" certificates/dev1.p12 password

info "Create a hello-world.red update package"
packager create-package "$dst/hello-world.red.appkg" hello-world.red

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
sed <"$src/--PACKAGE-HEADER--.orig" >"$src/--PACKAGE-HEADER--" 's/formatVersion: 2/formatVersion: X/'
tar -C "$src" -cf "$dst/test-invalid-header-formatversion.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-HEADER--{.orig,}

info "Create a package with a 0 diskSpaceUsed header field"
mv "$src"/--PACKAGE-HEADER--{,.orig}
sed <"$src/--PACKAGE-HEADER--.orig" >"$src/--PACKAGE-HEADER--" 's/diskSpaceUsed: [0-9]*/diskSpaceUsed: 0/'
tar -C "$src" -cf "$dst/test-invalid-header-diskspaceused.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-HEADER--{.orig,}

info "Create a package with an invalid id header field"
mv "$src"/--PACKAGE-HEADER--{,.orig}
sed <"$src/--PACKAGE-HEADER--.orig" >"$src/--PACKAGE-HEADER--" "s/packageId: '[a-z0-9.-]*'/packageId: ':invalid'/"
tar -C "$src" -cf "$dst/test-invalid-header-id.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-HEADER--{.orig,}

info "Create a package with a non-matching id header field"
mv "$src"/--PACKAGE-HEADER--{,.orig}
sed <"$src/--PACKAGE-HEADER--.orig" >"$src/--PACKAGE-HEADER--" "s/packageId: '[a-z0-9.-]*'/packageId: 'non-matching'/"
tar -C "$src" -cf "$dst/test-non-matching-header-id.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-HEADER--{.orig,}

info "Create a package with a tampered extraSigned header field"
mv "$src"/--PACKAGE-HEADER--{,.orig}
( cat "$src/--PACKAGE-HEADER--.orig" ; echo "extraSigned: { foo: bar }") >"$src/--PACKAGE-HEADER--"
tar -C "$src" -cf "$dst/test-tampered-extra-signed-header.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-HEADER--{.orig,}

info "Create a package with an invalid info.yaml id"
mv "$src"/info.yaml{,.orig}
sed <"$src/info.yaml.orig" >"$src/info.yaml" 's/id: "[a-z0-9.-]*"/id: ":invalid"/'
tar -C "$src" -cf "$dst/test-invalid-info-id.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/info.yaml{.orig,}

info "Create a package with an invalid info.yaml file"
mv "$src"/info.yaml{,.orig}
sed <"$src/info.yaml.orig" >"$src/info.yaml" 's/code: "test"/: "test"/'
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
touch "$src/../invalid-path"
tar -C "$src" -P -cf "$dst/test-invalid-path.appkg" -- --PACKAGE-HEADER-- info.yaml icon.png ../invalid-path test --PACKAGE-FOOTER--

echo -e "$G All test packages have been created successfully$W"
echo

exit 0
