#ifndef WEB_SOCKET_SESSION_H
#define WEB_SOCKET_SESSION_H

#include <QObject>
#include <QTimer>
#include <QWebSocket>
#include <cstdint>

namespace network {

class WebSocketSession : public QObject
{
    Q_OBJECT

public:
    explicit WebSocketSession(QObject* parent = nullptr);
    ~WebSocketSession() override;

    void open(uint64_t account);
    void close();
    void setAuthenticated(const QString& token);
    bool sendRequest(int type, const QByteArray& data, const QString& requestId = {});
    bool isOnline() const;

signals:
    void connected();
    void responseReceived(const QByteArray& frame);
    void connectionLost();
    void connectionError(const QString& error);

private:
    void onDisconnected();
    void onFrameReceived(const QByteArray& frame);
    void failConnection(const QString& error);
    void loadTrustAnchor();
    QString findCertFile() const;

    QWebSocket* m_socket = nullptr;
    QTimer m_heartbeatTimer;
    uint64_t m_account = 0;
    QString m_token;
    bool m_authenticated = false;
    bool m_waitingPong = false;

    static constexpr const char* kWsUrl = "wss://127.0.0.1:6666";
};

}

#endif // WEB_SOCKET_SESSION_H
