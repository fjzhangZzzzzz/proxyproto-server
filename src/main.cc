/**
 * @file main.cc
 * @author fangjun.zhang (fjzhang_@outlook.com)
 * @brief
 * @version 0.1
 * @date 2022-06-29
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <signal.h>

#include <memory>

#include "conf.h"
#include "logging.h"
#include "server.h"

bool g_exit = false;
void OnSigal(int signum) { g_exit = true; }

int main(int argc, char** argv) {
  auto conf = std::make_shared<Conf>();
#ifdef NDEBUG
  conf->log_level = LOG_LEVEL_INFO;
#else
  conf->log_level = LOG_LEVEL_DEBUG;
#endif
  if (LoadConf(argc, argv, conf.get()) != 0) {
    ShowHelp(argc, argv);
    return 1;
  }

  SetLogLevel(conf->log_level);

  auto server = std::make_shared<Server>(conf);
  if (server->Start() == 0) {
    LOGI("server start at port %d", conf->listen_port);
    signal(SIGINT, OnSigal);
    while (!g_exit) {
      server->Poll(1000);
    }
    LOGI("server stop");
  }
  return 0;
}
