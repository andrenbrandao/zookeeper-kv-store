#include <chrono>
#include <condition_variable>
#include <cstring>
#include <gflags/gflags.h>
#include <iostream>
#include <mutex>
#include <zookeeper/zookeeper.h>

DEFINE_string(server_id, "", "Defines the server id.");

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

void children_watcher(zhandle_t *zh, int type, int zk_state, const char *path,
                      void *watcherCtx) {
  std::cout << "/live-servers changed. New values: ";

  struct String_vector strings;
  // Re-register the watcher because ZooKeeper's watchers are one-shot.
  int rc = zoo_wget_children(zh, path, children_watcher, nullptr, &strings);
  if (rc == ZOK) {
    // Print each child node.
    for (int i = 0; i < strings.count; ++i) {
      std::cout << strings.data[i] << " ";
    }
  }
  deallocate_String_vector(&strings);
  std::cout << std::endl;
}

int get_children(zhandle_t *zh, const std::string &path, watcher_fn watcher,
                 std::vector<std::string> &children) {
  struct String_vector strings;
  int rc = zoo_wget_children(zh, path.c_str(), watcher, nullptr, &strings);
  if (rc != ZOK) {
    std::cerr << "zoo_wget_children failed: " << rc << std::endl;
    return rc;
  }
  for (int i = 0; i < strings.count; ++i) {
    children.push_back(strings.data[i]);
  }
  deallocate_String_vector(&strings);
  return rc;
}

int create(zhandle_t *zh, const std::string &path, const std::string &value,
           const struct ACL_vector *acl, int mode,
           std::string *created_path = nullptr) {

  char path_buffer[4096];
  int path_buffer_len = sizeof(path_buffer);
  int rc =
      zoo_create(zh, path.c_str(), value.data(), static_cast<int>(value.size()),
                 acl, mode, path_buffer, path_buffer_len);
  if (rc == ZNODEEXISTS) {
    std::cout << "Node already exists\n";
  } else if (rc != ZOK) {
    std::cerr << "zoo_create failed: " << rc << std::endl;
  }

  if (rc == ZOK && created_path) {
    *created_path = path_buffer;
  }

  return rc;
}

int get_data(zhandle_t *zh, const std::string &path, std::string &output) {
  char buffer[4096];
  int buffer_len = sizeof(buffer);
  int rc = zoo_get(zh, path.c_str(), 0, buffer, &buffer_len, nullptr);
  if (rc != ZOK) {
    std::cerr << "zoo_get failed: " << rc << std::endl;
    return rc;
  }

  if (buffer_len > sizeof(buffer)) {
    std::cerr << "zoo_get failed. Data truncated (buffer too small)\n";
    return ZBADARGUMENTS;
  }

  output.assign(buffer, buffer_len);
  return rc;
}

int set_data(zhandle_t *zh, const std::string &path, const std::string &value) {
  int rc = zoo_set(zh, path.c_str(), value.c_str(),
                   static_cast<int>(value.size()), -1);
  if (rc != ZOK) {
    std::cerr << "zoo_set failed: " << rc << std::endl;
  }
  return rc;
}

int main(int argc, char *argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_server_id == "") {
    std::cerr << "Must provide a server id with flag --server_id." << std::endl;
    return 1;
  }

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

  // Create a persistent node.
  int rc = create(zh, path, value, &ZOO_OPEN_ACL_UNSAFE, ZOO_PERSISTENT);
  if (rc != ZOK && rc != ZNODEEXISTS) {
    zookeeper_close(zh);
    return 1;
  }

  // Read the value from it.
  std::string output;
  rc = get_data(zh, path, output);
  if (rc != ZOK) {
    zookeeper_close(zh);
    return 1;
  }

  std::cout << "Read from ZooKeeper: " << output << std::endl;

  // Set new value to node.
  std::string new_value =
      "hello from timestamp: " +
      std::format("{:%Y-%m-%d %H:%M:%OS}", std::chrono::system_clock::now());
  rc = set_data(zh, path, new_value);
  if (rc != ZOK) {
    zookeeper_close(zh);
    return 1;
  }

  // Read the value again.
  std::string saved_value;
  rc = get_data(zh, path, saved_value);
  if (rc != ZOK && rc != ZNODEEXISTS) {
    zookeeper_close(zh);
    return 1;
  }

  std::cout << "Read from ZooKeeper: " << saved_value << std::endl;

  // Maintain a list of live servers.
  // Create a persistent node.
  rc = create(zh, "/live-servers", "", &ZOO_OPEN_ACL_UNSAFE, ZOO_PERSISTENT);
  if (rc != ZOK && rc != ZNODEEXISTS) {
    zookeeper_close(zh);
    return 1;
  }

  // Create an ephemeral node representing the connection of each server.
  std::string ephemeral_path = "/live-servers/child-";
  std::string created_ephemeral_path;
  std::cout << "Creating ephemeral node: " << ephemeral_path << std::endl;
  rc = create(zh, ephemeral_path, "", &ZOO_OPEN_ACL_UNSAFE,
              ZOO_EPHEMERAL_SEQUENTIAL, &created_ephemeral_path);
  if (rc != ZOK && rc != ZNODEEXISTS) {
    zookeeper_close(zh);
    return 1;
  }
  std::cout << "ZooKeeper created ephemeral node at: " << created_ephemeral_path
            << std::endl;

  if (rc != ZNODEEXISTS) {
    std::cout << "Ephemeral node created!" << std::endl;
  }

  std::vector<std::string> children;
  rc = get_children(zh, "/live-servers", children_watcher, children);
  if (rc != ZOK) {
    zookeeper_close(zh);
    return 1;
  }
  std::cout << "/live-servers changed. New values: ";
  for (const std::string &node : children) {
    std::cout << node << " ";
  }
  std::cout << std::endl;

  cv.wait(lock, [] { return state == ConnState::Expired; });

  zookeeper_close(zh);
  return 0;
}
