// SPDX-License-Identifier: MIT
// Copyright (c) 2026 myocro
// PICNIC2 UDP I/O library interface.

#ifndef PICNIC_IO_HPP_
#define PICNIC_IO_HPP_
#include <memory>
#include <string>
#include <vector>

class PICNICIO {
public:
  PICNICIO();
  ~PICNICIO();

  PICNICIO(const PICNICIO&) = delete;
  PICNICIO& operator=(const PICNICIO&) = delete;
  PICNICIO(PICNICIO&&) noexcept;
  PICNICIO& operator=(PICNICIO&&) noexcept;

  std::string getDebugMsg();
  bool isOpened(void);
  bool open(const std::string &target_address);
  void close();
  bool getValues(std::vector<uint16_t> &dstValues);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl; 
};

#endif
