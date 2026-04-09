#include "picnic_io.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

class MockPicnicServer {
public:
  MockPicnicServer() {
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
      throw std::runtime_error("failed to create UDP socket");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(socket_fd_, reinterpret_cast<const sockaddr *>(&address),
             sizeof(address)) != 0) {
      ::close(socket_fd_);
      throw std::runtime_error("failed to bind UDP socket");
    }

    sockaddr_in bound{};
    socklen_t bound_size = sizeof(bound);
    if (getsockname(socket_fd_, reinterpret_cast<sockaddr *>(&bound),
                    &bound_size) != 0) {
      ::close(socket_fd_);
      throw std::runtime_error("failed to inspect UDP socket");
    }
    port_ = ntohs(bound.sin_port);

    timeval timeout{};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    worker_ = std::thread([this] { serve(); });
  }

  ~MockPicnicServer() {
    stop_ = true;
    if (socket_fd_ >= 0) {
      ::close(socket_fd_);
      socket_fd_ = -1;
    }
    if (worker_.joinable()) {
      worker_.join();
    }
  }

  uint16_t port() const { return port_; }

private:
  void serve() {
    constexpr uint16_t expected_register = 0x1357;
    constexpr uint8_t request_channels[8] = {0x81, 0x89, 0x91, 0x99,
                                             0xa1, 0xa9, 0xb1, 0xb9};

    while (!stop_) {
      unsigned char request[3];
      sockaddr_in client{};
      socklen_t client_size = sizeof(client);
      const ssize_t received =
          recvfrom(socket_fd_, request, sizeof(request), 0,
                   reinterpret_cast<sockaddr *>(&client), &client_size);
      if (received < 0) {
        continue;
      }
      if (received != 3 || request[0] != 0x04) {
        continue;
      }

      int channel = -1;
      for (int i = 0; i < 8; ++i) {
        if (request[1] == request_channels[i]) {
          channel = i;
          break;
        }
      }
      if (channel < 0) {
        continue;
      }

      const uint16_t value = static_cast<uint16_t>(1000 + channel);
      unsigned char response[8] = {};
      response[2] = static_cast<unsigned char>((expected_register >> 8) & 0xff);
      response[3] = static_cast<unsigned char>(expected_register & 0xff);
      response[4] = static_cast<unsigned char>((value >> 8) & 0xff);
      response[5] = static_cast<unsigned char>(value & 0xff);
      sendto(socket_fd_, response, sizeof(response), 0,
             reinterpret_cast<const sockaddr *>(&client), client_size);
    }
  }

  std::atomic<bool> stop_{false};
  int socket_fd_{-1};
  uint16_t port_{0};
  std::thread worker_;
};

void test_open_requires_well_formed_address() {
  PICNICIO picnic;
  require(!picnic.open("127.0.0.1"), "address without port should fail");
  require(picnic.getDebugMsg().find("format error") != std::string::npos,
          "format error message should be reported");

  require(!picnic.open("127.0.0.1:abc"), "non-numeric port should fail");
  require(picnic.getDebugMsg().find("port error") != std::string::npos,
          "port error message should be reported");
}

void test_get_values_requires_open_socket() {
  PICNICIO picnic;
  std::vector<uint16_t> values = {1, 2, 3};
  require(!picnic.getValues(values), "reading before open should fail");
  require(values.empty(), "failed read should clear values");
  require(picnic.getDebugMsg() == "socket is not open",
          "unopened socket error should be explicit");
}

void test_get_values_round_trip() {
  MockPicnicServer server;
  PICNICIO picnic;
  require(picnic.open("127.0.0.1:" + std::to_string(server.port())),
          "open against local mock server should succeed");

  std::vector<uint16_t> values;
  require(picnic.getValues(values), "round-trip ADC read should succeed");
  require(values.size() == 8, "exactly eight ADC values should be returned");
  for (size_t i = 0; i < values.size(); ++i) {
    require(values[i] == static_cast<uint16_t>(1000 + i),
            "mock ADC value mismatch");
  }
  require(picnic.getDebugMsg() == "OK", "successful read should set OK message");
}

template <typename Func>
bool runTest(const std::string &name, const std::string &description, Func test) {
  std::cout << "[RUN ] " << name << " - " << description << std::endl;
  try {
    test();
    std::cout << "[PASS] " << name << std::endl;
    return true;
  } catch (const std::exception &error) {
    std::cout << "[FAIL] " << name << " - " << error.what() << std::endl;
    return false;
  }
}

}  // namespace

int main() {
  bool ok = true;
  ok = runTest("open_requires_well_formed_address",
               "malformed address and port are rejected",
               test_open_requires_well_formed_address) && ok;
  ok = runTest("get_values_requires_open_socket",
               "read fails clearly before open",
               test_get_values_requires_open_socket) && ok;
  ok = runTest("get_values_round_trip",
               "mock UDP server returns 8 ADC values",
               test_get_values_round_trip) && ok;

  if (!ok) {
    return 1;
  }
  return 0;
}
