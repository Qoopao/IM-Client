# 项目目录执行

$protos = (Get-ChildItem .\proto\*.proto).Name | ForEach-Object { "proto\$_" }

C:\vcpkg\installed\x64-windows\tools\protobuf\protoc.exe `
  -I .\proto `
  --cpp_out=./src/proto `
  --grpc_out=./src/proto `
  --plugin=protoc-gen-grpc=C:/vcpkg/installed/x64-windows/tools/grpc/grpc_cpp_plugin.exe `
  $protos