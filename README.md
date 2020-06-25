# mod_event_redis
Freeswitch Redis Events Plugin

![Build Status](https://travis-ci.com/voiceip/mod_event_redis.svg?branch=master)


### Build

```
git submodule update --init --recursive
pushd lib/cpp_redis
mkdir build
cd build
cmake ..
make
make install
popd

make
make install
```

