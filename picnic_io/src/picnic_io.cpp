// SPDX-License-Identifier: MIT
// Copyright (c) 2026 myocro
// PICNIC2 UDP I/O library implementation.

#include "picnic_io.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

template <typename... Args>
static std::string format(const std::string &format, Args... args) {
    size_t len = std::snprintf(nullptr, 0, format.c_str(), args...);
    std::vector<char> buf(len + 1);
    std::snprintf(&buf[0], len + 1, format.c_str(), args...);
    return std::string(&buf[0], &buf[0] + len);
}

static uint64_t timestamp(void) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}
static uint64_t timestamp_msec(int msec){
  return (uint64_t)msec * (uint64_t)1000000;
}

class PICNICIO::Impl {
public:
  Impl() = default;
  ~Impl() = default;
  std::string debugMsg;
  int target_socket_=-1;
  sockaddr_in target_socket_address_;

  unsigned char rbuf[16];
  bool recv(uint16_t &value, uint16_t & reg_TRIS_RJ_RE, std::string &error) {
    sockaddr_in recv_from;
    socklen_t sockaddr_in_size = sizeof(sockaddr_in);
    ssize_t read_data_size =
        recvfrom(target_socket_, rbuf, sizeof(rbuf), 0,
                 reinterpret_cast<sockaddr *>(&recv_from), &sockaddr_in_size);
    if (read_data_size < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        error = "receive timeout";
      } else {
        error = format("receive error (%s)", std::strerror(errno));
      }
      return false;
    }
    if (read_data_size != 8) {
      error = format("invalid response size (%zd)", read_data_size);
      return false;
    }
    value =  (rbuf[4] << 8 )+ rbuf[5];
    reg_TRIS_RJ_RE = (rbuf[2] << 8) + rbuf[3];
    return true;
  }
};

PICNICIO::PICNICIO(): pimpl(std::make_unique<Impl>()) {
}
PICNICIO::~PICNICIO() { close(); }
PICNICIO::PICNICIO(PICNICIO&&) noexcept = default;
PICNICIO& PICNICIO::operator=(PICNICIO&&) noexcept = default;

bool PICNICIO::isOpened(void) { return pimpl->target_socket_ >= 0; }

bool PICNICIO::open(const std::string &target_address){
  pimpl->debugMsg = "";
  close();

  const size_t address_str_separator_pos = target_address.rfind(':');
  if (address_str_separator_pos == std::string::npos || address_str_separator_pos == 0 ||
      address_str_separator_pos + 1 >= target_address.size()) {
    pimpl->debugMsg = "format error ("+target_address+")";
    return false;
  }
  const std::string target_ip_address =
      target_address.substr(0, address_str_separator_pos);
  const std::string target_port_string =
      target_address.substr(address_str_separator_pos + 1);

  char *port_end = nullptr;
  errno = 0;
  const long parsed_port = std::strtol(target_port_string.c_str(), &port_end, 10);
  if (errno != 0 || port_end == nullptr || *port_end != '\0' ||
      parsed_port <= 0 || parsed_port > std::numeric_limits<uint16_t>::max()) {
    pimpl->debugMsg = "port error ("+target_address+")";
    return false;
  }
  const uint16_t target_port = static_cast<uint16_t>(parsed_port);

  pimpl->target_socket_address_ = sockaddr_in();
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  addrinfo *result = nullptr;
  const int gai_error =
      getaddrinfo(target_ip_address.c_str(), nullptr, &hints, &result);
  if (gai_error != 0 || result == nullptr) {
    pimpl->debugMsg = "host name lookup failure ("+target_address+")";
    return false;
  }

  pimpl->target_socket_address_ =
      *reinterpret_cast<sockaddr_in *>(result->ai_addr);
  freeaddrinfo(result);

  pimpl->target_socket_address_.sin_port = htons(target_port);
  pimpl->target_socket_address_.sin_family = AF_INET;
  pimpl->target_socket_ = socket(AF_INET, SOCK_DGRAM, 0);

  if (pimpl->target_socket_ < 0) {
    pimpl->debugMsg = format("Socket open error.(ip=%s,port=%d)",
            target_ip_address.c_str(), target_port);
    return false;
  }

  struct timeval tv;
  tv.tv_sec = 5;
  tv.tv_usec = 0;
  if (setsockopt(pimpl->target_socket_, SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char *>(&tv), sizeof(tv)) != 0) {
    pimpl->debugMsg = format("socket option error (%s)", std::strerror(errno));
    close();
    return false;
  }

  pimpl->debugMsg = "Socket init succeeded";
  return true;
}

void PICNICIO::close() {
  if (pimpl->target_socket_ >= 0) {
    ::close(pimpl->target_socket_);
    pimpl->target_socket_ = -1;
  }
}

bool PICNICIO::getValues(std::vector<uint16_t> &dstValues){
  if (!isOpened()) {
    dstValues.clear();
    pimpl->debugMsg = "socket is not open";
    return false;
  }

  uint64_t start_time = timestamp();
  pimpl->debugMsg = "";

  constexpr int chCount = 8;
  std::vector<uint16_t> values(chCount);
  unsigned char ch_new_picnic[] = {0x81,0x89,0x91,0x99,0xa1,0xa9,0xb1,0xb9};

  uint16_t reg_TRIS_RJ_RE = 0;
  for(int i=0;i<chCount;){
      unsigned char packet[3] = {0x04, ch_new_picnic[i], 0x00};
      ssize_t sz = sendto(pimpl->target_socket_, packet, sizeof(packet), 0,
                 reinterpret_cast<const sockaddr *>(&pimpl->target_socket_address_),
                 sizeof(pimpl->target_socket_address_));

      if(sz != static_cast<ssize_t>(sizeof(packet))){
          dstValues.clear();
          pimpl->debugMsg = format("ERR: fail to send request(ch:%d) ",i);
          return false;
      }
      uint16_t value;
      uint16_t r;
      std::string recv_error;
      if(pimpl->recv(value, r, recv_error)){
          values[i] = value;
          if(i==0)
            reg_TRIS_RJ_RE = r;
          else if(reg_TRIS_RJ_RE != r){
            dstValues.clear();
            pimpl->debugMsg = format("ERR: TRIS_RJ/RE check fail(ch:%d)", i);
            return false;
          }
          i++;
      }else{
          dstValues.clear();
          pimpl->debugMsg = format("ERR: %s (ch:%d)", recv_error.c_str(), i);
          return false;
      }
      uint64_t t = (timestamp() - start_time);
      if(t>timestamp_msec(10000)){
        dstValues.clear();
        pimpl->debugMsg = format("timeout (receivedNum=%d)", i);
        return false;
      };
  }
  dstValues = std::move(values);
  pimpl->debugMsg = "OK";
  return true;
}

std::string PICNICIO::getDebugMsg(){
  return pimpl->debugMsg;
}
