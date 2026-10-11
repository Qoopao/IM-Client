#ifndef REMOTE_AUTH_DAO
#define REMOTE_AUTH_DAO

#include <QWebSocket>
#include <QTimer>
#include <QObject>
#include <functional>
#include <cstdint>

#include "../../model/loginResult.h"
#include "../../proto/sdkws.pb.h"


namespace dao {

class RemoteAuthDao : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(const model::LoginResult&)>;

    explicit RemoteAuthDao(QObject* parent = nullptr);
    ~RemoteAuthDao();

    void sendAuth(uint64_t account, const QString& password, Callback callback);
    void cancel();

private:
    QString findCertFile();
    void loadTrustAnchor();
    void genBin(uint64_t account, const QString& password);

private slots:
    void onConnected();
    void onDisconnected();
    void onAuthRespReceived(const QByteArray& msg);
    void onTimeout();
    void onSslErrors(const QList<QSslError>& errors);

private:
    void releaseSocket();
    model::LoginResult parseResp(const sdkws::SdkWSResp& resp);

private:
    QWebSocket* m_socket = nullptr;
    QTimer      m_timeoutTimer;

    std::string m_bin;                  // 二进制认证信息


    Callback    m_callback;          // 当前登录回调
    QString     m_pendingRequestId;  // 当前登录请求的 requestId


    static constexpr int kTimeoutMs = 8000;
    static constexpr const char* kWsUrl ="wss://127.0.0.1:6666";
};

} // namespace dao

#endif