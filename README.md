# Distributed Key-Value Store with Zookeeper

A minimal implementation in C++ of a Key-Value Store using Zookeeper for leader election and coordination.

## Dependencies

Install `libzookeeper-mt-dev` and `cmake`.

```sh
sudo apt install libzookeeper-mt-dev cmake
```

## Build

First, build the project with cmake.

```sh
cmake -B build
cmake --build build
```

Make sure ZooKeeper is running locally on the default port 2181. To run with docker, execute:

```sh
docker run -p 2181:2181 zookeeper
```

## Run

Execute the binary:

```sh
./build/kv_server
```

## License

[MIT](LICENSE) © André Brandão
