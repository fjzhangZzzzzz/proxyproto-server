/**
 * @file server.h
 * @author fangjun.zhang (fjzhang_@outlook.com)
 * @brief
 * @version 0.1
 * @date 2022-06-29
 *
 * @copyright Copyright (c) 2022
 *
 */

#pragma once

#include <stdint.h>
#include <sys/epoll.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "conf.h"

class Server {
  enum ConnState {
    kDisconnected,
    kConnected,
  };

  struct Conn {
    std::string name;
    int state;
    int sockfd;
    int watch_events;
    size_t conn_time;
    std::string ibuf;
    std::string obuf;

    Conn()
        : state(kDisconnected),
          sockfd(-1),
          watch_events(kNoneEvent),
          conn_time(0) {}
    ~Conn();
    const char* cname() const { return name.c_str(); }
  };

 public:
  explicit Server(std::shared_ptr<Conf> conf);
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  int Start();
  int Stop();
  int Poll(int timeout);

 private:
  void Update(int operation, int sockfd, int events, void* userp);
  void Update(Conn* conn);
  void EnableReading(Conn* conn);
  void EnableWriting(Conn* conn);
  void DisableReading(Conn* conn);
  void DisableWriting(Conn* conn);
  void HandleEvents(int events, void* userp);
  void OnNewConn(int events);
  void OnConnEvt(Conn* conn, int events);

 private:
  static const size_t kInitialEventsNum;
  static const size_t kMaxEventsNum;
  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;
  static const size_t kMaxConnNum;

  std::shared_ptr<Conf> conf_;
  int epoll_fd_;
  int listen_sockfd_;
  uint32_t conn_index_;
  std::vector<struct epoll_event> active_events_;
  std::map<Conn*, std::unique_ptr<Conn>> conns_;
};
