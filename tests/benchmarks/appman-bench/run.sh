#!/bin/bash
# Copyright (C) 2021 The Qt Company Ltd.
# Copyright (C) 2019 Luxoft Sweden AB
# Copyright (C) 2018 Pelagicore AG
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause


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
     echo "  'qmlscene'                : Starts a native runtime using qmlscene for interpreting the qml code"
     echo "-q <qt-folder>              : Defines which Qt is used. This is required by the qmlscene runtime-template."
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
    (cd $temp_folder && $APPMAN -c am-config.yaml --clear-cache --no-dlt-logging)
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


