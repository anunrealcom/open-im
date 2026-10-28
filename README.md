# 一幻IM（Qt6 / C++ 版）

原 Web/Electron 版（`../yihuan-im/`）的 **Qt6 Widgets + C++ 单体重写**。
每个桌面实例**自带服务端**，双击即用；局域网内其它实例通过 UDP 发现互相连接。

## 为什么用 Qt/C++ 而不是 Web

- 依赖收敛为单一技术栈（Qt6 + C++17），后期运维认知负担低。
- 服务端用 Qt 原生 `QTcpSocket` **自研最小 WebSocket 协议**（RFC6455 握手 + 帧编解码），
  不依赖 Qt6::WebSockets / Qt6::HttpServer（本机 Qt 未安装这些模块），零额外依赖。
- 文件传输走 WebSocket 二进制帧；AI / 机器人走 `QNetworkAccessManager` 出站。

## 技术栈

| 层 | 技术 |
|---|---|
| UI | Qt6 Widgets（纯 C++，无 QML/JS 引擎） |
| 网络 | Qt6::Network（`QTcpSocket` / `QUdpSocket` / `QNetworkAccessManager`） |
| 构建 | CMake + Ninja + MinGW 13.1（或 MSVC） |
| 持久化 | JSONL（消息历史，零依赖） |

## 已实现

- 文字消息收发、送达确认（chat-ack）、离线暂存
- 在线状态（presence 广播）、「正在输入」状态
- 图片/文件传输（WebSocket 二进制帧，自动保存到下载目录）
- AI 流式对话（OpenAI 兼容 SSE，多轮历史）
- 机器人框架（echo / webhook / agent 三种适配器）
- 局域网 UDP 发现与广播
- 配置热更新（服务端名称、AI Provider、机器人、局域网开关）
- 跨平台：Windows / macOS / Linux（CMake 一行切换工具链）

## 未实现（v1 范围外）

- **音视频通话**：原 Web 版依赖 WebRTC，Qt 端集成 libwebrtc 成本高，本期未做。
- 群聊：当前仅 1v1 + AI + Bot。
- 账号体系：登录仅填昵称，可冒名（无鉴权；配置里预留了 `auth` 字段）。

## 构建

需要：Qt 6.11+（含 `mingw_64` 套件）、CMake ≥ 3.16、Ninja、MinGW 或 MSVC。

```bash
# 以本机 Qt 安装为例
export PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/6.11.2/mingw_64/bin:$PATH"
cd yihuan-im-qt
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.2/mingw_64" -DQT_PREFIX="C:/Qt/6.11.2/mingw_64"
cmake --build build --target YiHuanIM
# 产物：build/YiHuanIM.exe（windeployqt 已自动收集运行时依赖）
```

不同机器把 `QT_PREFIX` 指向自己的 Qt6 安装目录即可（macOS/Linux 同理）。

## 测试

核心网络内核有无头自测（不依赖 GUI）：

```bash
cmake --build build --target yihuan_selftest
./build/yihuan_selftest.exe
```

覆盖：hello/welcome、在线列表、文本消息、送达确认、输入状态、机器人回显、文件二进制帧中继。

## 目录结构

```
src/net/      wsutil / wsserver      —— WebSocket 协议栈
src/core/     config/store/ai/bots/discovery/server —— 服务端核心
src/client/   connection/login/mainwindow/sidebar/chatwidget/messagebubble/settingsdialog
tests/        selftest.cpp           —— 无头自测
```
