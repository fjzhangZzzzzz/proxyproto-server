/**
 * @file proxyproto.cc
 * @author fangjun.zhang (fjzhang_@outlook.com)
 * @brief
 * @version 0.1
 * @date 2022-06-29
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "proxyproto.h"

#include <arpa/inet.h>  // inet_pton()

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>

static const char v2sig[12] = {
    0x0D, 0x0A, 0x0D, 0x0A, 0x00, 0x0D, 0x0A, 0x51, 0x55, 0x49, 0x54, 0x0A,
};

enum Error {
  kNeedMoreData = 0,
  kWrongProtocol = -1,
  kWrongDataSize = -2,
  kUnknownCommand = -3,
  kUnknownFamily = -4,
  kInvalidAddr = -6,
  kInvalidPort = -7,
};

union ProxyProtoHeader {
  struct {
    char line[108];
  } v1;
  struct {
    uint8_t sig[12];
    uint8_t ver_cmd;
    uint8_t fam;
    uint16_t len;
    union {
      struct { /* for TCP/UDP over IPv4, len = 12 */
        uint32_t src_addr;
        uint32_t dst_addr;
        uint16_t src_port;
        uint16_t dst_port;
      } ip4;
      struct { /* for TCP/UDP over IPv6, len = 36 */
        uint8_t src_addr[16];
        uint8_t dst_addr[16];
        uint16_t src_port;
        uint16_t dst_port;
      } ip6;
      struct { /* for AF_UNIX sockets, len = 216 */
        uint8_t src_addr[108];
        uint8_t dst_addr[108];
      } unx;
    } addr;
  } v2;
};

#define INVALID 1
#define TOOSMALL 2
#define TOOLARGE 3

static long long strtonum(const char* numstr, long long minval,
                          long long maxval, const char** errstrp) {
  long long ll = 0;
  char* ep;
  int error = 0;
  struct errval {
    const char* errstr;
    int err;
  } ev[4] = {
      {NULL, 0},
      {"invalid", EINVAL},
      {"too small", ERANGE},
      {"too large", ERANGE},
  };

  ev[0].err = errno;
  errno = 0;
  if (minval > maxval)
    error = INVALID;
  else {
    ll = strtoll(numstr, &ep, 10);
    if (numstr == ep || *ep != '\0')
      error = INVALID;
    else if ((ll == LLONG_MIN && errno == ERANGE) || ll < minval)
      error = TOOSMALL;
    else if ((ll == LLONG_MAX && errno == ERANGE) || ll > maxval)
      error = TOOLARGE;
  }
  if (errstrp != NULL) *errstrp = ev[error].errstr;
  errno = ev[error].err;
  if (error) ll = 0;

  return (ll);
}

static int DecodeV1(const char* data, size_t size, InetAddress* src,
                    InetAddress* dst) {
  const ProxyProtoHeader* hdr = reinterpret_cast<const ProxyProtoHeader*>(data);

  char* end = static_cast<char*>(memchr(hdr->v1.line, '\r', size - 1));
  if (end == nullptr || end[1] != '\n') {
    return kWrongProtocol;
  }
  *end = '\0';
  size = end + 2 - hdr->v1.line;

  union {
    struct sockaddr_in v4;
    struct sockaddr_in6 v6;
  } src_addr, dst_addr;
  memset(&src_addr, 0, sizeof(src_addr));
  memset(&dst_addr, 0, sizeof(dst_addr));

  /* PROXY TCP4 255.255.255.255 255.255.255.255 65535 65535
   * PROXY TCP6 ffff:f...f:ffff ffff:f...f:ffff 65535 65535
   * PROXY UNKNOWN
   * PROXY UNKNOWN ffff:f...f:ffff ffff:f...f:ffff 65535 65535
   */

  char* token;
  char* str = const_cast<char*>(hdr->v1.line);
  char* saveptr = nullptr;
  unsigned char buf[sizeof(struct in6_addr)] = {0};
  for (int i = 1;; i++, str = nullptr) {
    token = strtok_r(str, " ", &saveptr);
    if (token == nullptr) {
      return kWrongProtocol;
    }

    switch (i) {
        /* PROXY */
      case 1:
        continue;

        /* TCP4, TCP6, UNKNOWN */
      case 2:
        if (strcmp(token, "TCP4") == 0) {
          src_addr.v4.sin_family = AF_INET;
          dst_addr.v4.sin_family = AF_INET;
        } else if (strcmp(token, "TCP6") == 0) {
          src_addr.v6.sin6_family = AF_INET6;
          dst_addr.v6.sin6_family = AF_INET6;
        } else {
          return kUnknownFamily;
        }
        break;

        /* source address */
      case 3: {
        struct sockaddr* sa = reinterpret_cast<struct sockaddr*>(&src_addr);
        if (inet_pton(sa->sa_family, token, buf) != 1) {
          return kInvalidAddr;
        }
        if (sa->sa_family == AF_INET) {
          src_addr.v4.sin_addr.s_addr =
              reinterpret_cast<struct in_addr*>(buf)->s_addr;
        } else if (sa->sa_family == AF_INET6) {
          memcpy(&src_addr.v6.sin6_addr, buf, 16);
        }
      } break;

        /* destination address */
      case 4: {
        struct sockaddr* sa = reinterpret_cast<struct sockaddr*>(&dst_addr);
        if (inet_pton(sa->sa_family, token, buf) != 1) {
          return kInvalidAddr;
        }
        if (sa->sa_family == AF_INET) {
          dst_addr.v4.sin_addr.s_addr =
              reinterpret_cast<struct in_addr*>(buf)->s_addr;
        } else if (sa->sa_family == AF_INET6) {
          memcpy(&dst_addr.v6.sin6_addr, buf, 16);
        }
      } break;

        /* source port */
      case 5: {
        if (!isdigit((unsigned char)token[0])) return kInvalidPort;

        const char* errstr = nullptr;
        uint16_t port = (uint16_t)strtonum(token, 0, UINT16_MAX, &errstr);
        if (errstr != NULL) return kInvalidPort;

        struct sockaddr* sa = reinterpret_cast<struct sockaddr*>(&src_addr);
        if (sa->sa_family == AF_INET) {
          src_addr.v4.sin_port = htons(port);
        } else if (sa->sa_family == AF_INET6) {
          src_addr.v6.sin6_port = htons(port);
        }
      } break;

        /* destination port */
      case 6: {
        if (!isdigit((unsigned char)token[0])) return kInvalidPort;

        const char* errstr = nullptr;
        uint16_t port = (uint16_t)strtonum(token, 0, UINT16_MAX, &errstr);
        if (errstr != NULL) return kInvalidPort;

        struct sockaddr* sa = reinterpret_cast<struct sockaddr*>(&dst_addr);
        if (sa->sa_family == AF_INET) {
          dst_addr.v4.sin_port = htons(port);

          src->set_addr4(src_addr.v4);
          dst->set_addr4(dst_addr.v4);
        } else if (sa->sa_family == AF_INET6) {
          dst_addr.v6.sin6_port = htons(port);

          src->set_addr6(src_addr.v6);
          dst->set_addr6(dst_addr.v6);
        }
        return static_cast<int>(size);
      }

      default:
        return kWrongProtocol;
    }
  }

  return static_cast<int>(size);
}

static int DecodeV2(const char* data, size_t size, InetAddress* src,
                    InetAddress* dst) {
  const ProxyProtoHeader* hdr = reinterpret_cast<const ProxyProtoHeader*>(data);

  size_t n = 16 + ntohs(hdr->v2.len);
  if (size < n) {
    return kWrongDataSize;
  }

  switch (hdr->v2.ver_cmd & 0xF) {
    /* PROXY command */
    case 0x01:
      switch (hdr->v2.fam) {
        /* TCPv4 */
        case 0x11: {
          struct sockaddr_in addr;
          memset(&addr, 0, sizeof(addr));
          addr.sin_family = AF_INET;
          addr.sin_addr.s_addr = hdr->v2.addr.ip4.src_addr;
          addr.sin_port = hdr->v2.addr.ip4.src_port;
          src->set_addr4(addr);

          addr.sin_family = AF_INET;
          addr.sin_addr.s_addr = hdr->v2.addr.ip4.dst_addr;
          addr.sin_port = hdr->v2.addr.ip4.dst_port;
          dst->set_addr4(addr);
        } break;

          /* TCPv6 */
        case 0x21: {
          struct sockaddr_in6 addr;
          memset(&addr, 0, sizeof(addr));
          addr.sin6_family = AF_INET6;
          memcpy(&addr.sin6_addr, hdr->v2.addr.ip6.src_addr, 16);
          addr.sin6_port = hdr->v2.addr.ip6.src_port;
          src->set_addr6(addr);

          addr.sin6_family = AF_INET6;
          memcpy(&addr.sin6_addr, hdr->v2.addr.ip6.dst_addr, 16);
          addr.sin6_port = hdr->v2.addr.ip6.dst_port;
          dst->set_addr6(addr);
        } break;

        default:
          return kUnknownFamily;
      }
      break;

    case 0x00: /* LOCAL command */
    default:
      return kUnknownCommand; /* not a supported command */
  }

  return static_cast<int>(n);
}

int DecodeProxyProto(const char* data, size_t size, InetAddress* src,
                     InetAddress* dst) {
  const ProxyProtoHeader* hdr = reinterpret_cast<const ProxyProtoHeader*>(data);
  if (size >= 16 && memcmp(&hdr->v2, v2sig, sizeof(v2sig)) == 0 &&
      (hdr->v2.ver_cmd & 0xF0) == 0x20) {
    return DecodeV2(data, size, src, dst);
  } else if (size >= 8 && memcmp(hdr->v1.line, "PROXY", 5) == 0) {
    return DecodeV1(data, size, src, dst);
  } else {
    if (size >= 16)
      return kWrongProtocol;
    else if (memcmp(&hdr->v2, v2sig, std::min(sizeof(v2sig), size)) == 0)
      return kNeedMoreData;

    if (size >= 8)
      return kWrongProtocol;
    else if (memcmp(hdr->v1.line, "PROXY",
                    std::min(static_cast<size_t>(5), size)) == 0)
      return kNeedMoreData;

    return kWrongProtocol;
  }
}
