# BitRPC

BitRPC 是一个基于 C++11、Muduo 和 JSONCpp 实现的轻量级 RPC 框架。项目封装了消息协议、连接管理、请求分发和服务治理能力，支持直接 RPC 调用、服务注册与发现以及 Topic 发布订阅。

## 功能

- 基于长度字段的消息帧协议，处理 TCP 粘包与拆包
- JSON 请求、响应序列化与消息类型校验
- 同步调用、`std::future` 异步调用和回调调用
- RPC 方法注册、参数校验与请求路由
- 服务注册、发现及上下线通知
- Topic 创建、删除、订阅、取消订阅与消息发布
- 基于 Muduo Reactor 模型的 TCP 客户端和服务端封装

## 目录结构

```text
source/
├── common/      # 消息协议、网络抽象、序列化和请求分发
├── client/      # RPC、服务发现和 Topic 客户端
├── server/      # RPC、注册中心和 Topic 服务端
└── test/        # 可运行的集成示例
    ├── test1/   # 客户端直连 RPC 服务
    ├── test2/   # 注册中心与服务发现
    └── test3/   # Topic 发布订阅
```

`build/` 中的预编译 Muduo 文件和 `third/` 中的源码压缩包属于本地依赖，不建议提交到 Git 仓库。

## 环境依赖

- Linux
- 支持 C++11 的 GCC
- Muduo
- JSONCpp
- pthread

如果 Muduo 安装在自定义目录，可在编译时指定 `MUDUO_ROOT`：

```bash
make MUDUO_ROOT=/path/to/muduo-install
```

未指定 `MUDUO_ROOT` 时，编译器和链接器会从系统默认路径查找 Muduo。

## 运行示例

### 直接 RPC 调用

```bash
cd source/test/test1
make
./server
./client
```

### 服务注册与发现

分别启动注册中心、RPC 服务端和客户端：

```bash
cd source/test/test2
make
./reg_server
./rpc_server
./rpc_client
```

### 发布订阅

分别启动 Topic 服务端、订阅端和发布端：

```bash
cd source/test/test3
make
./server
./subscribe_client
./publish_client
```

## 说明

当前实现用于展示 RPC 核心机制，尚未包含生产环境通常需要的鉴权、限流、配置中心、链路追踪、持久化和完善的故障恢复能力。

## 版权说明

原项目 README 声明该项目内容归“比特就业课”开发者或授权方所有，未经明确授权不得商业使用、修改、复制或传播。公开 fork、修改或重新发布本项目之前，请先确认已经获得版权所有方许可，并在仓库中保留来源和授权信息。
