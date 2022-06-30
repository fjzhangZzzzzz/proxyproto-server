/**
 * @file server.cc
 * @author fangjun.zhang (fjzhang_@outlook.com)
 * @brief
 * @version 0.1
 * @date 2022-06-29
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>
#include <utility>

#include "inet_address.h"
#include "logging.h"
#include "proxyproto.h"

const size_t Server::kInitialEventsNum = 4;
const size_t Server::kMaxEventsNum = 20;
const int Server::kNoneEvent = 0;
const int Server::kReadEvent = POLLIN | POLLPRI;
const int Server::kWriteEvent = POLLOUT;
const size_t Server::kMaxConnNum = 1024;

static void Close(int& fd) {
  if (fd != -1) {
    LOGD("close fd %d", fd);
    close(fd);
    fd = -1;
  }
}

static const char* Operation2String(int operation) {
  switch (operation) {
    case EPOLL_CTL_ADD:
      return "EPOLL_CTL_ADD";
    case EPOLL_CTL_DEL:
      return "EPOLL_CTL_DEL";
    case EPOLL_CTL_MOD:
      return "EPOLL_CTL_MOD";

    default:
      return "UNKNOWN";
  }
}

static size_t GetSteadyTime() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

Server::Conn::~Conn() { Close(sockfd); }

Server::Server(std::shared_ptr<Conf> conf)
    : conf_{std::move(conf)},
      epoll_fd_(-1),
      listen_sockfd_{-1},
      conn_index_{0},
      active_events_{kInitialEventsNum} {}

Server::~Server() { Stop(); }

int Server::Start() {
  int err = 0;
  do {
    err = Stop();
    if (err != 0) {
      err = -1;
      break;
    }

    epoll_fd_ = epoll_create(1024);
    if (epoll_fd_ == -1) {
      err = -2;
      break;
    }

    listen_sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sockfd_ == -1) {
      err = -3;
      break;
    }

    int flags = fcntl(listen_sockfd_, F_GETFL, 0);
    flags |= (O_NONBLOCK | FD_CLOEXEC);
    do {
      err = fcntl(listen_sockfd_, F_SETFL, flags);
    } while (err == -1 && errno == EINTR);

    if (err != 0) {
      err = -4;
      break;
    }

    int reuse = 1;
    err = setsockopt(listen_sockfd_, SOL_SOCKET, SO_REUSEADDR, &reuse,
                     sizeof(reuse));
    if (err != 0) {
      err = -5;
      break;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(conf_->listen_port);
    err = bind(listen_sockfd_, reinterpret_cast<struct sockaddr*>(&addr),
               sizeof(addr));
    if (err != 0) {
      err = -6;
      break;
    }

    err = listen(listen_sockfd_, SOMAXCONN);
    if (err != 0) {
      err = -7;
      break;
    }

    Update(EPOLL_CTL_ADD, listen_sockfd_, kReadEvent, this);
  } while (0);

  if (err != 0) {
    Stop();
  }
  return err;
}

int Server::Stop() {
  Close(listen_sockfd_);
  Close(epoll_fd_);
  return 0;
}

int Server::Poll(int timeout) {
  int num_events = epoll_wait(epoll_fd_, &*active_events_.begin(),
                              static_cast<int>(active_events_.size()), timeout);
  if (num_events > 0) {
    for (int i = 0; i < num_events; ++i) {
      HandleEvents(active_events_[i].events, active_events_[i].data.ptr);
    }

    if (static_cast<size_t>(num_events) == active_events_.size() &&
        active_events_.size() < kMaxEventsNum) {
      active_events_.resize(std::min(active_events_.size() * 2, kMaxEventsNum));
    }
  } else if (num_events == 0) {
    // nothing happened
  } else {
    if (errno != EINTR) {
      LOGE("epoll_wait err %s", strerror(errno));
    }
  }
  return 0;
}

void Server::Update(int operation, int sockfd, int events, void* userp) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = events;
  event.data.ptr = userp;
  if (epoll_ctl(epoll_fd_, operation, sockfd, &event) < 0) {
    LOGE("epoll_ctl op=%s fd=%d err %s", Operation2String(operation), sockfd,
         strerror(errno));
  }
}

void Server::Update(Conn* conn) {
  Update(EPOLL_CTL_MOD, conn->sockfd, conn->watch_events, conn);
}

void Server::EnableReading(Conn* conn) {
  conn->watch_events |= kReadEvent;
  Update(conn);
}
void Server::EnableWriting(Conn* conn) {
  conn->watch_events |= kWriteEvent;
  Update(conn);
}

void Server::DisableReading(Conn* conn) {
  conn->watch_events &= ~kReadEvent;
  Update(conn);
}

void Server::DisableWriting(Conn* conn) {
  conn->watch_events &= ~kWriteEvent;
  Update(conn);
}

void Server::HandleEvents(int events, void* userp) {
  if (userp == this) {
    OnNewConn(events);
  } else {
    Conn* conn = reinterpret_cast<Conn*>(userp);
    auto iter = conns_.find(conn);
    if (iter != conns_.end()) {
      OnConnEvt(conn, events);
      if (conn->state == kDisconnected) {
        LOGI("remove %s", conn->cname());
        Update(EPOLL_CTL_DEL, conn->sockfd, conn->watch_events, conn);
        conns_.erase(conn);
      }
    }
  }
}

void Server::OnNewConn(int events) {
  if (events & (POLLIN | POLLPRI | POLLRDHUP)) {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int sockfd = accept(listen_sockfd_,
                        reinterpret_cast<struct sockaddr*>(&addr), &addrlen);
    if (sockfd != -1) {
      LOGD("accept new sockfd %d", sockfd);

      if (conns_.size() >= kMaxConnNum) {
        Close(sockfd);
        LOGI("the number of connections exceeds the limit");
        return;
      }

      std::unique_ptr<Conn> conn(new Conn);
      conn->sockfd = sockfd;
      conn->state = kConnected;
      conn->watch_events = kReadEvent;
      conn->conn_time = GetSteadyTime();

      std::ostringstream oss;
      oss << "conn#" << conn_index_++ << "-" << sockfd << "-"
          << conn->conn_time;
      conn->name = oss.str();

      Update(EPOLL_CTL_ADD, sockfd, kReadEvent, conn.get());

      LOGI("add new conn [%s]", conn->cname());
      conns_.insert(std::make_pair(conn.get(), std::move(conn)));
    } else {
      LOGE("accept err %s", strerror(errno));
    }
  }
}

void Server::OnConnEvt(Conn* conn, int events) {
  if ((events & POLLHUP) && !(events & POLLIN)) {
    // close
    conn->state = kDisconnected;
    LOGI("%s close", conn->cname());
  }

  if (events & (POLLERR | POLLNVAL)) {
    // error
    conn->state = kDisconnected;
    LOGI("%s error", conn->cname());
  }

  if (events & (POLLIN | POLLPRI | POLLRDHUP)) {
    // readable
    char buf[1024];
    ssize_t n = recv(conn->sockfd, buf, sizeof(buf), 0);
    if (n > 0) {
      conn->ibuf.append(buf, n);

      InetAddress src, dst;
      int ret =
          DecodeProxyProto(conn->ibuf.data(), conn->ibuf.size(), &src, &dst);
      if (ret > 0) {
        LOGI("%s proxy: %s -> %s", conn->cname(), src.ToAddrPort().c_str(),
             dst.ToAddrPort().c_str());
        conn->state = kDisconnected;
      } else if (ret == 0) {
        // continue
      } else {
        LOGW("%s decode proxy proto err %d", conn->cname(), ret);
      }
    } else if (n == 0) {
      conn->state = kDisconnected;
      LOGI("%s closed by peer", conn->cname());
    } else {
      LOGW("%s recv err %s", conn->cname(), strerror(errno));
    }
  }

  if (events & POLLOUT) {
    // writable
    if (!conn->obuf.empty()) {
      //
    }

    if (conn->obuf.empty()) {
      DisableWriting(conn);
    }
  }
}
