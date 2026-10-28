#pragma once
#include <QObject>
#include <QByteArray>
#include <QString>

// 最小 WebSocket 协议工具（RFC6455 子集）：握手 + 帧编解码。
// 服务端接收客户端帧需解掩码；客户端发送需加掩码。
namespace ws {

enum Opcode : quint8 {
    OpContinuation = 0x0,
    OpText         = 0x1,
    OpBinary       = 0x2,
    OpClose        = 0x8,
    OpPing         = 0x9,
    OpPong         = 0xA
};

// 计算 Sec-WebSocket-Accept
QString acceptKey(const QString& clientKey);

// 生成服务端握手响应
QByteArray serverHandshake(const QString& clientKey);

// 生成客户端握手请求
QByteArray clientHandshake(const QString& host, quint16 port, const QString& path = "/ws");

// 从服务端响应中解析出 Sec-WebSocket-Accept（用于客户端校验）
QString parseAcceptFromResponse(const QByteArray& response);

// 编码一帧。mask=true 时按客户端规范加掩码。
QByteArray encodeFrame(Opcode op, const QByteArray& payload, bool mask = false);

// 增量帧解析器：喂入原始字节，信号输出完整消息。
class FrameParser : public QObject {
    Q_OBJECT
public:
    explicit FrameParser(QObject* parent = nullptr);
    void feed(const QByteArray& data);

signals:
    void textMessage(const QString& text);
    void binaryMessage(const QByteArray& data);
    void ping(const QByteArray& data);
    void closed();

private:
    enum State { S_Opcode, S_Length, S_Mask, S_Payload };

    State state_ = S_Opcode;
    bool  fin_ = true;
    Opcode opcode_ = OpText;
    quint64 payloadLen_ = 0;
    int  lenBytes_ = 0;          // 还需读取的扩展长度字节数
    bool needMask_ = false;
    QByteArray maskKey_;
    quint8 maskIdx_ = 0;
    QByteArray payload_;
    QByteArray pending_;

    void reset();
    void emitMessage();
};

} // namespace ws
