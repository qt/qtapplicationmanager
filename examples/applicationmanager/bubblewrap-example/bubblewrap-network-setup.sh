#!/bin/sh
# Copyright (C) 2023 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

# Parameters:
# $1  event  The stage for which this script has been called: can be "quicklaunch", "start" or "stop".
#             - "quicklaunch" is called when a new quicklauncher instance is started.
#             - "start" is called when an applications starts or when an application is attached to
#               an already running quicklaunch instance.
#             - "stop" is called when an application (or unused quicklauncher instance) shuts down.
# $2  appid  The application's id for both the "start" and "stop" events. It is simply "quicklaunch"
#            for the "quicklaunch" event and also for "stop" events on unused quicklauncher instances.
# $3  pid    The PID of the containerized application. Use /proc/<pid>/ns/net to access the
#            corresponding network namespace. In case of the "stop" event, the process with the
#            given PID is already dead and just acts as a reference.

set -e
#set -x

echo >&2 "Network setup script called: $@"

#######

HOST_BRIDGE="br0"
HOST_IF_PREFIX="qtam-"
CONT_IF="veth"
CONT_IP="10.1.1.111/24"
CONT_GW="10.1.1.1"

#######

EVENT="$1"
APPID="$2"
NETNS_PID="$3"
# network device names can be max 15 chars: "qtam-<10 chars>"
HASHED_APPID=$(echo $APPID | sha1sum -b | base64 | tr '/' '_' | cut -b 1-10)
HOST_IF="${HOST_IF_PREFIX}${HASHED_APPID}"
NETNS="qtam-$APPID"

case "$EVENT" in
start)
  # "ip netns" cannot use the special /proc/<pid>/ns/net symlinks directly. Instead, we need to
  # create a second level link in /var/run/netns and then reference the network namespace by the
  # name of the this symlink.
  mkdir -p /var/run/netns
  ln -sfT /proc/$NETNS_PID/ns/net "/var/run/netns/$NETNS"

  # create a virtual ethernet (veth) device pair
  ip link add "$HOST_IF" type veth peer name "$CONT_IF" netns "$NETNS"

  # host side: bring it up and add it to an existing bridge
  ip link set dev "$HOST_IF" up

  # for a bridged setup, add the host interface to your external bridge
  # brctl addif "${HOST_BRIDGE}" "${HOST_IF}"

  # client side: bring it up, set a fixed IP and add a default route
  ip -n "$NETNS" link set dev "$CONT_IF" up
  ip -n "$NETNS" addr add "$CONT_IP" dev "$CONT_IF"
  ip -n "$NETNS" route add default via "$CONT_GW"
  ;;

stop)
  # the kernel cleans up our veth pair automatically

  # we could have removed this link at the end of "start" above already, but keeping it around
  # makes debugging the network namespace easier during development
  rm -f "/var/run/netns/$NETNS"
  ;;

quicklaunch)
  # the quick launcher does not need network access
  ;;
esac

