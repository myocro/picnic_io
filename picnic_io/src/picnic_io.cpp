#include "picnic_io.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
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
  bool recv(uint16_t &value, uint16_t & reg_TRIS_RJ_RE) {
    sockaddr_in recv_from;
    socklen_t sockaddr_in_size = sizeof(sockaddr_in);
    ssize_t read_data_size =
        recvfrom(target_socket_, rbuf, sizeof(rbuf), 0,
                 reinterpret_cast<sockaddr *>(&recv_from), &sockaddr_in_size);
    if (read_data_size == 8) {
      value =  (rbuf[4] << 8 )+ rbuf[5];
      reg_TRIS_RJ_RE = (rbuf[2] << 8) + rbuf[3];
      return true;
    } else {
      return false;
    }
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

  size_t address_str_separator_pos = target_address.find_first_of(":");
  if (address_str_separator_pos == std::string::npos || address_str_separator_pos == 0) {
    pimpl->debugMsg = "format error ("+target_address+")";
    return false;
  }
  std::string target_ip_address =
      target_address.substr(0, address_str_separator_pos);
  uint16_t target_port =
      atoi(target_address.substr(address_str_separator_pos + 1).c_str());

  pimpl->target_socket_address_ = sockaddr_in();
  pimpl->target_socket_address_.sin_addr.s_addr =
      inet_addr(target_ip_address.c_str());

 if (pimpl->target_socket_address_.sin_addr.s_addr == 0xffffffff) {
	 struct hostent *host;
	 host = gethostbyname(target_ip_address.c_str());
	 if (host == NULL) {
     pimpl->debugMsg = "host name lookup failure ("+target_address+")";
		 return false;
	 }
	 pimpl->target_socket_address_.sin_addr.s_addr =
             *(unsigned int *)host->h_addr_list[0];
 }

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
  setsockopt(pimpl->target_socket_, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv,
             sizeof(tv));

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
  uint64_t start_time = timestamp();
  dstValues.clear();
  pimpl->debugMsg = "";

  constexpr int chCount = 8;
  dstValues.resize(chCount);
  unsigned char ch_new_picnic[] = {0x81,0x89,0x91,0x99,0xa1,0xa9,0xb1,0xb9};

  uint16_t reg_TRIS_RJ_RE = 0;
  for(int i=0;i<chCount;){
      unsigned char packet[3] = {0x04, ch_new_picnic[i], 0x00};
      ssize_t sz = sendto(pimpl->target_socket_, packet, sizeof(packet), 0,
                 reinterpret_cast<const sockaddr *>(&pimpl->target_socket_address_),
                 sizeof(pimpl->target_socket_address_));

      if(sz != static_cast<ssize_t>(sizeof(packet))){
          pimpl->debugMsg = format("ERR: fail to send request(ch:%d) ",i);
          return false;
      }
      uint16_t value;
      uint16_t r;
      if(pimpl->recv(value, r)){
          dstValues[i] = value;
          if(i==0)
            reg_TRIS_RJ_RE = r;
          else if(reg_TRIS_RJ_RE != r){
            pimpl->debugMsg = format("ERR: TRIS_RJ/RE check fail(ch:%d)", i);
            return false;
          }
          i++;
      }else{
          pimpl->debugMsg = format("ERR: Invalid data(ch:%d)", i);
          return false;
      }
      uint64_t t = (timestamp() - start_time);
      if(t>timestamp_msec(10000)){
        pimpl->debugMsg = format("timeout (receivedNum=%d)", i);
        return false;
      };
  }
  pimpl->debugMsg = "OK";
  return true;
}

std::string PICNICIO::getDebugMsg(){
  return pimpl->debugMsg;
}
