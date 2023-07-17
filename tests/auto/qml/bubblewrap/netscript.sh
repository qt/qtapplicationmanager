#!/bin/sh
# Copyright (C) 2023 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

bindir="$(dirname "$(readlink /proc/$PPID/exe)")"

if [ -x "$bindir/appman-controller" ]; then
  ( exec nohup "$bindir/appman-controller" inject-intent-request \
       --requesting-application-id ":sysui:" --broadcast \
       "netscript-args" "{ \"args\": \"$*\" }" \
  ) >/dev/null 2>&1 &
else
  echo >&2 "Could not find appman-controller in $bindir"
  exit 1
fi
