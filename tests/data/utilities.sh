# Copyright (C) 2021 The Qt Company Ltd.
# Copyright (C) 2019 Luxoft Sweden AB
# Copyright (C) 2018 Pelagicore AG
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

Y="\033[0;33m"
R="\033[0;31m"
G="\033[0;32m"
W="\033[0m"

[ `uname` == 'Darwin' ] && isMac=1 || isMac=0

echo()
{
    [ $isMac -eq 1 ] && builtin echo "$@" || /bin/echo "$@"
}

info()
{
  echo -e "$Y * $1 $W"
}

