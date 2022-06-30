/**
 * @file logging.cc
 * @author fangjun.zhang (fjzhang_@outlook.com)
 * @brief
 * @version 0.1
 * @date 2022-06-29
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "logging.h"

#include <sys/types.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

static int g_log_level = LOG_LEVEL_INFO;

static const char* g_log_level_string[] = {"D", "I", "W", "E"};

#ifndef NDEBUG
static const char* GetBasename(const char* path) {
  char* p1 = strrchr(const_cast<char*>(path), '/');
  char* p2 = strrchr(const_cast<char*>(path), '\\');
  return p1 ? p1 + 1 : (p2 ? p2 + 1 : path);
}
#endif

void SetLogLevel(int lv) {
  if (lv >= LOG_LEVEL_DEBUG && lv <= LOG_LEVEL_ERROR) {
    g_log_level = lv;
  }
}

void Log(int lv, const char* file, const int line, const char* func,
         const char* fmt, ...) {
  if (lv < g_log_level) return;

  char timebuf[32];
  time_t now = time(nullptr);
#ifdef WIN32
  struct tm now_tm;
  memset(&now_tm, 0, sizeof(now_tm));
  (void)localtime_s(&now_tm, &now);
  strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &now_tm);
#else
  strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
#endif

  va_list va;
  va_start(va, fmt);
#ifdef NDEBUG
  fprintf(stdout, "%s [%s] ", timebuf,
          (lv >= LOG_LEVEL_DEBUG && lv <= LOG_LEVEL_ERROR)
              ? g_log_level_string[lv]
              : "U");
#else
  fprintf(stdout, "%s [%s:%d:%s] [%s] ", timebuf, GetBasename(file), line, func,
          (lv >= LOG_LEVEL_DEBUG && lv <= LOG_LEVEL_ERROR)
              ? g_log_level_string[lv]
              : "U");
#endif
  vfprintf(stdout, fmt, va);
  fprintf(stdout, "\n");
  va_end(va);
}
