/**
 * @file conf.cc
 * @author fangjun.zhang (fjzhang_@outlook.com)
 * @brief
 * @version 0.1
 * @date 2022-06-29
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "conf.h"

#include <getopt.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#define OPTIND_LISTEN_PORT 0x1
#define OPTIND_LOG_LEVEL 0x2

int ShowHelp(int argc, char** argv) {
  static struct {
    const char* option;
    const char* desc;
  } info[] = {
      {"--listen-port=PORT", "set listen port"},
      {"--log-level=LEVEL", "set log level, 0-debug,1-info,2-warn,3-error"},
  };
  static int size = sizeof(info) / sizeof(info[0]);

  int maxlen = 0;
  for (int i = 0; i < size; ++i) {
    int len = strlen(info[i].option);
    maxlen = std::max(maxlen, len);
  }

  char fmt[32] = {0};
  snprintf(fmt, sizeof(fmt), "  %%-%ds  %%s\n", maxlen);

  fprintf(stdout, "Usage: %s [OPTION]...\n\n", argv[0]);
  for (int i = 0; i < size; ++i) {
    fprintf(stdout, fmt, info[i].option, info[i].desc);
  }
}

int LoadConf(int argc, char** argv, Conf* conf) {
  static struct option long_options[] = {
      {"listen-port", required_argument, nullptr, OPTIND_LISTEN_PORT},
      {"log-level", required_argument, nullptr, OPTIND_LOG_LEVEL},
      {0, 0, 0, 0},
  };

  if (conf == nullptr) return -1;

  int required_mask = OPTIND_LISTEN_PORT;
  int opt;
  while ((opt = getopt_long(argc, argv, "", long_options, nullptr)) != -1) {
    switch (opt) {
      case OPTIND_LISTEN_PORT:
        required_mask &= ~opt;
        conf->listen_port = atoi(optarg);
        break;
      case OPTIND_LOG_LEVEL:
        required_mask &= ~opt;
        conf->log_level = atoi(optarg);
        break;
      default:
        return -2;
    }
  }

  if (conf->listen_port <= 0) {
    return -3;
  }

  if (conf->log_level < 0 || conf->log_level > 3) {
    return -4;
  }

  return required_mask == 0 ? 0 : -5;
}
