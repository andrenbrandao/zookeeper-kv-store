#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <zookeeper/zookeeper.h>

std::atomic<bool> connected = false;
std::mutex m;
std::condition_variable cv;

void watcher(zhandle_t *zh, int type, int state, const char *path,
             void *watcherCtx) {

  if (type == ZOO_SESSION_EVENT) {
    if (state == ZOO_CONNECTED_STATE) {
      std::cout << "Connected to ZooKeeper!" << std::endl;
      {
        std::lock_guard<std::mutex> lock(m);
        connected = true;
      }
      cv.notify_one();
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

  std::unique_lock lock(m);
  if (!cv.wait_for(lock, std::chrono::milliseconds(3000),
                   [] { return connected.load(); })) {
    std::cerr << "Unable to connect to ZooKeeper!" << std::endl;
    zookeeper_close(zh);
    return 1;
  }

  // Use Zookeeper here.

  zookeeper_close(zh);
  return 0;
}
