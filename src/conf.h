/**
 * @file conf.h
 * @author fangjun.zhang (fjzhang_@outlook.com)
 * @brief
 * @version 0.1
 * @date 2022-06-29
 *
 * @copyright Copyright (c) 2022
 *
 */

#pragma once

struct Conf {
  int listen_port;
  int log_level;
};

int LoadConf(int argc, char** argv, Conf* conf);
int ShowHelp(int argc, char** argv);
