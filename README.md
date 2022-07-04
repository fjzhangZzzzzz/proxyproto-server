# proxyproto-server
用于自测代理协议的 demo server

## 构建

### 要求

- Linux
- CMake 2.6+
- gcc-4.9.4+

### CMake

```bash
git clone https://github.com/fjzhangZzzzzz/proxyproto-server.git
mkdir proxyproto-server/build && cd proxyproto-server/build
cmake -DCMAKE_BUILD_TYPE=Release .. && make
```

## 使用

```bash
$ ./proxyproto-server
Usage: ./proxyproto-server [OPTION]...

  --listen-port=PORT  set listen port
  --log-level=LEVEL   set log level, 0-debug,1-info,2-warn,3-error

$ ./proxyproto-server --listen-port=8889
2022-07-01 11:18:42 [I] server start at port 8889
2022-07-01 11:18:51 [I] add conn [conn#0-5-694011]
2022-07-01 11:18:51 [I] conn#0-5-694011 proxy: 192.168.136.146:35608 -> 192.168.136.152:8001
2022-07-01 11:18:51 [I] del conn [conn#0-5-694011]
# <Ctrl+C> to exit
^C2022-07-01 11:18:51 [I] server stop
```
