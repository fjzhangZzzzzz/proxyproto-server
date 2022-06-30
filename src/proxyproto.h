/**
 * @file proxyproto.h
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

#include <string>

#include "inet_address.h"

/**
 * @brief 解析代理协议
 *
 * @param data 输入数据
 * @param size 输入数据长度
 * @param src 输出的来源地址信息
 * @param dst 输出的目的地址信息
 * @return int 负值表示错误，0表示数据不足，正值表示解析成功，值为代理数据长度
 */
int DecodeProxyProto(const char* data, size_t size, InetAddress* src,
                     InetAddress* dst);
