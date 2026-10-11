# WCDB 自定义 vcpkg port

项目通过 `vcpkg-configuration.json` 的 `overlay-ports` 使用本目录，
并在 `vcpkg.json` 中声明 `wcdb`。无需修改全局 `C:/vcpkg/ports`。

## 构建

使用现有 Qt Creator 的 **MSVC 2022 64-bit** Kit 重新运行 CMake，再构建项目。
vcpkg 会自动安装 WCDB 的 Debug 和 Release 库，默认 triplet 为 `x64-windows`。
首次安装需要访问 GitHub 下载源码。

依赖统一安装到项目根目录的 `vcpkg_installed/x64-windows/`，由 Debug 和
Release 构建目录共享。删除 `build` 不会删除依赖；重新运行 CMake 会更新
已有构建目录缓存中的安装路径。`vcpkg_installed/` 已加入 Git 忽略规则。

也可以在 VS x64 开发者命令行中执行（Qt 路径按本机调整）：

```powershell
cmake -S . -B build/wcdb -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/msvc2022_64 `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build/wcdb
```

项目沿用原来的 `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`；
其他机器可以通过 `-DCMAKE_TOOLCHAIN_FILE=...` 覆盖。
vcpkg 的 Windows app-local 部署会复制链接的 DLL；发布时仍需部署 Qt 运行库。

## C++ 使用

```cmake
find_package(WCDB CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE WCDB::WCDB)
```

```cpp
#include <WCDB/WCDBCpp.h>

WCDB::Database database("messages.db");
bool opened = database.canOpen();
```

当前接入仅增加依赖和链接，登录、注册 DAO 尚未迁移到本地数据库。

## 版本与适配

- WCDB 固定为 `v2.1.16`，源码下载通过 SHA512 校验。
- SQLCipher 固定为此版本的子模块提交
  `f049bed66ca26741f09a6e4f0603ed3af195ac96`，单独下载并校验。
  WCDB 需要 Tencent 分支的 SQLite hooks，不能直接替换成普通 SQLCipher。
- OpenSSL、zlib、zstd 使用 vcpkg 包，替换上游预编译依赖。
- `vcpkg-build.patch` 添加公开头文件安装、CMake package 导出，并适配依赖路径。
- `sqlcipher-runtime.patch` 移除固定 `/MD`、`/MT` 参数，由 vcpkg 管理 CRT。
- 当前 port 仅支持 Windows/MSVC 动态库 triplet；不支持 MinGW 或静态库 triplet。

升级 WCDB 时，需要同时核对 SQLCipher 子模块提交、源码哈希和两份补丁。

上游说明：https://github.com/Tencent/wcdb/wiki/C%2B%2B-%E5%AE%89%E8%A3%85%E4%B8%8E%E5%85%BC%E5%AE%B9%E6%80%A7
