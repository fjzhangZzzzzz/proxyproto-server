/**
 * @file inet_address.h
 * @author fangjun.zhang (fjzhang_@outlook.com)
 * @brief
 * @version 0.1
 * @date 2022-06-30
 *
 * @copyright Copyright (c) 2022
 *
 */

#pragma once

#include <netinet/in.h>
#include <stdint.h>

#include <string>

class InetAddress {
  enum Family { kIpv4, kIpv6 };

 public:
  explicit InetAddress(uint16_t port = 0, Family family = kIpv4,
                       bool loopback_only = false);
  InetAddress(const std::string& addr, uint16_t port, Family family = kIpv4);
  explicit InetAddress(const struct sockaddr_in& addr) : addr4_(addr) {}
  explicit InetAddress(const struct sockaddr_in6& addr) : addr6_(addr) {}

  const struct sockaddr* GetSockAddr() const {
    return reinterpret_cast<const struct sockaddr*>(&addr6_);
  }

  sa_family_t family() const { return addr4_.sin_family; }

  void set_addr4(const struct sockaddr_in& addr) { addr4_ = addr; }
  void set_addr6(const struct sockaddr_in6& addr) { addr6_ = addr; }

  std::string ToAddr();
  uint16_t ToPort();
  std::string ToAddrPort();

 private:
  union {
    struct sockaddr_in addr4_;
    struct sockaddr_in6 addr6_;
  };
};
