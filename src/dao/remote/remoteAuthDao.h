#ifndef REMOTE_AUTH_DAO
#define REMOTE_AUTH_DAO

#include <QObject>
#include <QTimer>
#include <functional>
#include <cstdint>
#include "../../model/loginResult.h"
#include "../../network/webSocketSession.h"
#include "../../proto/sdkws.pb.h"

namespace dao {

class RemoteAuthDao : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(const model::LoginResult&)>;

    explicit RemoteAuthDao(network::WebSocketSession& connection, QObject* parent = nullptr);
    ~RemoteAuthDao() override;

    void sendAuth(uint64_t account, const QString& password, Callback callback);
    void cancel();

private:
    void onConnected();
    void onResponseReceived(const QByteArray& frame);
    void failAuthentication(const QString& error);
    void finishAuthentication(const model::LoginResult& result);
    model::LoginResult parseResp(const sdkws::SdkWSResp& resp) const;

    network::WebSocketSession& m_connection;
    QTimer m_timeoutTimer;
    Callback m_callback;
    QString m_pendingRequestId;
    QByteArray m_password;

    static constexpr int kTimeoutMs = 8000;
};

}

#endif // REMOTE_AUTH_DAO
