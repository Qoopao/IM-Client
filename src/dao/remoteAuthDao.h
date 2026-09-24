#ifndef REMOTE_AUTH_DAO
#define REMOTE_AUTH_DAO

#include <QWebSocket>
#include <QTimer>
#include <QObject>
#include <functional>
#include <cstdint>

#include "../model/loginResult.h"
#include "../proto/sdkws.pb.h"


namespace dao {

class RemoteAuthDao : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(const model::LoginResult&)>;

    explicit RemoteAuthDao(QObject* parent = nullptr);
    ~RemoteAuthDao() = default;

    void sendAuth(uint64_t account, const QString& password, Callback callback);
    void cancel();

    QString findCertFile();
    void loadTrustAnchor();

private slots:
    void onConnected();
    void onDisconnected();
    void onAuthRespReceived(const QByteArray& msg);
    void onTimeout();
    void onSslErrors(const QList<QSslError>& errors);

private:
    void sendReq(const sdkws::SdkWSReq& req);
    model::LoginResult parseResp(const sdkws::SdkWSResp& resp);

private:
    QWebSocket* m_socket = nullptr;
    QTimer      m_timeoutTimer;

    uint64_t    m_account = 0;       // 当前登录账号
    QString     m_token;             // 认证成功后保存，供后续请求用

    Callback    m_callback;          // 当前登录回调
    QString     m_pendingRequestId;  // 当前登录请求的 requestId

    bool        m_timedOut = false;
    bool        m_hadSslError = false;
    QString     m_sslErrorString;

    static constexpr int kTimeoutMs = 8000;
    static constexpr const char* kWsUrl ="wss://127.0.0.1:6666";
};

} // namespace dao

#endif