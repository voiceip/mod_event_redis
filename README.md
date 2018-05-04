# mod_event_redis
Freeswitch Redis Events Plugin

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

