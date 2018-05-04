#!/bin/bash
set -ex

BUILD_ROOT=$(mktemp -d)
VERSION=$(date +%s)
cp -r debian/* $BUILD_ROOT/

mkdir -p $BUILD_ROOT/usr/local/lib/
mkdir -p $BUILD_ROOT/usr/lib/freeswitch/mod
mkdir -p $BUILD_ROOT/etc/freeswitch/autoload_configs/

cp /etc/freeswitch/autoload_configs/event_redis.conf.xml  $BUILD_ROOT/etc/freeswitch/autoload_configs/event_redis.conf.xml
cp /usr/lib/freeswitch/mod/mod_event_redis.so  $BUILD_ROOT/usr/lib/freeswitch/mod/mod_event_redis.so

sed -i "s/_VERSION_/$VERSION/g" $BUILD_ROOT/DEBIAN/control
dpkg-deb --build $BUILD_ROOT freeswitch-mod-event-redis.deb

rm -rf $BUILD_ROOT