#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <zookeeper/zookeeper.h>

bool connected = false;
std::mutex m;
std::condition_variable cv;
enum class ConnState { Connecting, Connected, Expired };
ConnState state = ConnState::Connecting;

void watcher(zhandle_t *zh, int type, int zk_state, const char *path,
             void *watcherCtx) {

  if (type == ZOO_SESSION_EVENT) {

    std::lock_guard<std::mutex> lock(m);
    if (zk_state == ZOO_CONNECTED_STATE) {
      std::cout << "Connected to ZooKeeper!" << std::endl;
      state = ConnState::Connected;
    } else if (zk_state == ZOO_EXPIRED_SESSION_STATE) {
      std::cerr << "ZooKeeper session expired!" << std::endl;
      state = ConnState::Expired;
    }
    cv.notify_one();
  }
}

int create(zhandle_t *zh, const std::string &path, const std::string &value,
           const struct ACL_vector *acl, int mode) {

  int rc = zoo_create(zh, path.c_str(), value.data(),
                      static_cast<int>(value.size()), acl, mode, nullptr, 0);
  if (rc == ZNODEEXISTS) {
    std::cout << "Node already exists\n";
  } else if (rc != ZOK) {
    std::cerr << "zoo_create failed: " << rc << std::endl;
  }

  return rc;
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
  cv.wait(lock, [] {
    return state == ConnState::Connected || state == ConnState::Expired;
  });

  if (state != ConnState::Connected) {
    std::cerr << "Unable to connect to ZooKeeper!" << std::endl;
    zookeeper_close(zh);
    return 1;
  }

  std::string path = "/zk-demo";
  std::string value = "hello zookeeper";

  // create
  int rc = create(zh, path, value, &ZOO_OPEN_ACL_UNSAFE, ZOO_PERSISTENT);
  if (rc != ZOK) {
    zookeeper_close(zh);
    return 1;
  }

  zookeeper_close(zh);
  return 0;
}
