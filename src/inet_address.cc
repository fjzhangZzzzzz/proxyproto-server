/**
 * @file inet_address.cc
 * @author fangjun.zhang (fjzhang_@outlook.com)
 * @brief
 * @version 0.1
 * @date 2022-06-30
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "inet_address.h"

#include <arpa/inet.h>  // inet_ntop()

#include <cstring>

// INADDR_ANY use (type)value casting.
#pragma GCC diagnostic ignored "-Wold-style-cast"
static const in_addr_t kInaddrAny = INADDR_ANY;
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;
#pragma GCC diagnostic error "-Wold-style-cast"

//     /* Structure describing an Internet socket address.  */
//     struct sockaddr_in {
//         sa_family_t    sin_family; /* address family: AF_INET */
//         uint16_t       sin_port;   /* port in network byte order */
//         struct in_addr sin_addr;   /* internet address */
//     };

//     /* Internet address. */
//     typedef uint32_t in_addr_t;
//     struct in_addr {
//         in_addr_t       s_addr;     /* address in network byte order */
//     };

//     struct sockaddr_in6 {
//         sa_family_t     sin6_family;   /* address family: AF_INET6 */
//         uint16_t        sin6_port;     /* port in network byte order */
//         uint32_t        sin6_flowinfo; /* IPv6 flow information */
//         struct in6_addr sin6_addr;     /* IPv6 address */
//         uint32_t        sin6_scope_id; /* IPv6 scope-id */
//     };

namespace detail {

static int ToAddr(const struct sockaddr* addr, char* buf, size_t size) {
  if (addr->sa_family == AF_INET) {
    const struct sockaddr_in* addr4 =
        reinterpret_cast<const struct sockaddr_in*>(addr);
    return inet_ntop(AF_INET, &addr4->sin_addr, buf,
                     static_cast<socklen_t>(size)) == nullptr
               ? -1
               : 0;
  } else if (addr->sa_family == AF_INET6) {
    const struct sockaddr_in6* addr6 =
        reinterpret_cast<const struct sockaddr_in6*>(addr);
    return inet_ntop(AF_INET6, &addr6->sin6_addr, buf,
                     static_cast<socklen_t>(size)) == nullptr
               ? -1
               : 0;
  }
  return -1;
}

static int ToAddrPort(const struct sockaddr* addr, char* buf, size_t size) {
  if (ToAddr(addr, buf, size) != 0) {
    return -1;
  }
  size_t end = strlen(buf);
  uint16_t port =
      htons(reinterpret_cast<const struct sockaddr_in*>(addr)->sin_port);
  snprintf(buf + end, size - end, ":%u", port);
  return 0;
}

static int FromAddrPort(const char* ip, uint16_t port,
                        struct sockaddr_in* addr) {
  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);
  return inet_pton(AF_INET, ip, &addr->sin_addr) <= 0 ? -1 : 0;
}

static int FromAddrPort(const char* ip, uint16_t port,
                        struct sockaddr_in6* addr) {
  addr->sin6_family = AF_INET6;
  addr->sin6_port = htons(port);
  return inet_pton(AF_INET6, ip, &addr->sin6_addr) <= 0 ? -1 : 0;
}

}  // namespace detail

InetAddress::InetAddress(uint16_t port, Family family, bool loopback_only) {
  if (family == kIpv4) {
    memset(&addr4_, 0, sizeof addr4_);
    addr4_.sin_family = AF_INET;
    in_addr_t ip = loopback_only ? kInaddrLoopback : kInaddrAny;
    addr4_.sin_addr.s_addr = htonl(ip);
    addr4_.sin_port = htons(port);
  } else {
    memset(&addr6_, 0, sizeof addr6_);
    addr6_.sin6_family = AF_INET6;
    in6_addr ip = loopback_only ? in6addr_loopback : in6addr_any;
    addr6_.sin6_addr = ip;
    addr6_.sin6_port = htons(port);
  }
}

InetAddress::InetAddress(const std::string& addr, uint16_t port,
                         Family family) {
  if (family == kIpv4) {
    detail::FromAddrPort(addr.c_str(), port, &addr4_);
  } else {
    detail::FromAddrPort(addr.c_str(), port, &addr6_);
  }
}

std::string InetAddress::ToAddr() {
  char buf[64] = {0};
  return detail::ToAddr(GetSockAddr(), buf, sizeof(buf)) == 0 ? buf : "";
}

uint16_t InetAddress::ToPort() { return ntohs(addr4_.sin_port); }

std::string InetAddress::ToAddrPort() {
  char buf[64] = {0};
  return detail::ToAddrPort(GetSockAddr(), buf, sizeof(buf)) == 0 ? buf : "";
}
