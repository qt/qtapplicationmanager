#!/bin/bash
#############################################################################
##
## Copyright (C) 2019 Luxoft Sweden AB
## Copyright (C) 2018 Pelagicore AG
## Contact: https://www.qt.io/licensing/
##
## This file is part of the Luxoft Application Manager.
##
## $QT_BEGIN_LICENSE:BSD-QTAS$
## Commercial License Usage
## Licensees holding valid commercial Qt Automotive Suite licenses may use
## this file in accordance with the commercial license agreement provided
## with the Software or, alternatively, in accordance with the terms
## contained in a written agreement between you and The Qt Company.  For
## licensing terms and conditions see https://www.qt.io/terms-conditions.
## For further information use the contact form at https://www.qt.io/contact-us.
##
## BSD License Usage
## Alternatively, you may use this file under the terms of the BSD license
## as follows:
##
## "Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
##   * Redistributions of source code must retain the above copyright
##     notice, this list of conditions and the following disclaimer.
##   * Redistributions in binary form must reproduce the above copyright
##     notice, this list of conditions and the following disclaimer in
##     the documentation and/or other materials provided with the
##     distribution.
##   * Neither the name of The Qt Company Ltd nor the names of its
##     contributors may be used to endorse or promote products derived
##     from this software without specific prior written permission.
##
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
## "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
## LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
## A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
## OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
## SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
## LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
## DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
## THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
##
## $QT_END_LICENSE$
##
## SPDX-License-Identifier: BSD-3-Clause
##
#############################################################################


SCRIPT=$(dirname $0)
usage()
{
     echo "$0 [-n <number of apps>][-r <runtime-template-name>][-q <qt-folder>][-t <test>] <appman-binary>"
     echo ""
     echo "This script will execute a benchmark using the appman binary provided by <appman-binary>."
     echo "It uses templates for defining the runtime used and also templates for the test to be executed."
     echo ""
     echo "The following options are accepted:"
     echo "-n <number of apps>         : Defines how many applications are executed at the same time."
     echo "-r <runtime-template-name>  : Defines which runtime template is used. Possible values are:"
     echo "  'appman-qml'              : Starts a normal appman qml runtime using appman-launcher-qml"
     echo "  'qmlscene'                : Starts a native runtime and using qmlscene for interpreting the qml code"
     echo "-q <qt-folder>              : Defines which Qt is used. This is needed by some runtime-templates e.g. qmlscene."
     echo "-t <test>                   : Only executes this given single test. If not provided all tests in the 'tests' folder are executed"
     echo ""
     exit 1
}


TEMPLATE=appman-qml
APPS=4
QT_FOLDER=
TEST=

while getopts ":n:r:q:t:" option
do
case "${option}"
in
n) APPS=${OPTARG};;
r) TEMPLATE=${OPTARG};;
q) QT_FOLDER="${OPTARG}/bin/";;
t) TEST=${OPTARG};;
*) usage;;
esac
done

[ "$#" -lt 1 ] && usage
APPMAN="${@: -1}"
[ ! -e "$APPMAN" ] && usage

trap exit INT QUIT 0
run_test()
{
    temp_folder=$(mktemp -d)
    temp_app_folder="$temp_folder/apps"
    mkdir -p $temp_folder

    cp -a system-ui $temp_folder/
    cp -a am-config.yaml $temp_folder/

    mkdir -p $temp_app_folder

    for (( i=1; i<=$APPS; i++ ))
    do
        appid="app$i"
        mkdir -p $temp_app_folder/$appid
        cp -a templates/$TEMPLATE/* $temp_app_folder/$appid/
        sed -i -e "s/TEST_ID/$appid/g" $temp_app_folder/$appid/info.yaml
        sed -i -e "s|QT_FOLDER/|$QT_FOLDER|g" $temp_app_folder/$appid/info.yaml
    done

    echo "Running $test_qml in $temp_folder"
    cp $test_qml $temp_folder/test.qml
    (cd $temp_folder && $APPMAN -c am-config.yaml -r --no-dlt-logging)
}

if [ -n "$TEST" ]
then
    echo "RUNNING SINGLE TEST"
    if [ ! -e "tests/$TEST" ]; then
        echo "Test doesn't exist"
        exit 1
    fi
    test_qml="tests/$TEST"
    run_test
else
    echo "RUNNING ALL TESTS"
    for test_qml in tests/*
    do
        run_test
    done
fi


