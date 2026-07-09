## 构建

### 要求

- Linux kernel >= 3.9
- C11 支持；C++ 模块需 C++20（协程）
- CMake 3.22.0+

### 支持 SSL

dpapp 建议使用 BoringSSL 提供 SSL 支持。

> 在不需要 QUIC 支持时，仍可使用 OpenSSL。

1. 拉取代码

```
git clone https://boringssl.googlesource.com/boringssl
cd boringssl
```

2. 编译

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=1 -DCMAKE_INSTALL_PREFIX=/opt/dpbox/usr ../
make -j 8 && make install
```

### 支持 QUIC

1. 安装依赖（Ubuntu 24.04）

```
sudo apt install -y zlib1g-dev
```

2. 拉取代码

```
git clone https://github.com/litespeedtech/lsquic.git
cd lsquic
git submodule update --init

```

3. 编译

```
mkdir build
cd build
cmake -DLSQUIC_SHARED_LIB=1 -DLIBSSL_DIR=/opt/dpbox/usr -DCMAKE_INSTALL_PREFIX=/opt/dpbox/usr -DCMAKE_BUILD_TYPE=Release -DLSQUIC_LIBSSL=BORINGSSL ../
make -j8 && make install
```

### 编译 dpapp

```shell
mkdir build
cd build
cmake ../ -DCMAKE_BUILD_TYPE=MinSizeRel -DDPAPP_WITH_LSQUIC=ON
make -j8 && make install
```

默认安装到 `/opt/dpbox`；可 `-DCMAKE_INSTALL_PREFIX=<path>` 覆盖。

可选功能开关：

| 选项 | 默认 | 说明 |
|------|------|------|
| `DPAPP_WITH_SSL` | ON | OpenSSL；`DPAPP_WITH_LSQUIC=ON` 时强制 ON |
| `DPAPP_WITH_LSQUIC` | OFF | QUIC/HTTP3（需 SSL + lsquic） |
| `DPAPP_WITH_CWC` | ON | dpaco + dpcwc + C 示例 |
| `DPAPP_WITH_CPP` | ON | dpcpp + C++ 示例 |
| `DPAPP_WITH_LUA` | ON | dplua + LuaJIT |
| `DPAPP_COMPILE_LUA` | ON | Lua 脚本编译为字节码（`luajit -b`）；依赖 `DPAPP_WITH_LUA` |
| `DPAPP_WITH_TESTS` | ON | 测试用例 |
| `DPAPP_WITH_DOCS` | ON | Doxygen HTML（需 `doxygen`） |


安装生成的 `dpapp_config.h` 定义 `DPAPP_*_ENABLE` 与 `DPAPP_HAS_*` 宏（如 `DPAPP_HAS_LUA`），供 `#if` 条件编译。

### 安装布局

编译产物在 `build/stage/`，`make install` 后同步至 `/opt/dpbox`（或自定义 `CMAKE_INSTALL_PREFIX`），二者布局相同：

```
/opt/dpbox/
├── bin/dpapp              # 入口（链接 usr/bin/dpapp）
├── usr/lib/libdp*.so
├── lua/dplua/
├── app/example/
└── var/
```

安装完成后 `cd /opt/dpbox` 运行，详见 [start.md](start.md)。仅编译未安装时，将路径换为 `build/stage` 即可。

### 运行测试

在构建目录中执行 CTest（需已配置 `DPAPP_WITH_TESTS=ON`）：

```shell
cd build
ctest --output-on-failure
```
