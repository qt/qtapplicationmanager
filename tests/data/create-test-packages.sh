#!/bin/bash
# Copyright (C) 2021 The Qt Company Ltd.
# Copyright (C) 2019 Luxoft Sweden AB
# Copyright (C) 2018 Pelagicore AG
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#set -x
set -e

# OpenSSL as used by the packager might leak, but we aren't interested
export ASAN_OPTIONS="exitcode=0:detect_leaks=0"

# check basic requirement
[ ! -d certificates ] && { echo "Please cd to the tests/data directory before running this script"; exit 1; }

. ./utilities.sh

# set a well-known UTF-8 locale: C.UTF-8 is the obvious choice, but macOS doesn't support it
if [ "$isMac" = "1" ]; then
  export LC_ALL=en_US.UTF-8
else
  export LC_ALL=C.UTF-8
fi

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
tmp=$(mktemp -d "${TMPDIR:-/tmp}/$(basename $0).XXXXXXXXXXXX")
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

echo "Packager check:"
info "Using: $PACKAGER"
echo

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
packager create-package "$dst/test.ampkg" "$src"

info "Dev-sign package"
packager dev-sign-package --verbose "$dst/test.ampkg" "$dst/test-dev-signed.ampkg" certificates/dev-certs/dev-1.p12 password

info "Dev-verify package"
packager dev-verify-package --verbose "$dst/test-dev-signed.ampkg" certificates/dev-ca/dev-ca.crt certificates/root-ca/root-ca.crt

info "Store-sign package"
packager store-sign-package --verbose "$dst/test.ampkg" "$dst/test-store-signed.ampkg" certificates/store-certs/store.p12 password "foobar"

info "Store-verify package"
packager store-verify-package --verbose "$dst/test-store-signed.ampkg" certificates/store-ca/store-ca.crt certificates/root-ca/root-ca.crt "foobar"

info "Store-sign dev package"
packager store-sign-package --verbose "$dst/test-dev-signed.ampkg" "$dst/test-store-dev-signed.ampkg" certificates/store-certs/store.p12 password "foobar"

info "Store-verify dev package"
packager store-verify-package --verbose "$dst/test-store-dev-signed.ampkg" certificates/store-ca/store-ca.crt certificates/root-ca/root-ca.crt "foobar"

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

packager create-package "$dst/test-extra.ampkg" "$src" \
  -m '{ "foo": "bar" }' -m '{ "foo2": "bar2" }' -M "$src/../exmd"  -M "$src/../exmd2" \
  -s '{ "sfoo": "sbar" }' -s '{ "sfoo2": "sbar2" }' -S "$src/../exmds" -S "$src/../exmds2"

info "Dev-sign package with extra meta-data"
packager dev-sign-package --verbose "$dst/test-extra.ampkg" "$dst/test-extra-dev-signed.ampkg" certificates/dev-certs/dev-1.p12 password

### v2 packages for testing updates

echo "test update" >"$src/test"
sed <info.yaml >"$src/info.yaml" 's/version: "1.0"/version: "2.0"/'

info "Create update package"
packager create-package "$dst/test-update.ampkg" "$src"

info "Dev-sign update package"
packager dev-sign-package --verbose "$dst/test-update.ampkg" "$dst/test-update-dev-signed.ampkg" certificates/dev-certs/dev-2.p12 password

echo "test" >"$src/test"
cp "info.yaml" "$src"

### "other" packages

cp "info-other.yaml" "$src/info.yaml"
rm "$src/test"
echo "other" >"$src/other"

info "Create other package"
packager create-package "$dst/other-test.ampkg" "$src"

info "Dev-sign other package"
packager dev-sign-package --verbose "$dst/other-test.ampkg" "$dst/other-test-dev-signed.ampkg" certificates/dev-certs/dev-1.p12 password

rm "$src/other"
echo "test" >"$src/test"
cp "info.yaml" "$src"

### development-mode "application" overreach packages
# These are all correctly signed with dev-1 (which has wildcard restrictions),
# but each one declares a capability resp. category that the "narrow" developer
# certificate is NOT bound to. They are used to test that the restriction check
# against the *set* developer certificate rejects them, even though the package
# itself is correctly dev-signed.

info "Create a package with an overreaching capability"
sed <info.yaml >"$src/info.yaml" 's/  code: "test"/  code: "test"\n  capabilities: [ "cap-denied" ]/'
packager create-package "$dst/test-cap-overreach.ampkg" "$src"
packager dev-sign-package --verbose "$dst/test-cap-overreach.ampkg" "$dst/test-cap-overreach-dev-signed.ampkg" certificates/dev-certs/dev-1.p12 password
cp "info.yaml" "$src"

info "Create a package with an overreaching category"
sed <info.yaml >"$src/info.yaml" 's/categories: \[ "test-category" \]/categories: [ "test-category", "denied-category" ]/'
packager create-package "$dst/test-cat-overreach.ampkg" "$src"
packager dev-sign-package --verbose "$dst/test-cat-overreach.ampkg" "$dst/test-cat-overreach-dev-signed.ampkg" certificates/dev-certs/dev-1.p12 password
cp "info.yaml" "$src"

# This package stays fully within the "narrow" developer certificate's bounds
# (test-pkg, test-app, test-category, runtime qml), but it is signed by dev-1.
# It is used to test that installing it while dev-narrow is the set certificate
# is rejected due to the signer mismatch - not due to any metadata overreach.
info "Create a package with a qml runtime, signed by dev-1"
sed <info.yaml >"$src/info.yaml" 's/runtime: "native"/runtime: "qml"/'
packager create-package "$dst/test-qml.ampkg" "$src"
packager dev-sign-package --verbose "$dst/test-qml.ampkg" "$dst/test-qml-dev-signed.ampkg" certificates/dev-certs/dev-1.p12 password
cp "info.yaml" "$src"

### no-icon package

info "Create a package without an icon"
mv "$src"/info.yaml{,.orig}
sed <"$src/info.yaml.orig" >"$src/info.yaml" 's/icon: "icon.png"//'
rm "$src"/info.yaml.orig
rm "$src"/icon.png
packager create-package "$dst/test-no-icon.ampkg" "$src"
cp "icon.png" "$src"
cp "info.yaml" "$src"

### create invalid packages

tar -C "$src" -xof "$dst/test.ampkg" -- --PACKAGE-HEADER-- --PACKAGE-FOOTER--

info "Create a package with invalid format"
echo "invalid" >"$dst/test-invalid-format.ampkg"

info "Create a package with an invalid formatVersion header field"
mv "$src"/--PACKAGE-HEADER--{,.orig}
sed <"$src/--PACKAGE-HEADER--.orig" >"$src/--PACKAGE-HEADER--" 's/formatVersion: 2/formatVersion: X/'
tar -C "$src" -cf "$dst/test-invalid-header-formatversion.ampkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-HEADER--{.orig,}

info "Create a package with a 0 diskSpaceUsed header field"
mv "$src"/--PACKAGE-HEADER--{,.orig}
sed <"$src/--PACKAGE-HEADER--.orig" >"$src/--PACKAGE-HEADER--" 's/diskSpaceUsed: [0-9]*/diskSpaceUsed: 0/'
tar -C "$src" -cf "$dst/test-invalid-header-diskspaceused.ampkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-HEADER--{.orig,}

info "Create a package with an invalid id header field"
mv "$src"/--PACKAGE-HEADER--{,.orig}
sed <"$src/--PACKAGE-HEADER--.orig" >"$src/--PACKAGE-HEADER--" "s/packageId: '[a-z0-9.-]*'/packageId: ':invalid'/"
tar -C "$src" -cf "$dst/test-invalid-header-id.ampkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-HEADER--{.orig,}

info "Create a package with a non-matching id header field"
mv "$src"/--PACKAGE-HEADER--{,.orig}
sed <"$src/--PACKAGE-HEADER--.orig" >"$src/--PACKAGE-HEADER--" "s/packageId: '[a-z0-9.-]*'/packageId: 'non-matching'/"
tar -C "$src" -cf "$dst/test-non-matching-header-id.ampkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-HEADER--{.orig,}

info "Create a package with a tampered extraSigned header field"
mv "$src"/--PACKAGE-HEADER--{,.orig}
( cat "$src/--PACKAGE-HEADER--.orig" ; echo "extraSigned: { foo: bar }") >"$src/--PACKAGE-HEADER--"
tar -C "$src" -cf "$dst/test-tampered-extra-signed-header.ampkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-HEADER--{.orig,}

info "Create a package with an invalid info.yaml id"
mv "$src"/info.yaml{,.orig}
sed <"$src/info.yaml.orig" >"$src/info.yaml" 's/id: "[a-z0-9.-]*"/id: ":invalid"/'
tar -C "$src" -cf "$dst/test-invalid-info-id.ampkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/info.yaml{.orig,}

info "Create a package with an invalid info.yaml file"
mv "$src"/info.yaml{,.orig}
sed <"$src/info.yaml.orig" >"$src/info.yaml" 's/code: "test"/: "test"/'
tar -C "$src" -cf "$dst/test-invalid-info.ampkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/info.yaml{.orig,}

info "Create a package with an invalid file order"
tar -C "$src" -cf "$dst/test-invalid-file-order.ampkg" -- --PACKAGE-HEADER-- info.yaml test icon.png --PACKAGE-FOOTER--

info "Create a package with an invalid digest"
mv "$src"/--PACKAGE-FOOTER--{,.orig}
tr <"$src/--PACKAGE-FOOTER--.orig" >"$src/--PACKAGE-FOOTER--" 3 0
tar -C "$src" -cf "$dst/test-invalid-footer-digest.ampkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/--PACKAGE-FOOTER--{.orig,}

info "Create a package with an invalid signature"
packager dev-sign-package --verbose "$dst/test.ampkg" "$dst/test-invalid-footer-signature.ampkg" certificates/other-certs/other.p12 password

info "Create a package with an invalid entry path"
touch "$src/../invalid-path"
tar -C "$src" -P -cf "$dst/test-invalid-path.ampkg" -- --PACKAGE-HEADER-- info.yaml icon.png ../invalid-path test --PACKAGE-FOOTER--
rm "$src/../invalid-path"

info "Create a package with a non-existent icon"
mv "$src"/info.yaml{,.orig}
sed <"$src/info.yaml.orig" >"$src/info.yaml" 's/icon: "icon.png"/icon: "png.icon"/'
tar -C "$src" -cf "$dst/test-non-existent-icon.ampkg" -- --PACKAGE-HEADER-- info.yaml icon.png test --PACKAGE-FOOTER--
mv "$src"/info.yaml{.orig,}

info "Create a package with an icon in a sub-directory"
mv "$src"/info.yaml{,.orig}
sed <"$src/info.yaml.orig" >"$src/info.yaml" 's,icon: "icon.png",icon: "sub/icon.png",'
mkdir "$src"/sub
cp "$src"/{,sub/}icon.png
tar -C "$src" -cf "$dst/test-icon-in-subdir.ampkg" -- --PACKAGE-HEADER-- info.yaml sub/icon.png test --PACKAGE-FOOTER--
rm -rf "$src"/sub
mv "$src"/info.yaml{.orig,}

echo -e "$G All test packages have been created successfully$W"
echo

exit 0
