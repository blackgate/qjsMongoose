# Mongoose bindings for QuickJS

Quickjs bindings for the [mongoose](https://mongoose.ws/) c library.

## Implemented features

- [x] Http server
- [ ] Https server
- [x] Websocket server
- [ ] WebSocket client
- [ ] Http client
- [ ] Mqtt server
- [x] Mqtt client
- [x] Sntp client

## Building

```sh
mkdir build
cd build
cmake ..
make
```

## Installation

Just copy the `libqjsMongoose.so` to the desired location or install it system wide with:

```sh
sudo make install
```

## Run examples

```
sh ./run-example.sh ./examples/<example-directory>
```
