#ifndef REMOTE_REG_DAO_H
#define REMOTE_REG_DAO_H

#include <QObject>
#include <QTimer>
#include <QWebSocket>
#include <functional>
#include <string>

#include "../model/regResult.h"
#include "../proto/sdkws.pb.h"

namespace dao {

class RemoteRegDao : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(const model::RegResult&)>;

    explicit RemoteRegDao(QObject* parent = nullptr);
    ~RemoteRegDao() override;

    void sendReg(const QString& phone, const QString& nickname,
                 const QString& password, Callback callback);
    // Cancellation is silent, as in RemoteAuthDao.
    void cancel();

private slots:
    void onConnected();
    void onDisconnected();
    void onRegRespReceived(const QByteArray& msg);
    void onTimeout();
    void onSslErrors(const QList<QSslError>& errors);

private:
    QString findCertFile() const;
    void loadTrustAnchor();
    bool genBin(const QString& phone, const QString& nickname, const QString& password);
    model::RegResult parseResp(const sdkws::SdkWSResp& resp) const;
    void finish(const model::RegResult& result);
    void releaseSocket();

    QWebSocket* m_socket = nullptr;
    QTimer m_timeoutTimer;
    std::string m_bin;
    Callback m_callback;
    QString m_pendingRequestId;
    QString m_requestSettingsKey;

    static constexpr int kTimeoutMs = 8000;
    static constexpr const char* kWsUrl = "wss://127.0.0.1:6666";
};

} // namespace dao

#endif // REMOTE_REG_DAO_H
