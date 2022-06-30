/**
 * @file logging.h
 * @author fangjun.zhang (fjzhang_@outlook.com)
 * @brief
 * @version 0.1
 * @date 2022-06-29
 *
 * @copyright Copyright (c) 2022
 *
 */

#pragma once

enum LogLevel {
  LOG_LEVEL_DEBUG = 0,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR,
};

void SetLogLevel(int lv);
void Log(int lv, const char* file, const int line, const char* func,
         const char* fmt, ...);

#define LOGD(fmt, ...) \
  Log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define LOGI(fmt, ...) \
  Log(LOG_LEVEL_INFO, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define LOGW(fmt, ...) \
  Log(LOG_LEVEL_WARN, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define LOGE(fmt, ...) \
  Log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
