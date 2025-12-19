#include <chrono>
#include <iostream>
#include <thread>
#include <zookeeper/zookeeper.h>

void watcher(zhandle_t *zh, int type, int state, const char *path,
             void *watcherCtx) {

  if (type == ZOO_SESSION_EVENT) {
    if (state == ZOO_CONNECTED_STATE) {
      std::cout << "Connected to ZooKeeper!" << std::endl;
    } else if (state == ZOO_EXPIRED_SESSION_STATE) {
      std::cerr << "ZooKeeper session expired!" << std::endl;
    }
  }
}

int main() {
  // Initialize ZooKeeper handle
  // "127.0.0.1:2181" is the default local address
  zhandle_t *zh =
      zookeeper_init("127.0.0.1:2181", watcher, 30000, nullptr, nullptr, 0);

  if (!zh) {
    std::cerr << "Error opening handle to ZooKeeper!" << std::endl;
    return 1;
  }

  std::cout << "Successfully initialized ZooKeeper connection handle. "
               "Waiting for connection..."
            << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(3000));

  zookeeper_close(zh);
  return 0;
}
