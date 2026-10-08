#include "remoteRegDao.h"
#include "../proto/apeauth.pb.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QUrl>
#include <QUuid>
#include <QSettings>
#include <QCryptographicHash>
#include <utility>

namespace dao {

RemoteRegDao::RemoteRegDao(QObject* parent)
    : QObject(parent)
{
    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &RemoteRegDao::onTimeout);
}

RemoteRegDao::~RemoteRegDao()
{
    qDebug() << "[~RemoteRegDao] enter";
    cancel();
    qDebug() << "[~RemoteRegDao] exit";
}

void RemoteRegDao::sendReg(const QString& phone, const QString& nickname,
                          const QString& password, Callback callback)
{
    if (m_socket || !callback)
        return;

    m_callback = std::move(callback);
    if (!genBin(phone, nickname, password)) {
        model::RegResult result;
        result.error = model::RegError::LocalStorageError;
        result.message = QStringLiteral("无法保存注册请求标识，请检查本地配置目录的写入权限后重试。");
        finish(result);
        return;
    }
    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    loadTrustAnchor();

    connect(m_socket, &QWebSocket::connected, this, &RemoteRegDao::onConnected);
    connect(m_socket, &QWebSocket::disconnected, this, &RemoteRegDao::onDisconnected);
    connect(m_socket, &QWebSocket::binaryMessageReceived, this, &RemoteRegDao::onRegRespReceived);
    connect(m_socket, QOverload<const QList<QSslError>&>::of(&QWebSocket::sslErrors),
            this, &RemoteRegDao::onSslErrors);
    connect(m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (!m_socket)
            return;
        model::RegResult result;
        result.error = model::RegError::NetworkError;
        result.message = QStringLiteral("注册连接失败：") + m_socket->errorString();
        finish(result);
    });

    m_timeoutTimer.start(kTimeoutMs);
    m_socket->open(QUrl(QString::fromUtf8(kWsUrl)));
}

void RemoteRegDao::onConnected()
{
    if (!m_socket)
        return;
    const QByteArray message(m_bin.data(), static_cast<qsizetype>(m_bin.size()));
    m_bin.clear();
    m_socket->sendBinaryMessage(message);
}

void RemoteRegDao::onRegRespReceived(const QByteArray& msg)
{
    if (!m_callback)
        return;

    sdkws::SdkWSResp resp;
    if (!resp.ParseFromArray(msg.constData(), static_cast<int>(msg.size()))) {
        model::RegResult result;
        result.error = model::RegError::InvalidResponse;
        result.message = QStringLiteral("无法解析注册响应");
        finish(result);
        return;
    }

    // 每个响应必须带requestID
    const QString respId = QString::fromStdString(resp.requestid());
    if (!respId.isEmpty()) {
        if (respId != m_pendingRequestId)     // 响应对不上请求，判断为网关的错误帧
            return;
        finish(parseResp(resp));
        return;
    }
}

void RemoteRegDao::onDisconnected()
{
    model::RegResult result;
    result.error = model::RegError::NetworkError;
    result.message = QStringLiteral("服务器连接已断开");
    finish(result);
}

void RemoteRegDao::onTimeout()
{
    model::RegResult result;
    result.error = model::RegError::NetworkError;
    result.message = QStringLiteral("注册超时，请稍后重试");
    finish(result);
}

void RemoteRegDao::onSslErrors(const QList<QSslError>& errors)
{
    if (errors.isEmpty())
        return;
    model::RegResult result;
    result.error = model::RegError::NetworkError;
    result.message = QStringLiteral("SSL握手失败：") + errors.first().errorString();
    finish(result);
}

// void RemoteRegDao::finish(const model::RegResult& result)
// {
//     // 注册成功/注册参数非法时清理掉持久化的requestID
//     if (result.error == model::RegError::None || result.error == model::RegError::InvalidInput) {
//         QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("ape"), QStringLiteral("IM-client"));
//         if (!m_requestSettingsKey.isEmpty() && settings.value(m_requestSettingsKey).toString() == m_pendingRequestId)
//             settings.remove(m_requestSettingsKey);
//         settings.sync();
//     }
//     Callback callback = std::move(m_callback);
//     cancel();       // 取消注册（清理Dao的成员+释放连接）
//     // Clean up before invoking user code: the callback may retry or delete this DAO.
//     if (callback)
//         callback(result);
// }
void RemoteRegDao::finish(const model::RegResult& result)
{
    qDebug() << "[1] finish enter, error=" << int(result.error) << "code=" << result.serverCode;

    if (result.error == model::RegError::None || result.error == model::RegError::InvalidInput) {
        qDebug() << "[2] finish: cleaning settings";
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("ape"), QStringLiteral("IM-client"));
        if (!m_requestSettingsKey.isEmpty() &&
            settings.value(m_requestSettingsKey).toString() == m_pendingRequestId)
            settings.remove(m_requestSettingsKey);
        settings.sync();
        qDebug() << "[3] finish: settings cleaned";
    }

    Callback callback = std::move(m_callback);
    qDebug() << "[4] finish: callback moved, has=" << bool(callback);

    cancel();
    qDebug() << "[5] finish: cancel done";

    if (callback) {
        callback(result);
        qDebug() << "[6] finish: callback returned";   // ← 关键：这行打不打印
    }
    qDebug() << "[7] finish: exit";
}

void RemoteRegDao::cancel()
{
    qDebug() << "[cancel] enter";
    m_timeoutTimer.stop();
    m_callback = nullptr;
    m_pendingRequestId.clear();
    m_bin.clear();
    qDebug() << "[cancel] before releaseSocket";
    releaseSocket();
    qDebug() << "[cancel] exit";
}

void RemoteRegDao::releaseSocket()
{
    if (!m_socket) {
        qDebug() << "[releaseSocket] no socket, return";
        return;
    }
    qDebug() << "[releaseSocket] exchanging socket";
    QWebSocket* socket = std::exchange(m_socket, nullptr);
    socket->disconnect(this);
    qDebug() << "[releaseSocket] disconnected";
    socket->abort();
    qDebug() << "[releaseSocket] aborted";
    socket->deleteLater();
    qDebug() << "[releaseSocket] deleteLater done";
}

QString RemoteRegDao::findCertFile() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/tools/certs/ca.crt"),
        appDir + QStringLiteral("/../../tools/certs/ca.crt"),
        appDir + QStringLiteral("/../tools/certs/ca.crt"),
        QDir::currentPath() + QStringLiteral("/tools/certs/ca.crt"),
    };
    for (const QString& path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

void RemoteRegDao::loadTrustAnchor()
{
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    const QString path = findCertFile();
    if (!path.isEmpty()) {
        const auto certificates = QSslCertificate::fromPath(path, QSsl::Pem);
        if (!certificates.isEmpty())
            ssl.addCaCertificates(certificates);
        else
            qWarning() << "RemoteRegDao: cannot parse CA certificate:" << path;
    } else {
        qWarning() << "RemoteRegDao: tools/certs/ca.crt not found";
    }
    ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
    m_socket->setSslConfiguration(ssl);
}

bool RemoteRegDao::genBin(const QString& phone, const QString& nickname,
                          const QString& password)
{
    // 把同一个手机号的注册请求持久化到磁盘上，这样就算注册的时候崩掉了，也能通过后端幂等性来完成注册
    // 文件内容：registration/requests/[phone.sha256]=[uuid]
    m_requestSettingsKey = QStringLiteral("registration/requests/") + QString::fromLatin1(QCryptographicHash::hash(phone.toUtf8(), QCryptographicHash::Sha256).toHex());
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("ape"), QStringLiteral("IM-client"));
    settings.sync();        // 强制写入磁盘+刷新status
    if (settings.status() != QSettings::NoError)
        return false;
    m_pendingRequestId = settings.value(m_requestSettingsKey).toString();
    if (m_pendingRequestId.isEmpty()) {
        m_pendingRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(m_requestSettingsKey, m_pendingRequestId);
        settings.sync();
    }

    if (settings.status() != QSettings::NoError)
        return false;

    auth::RegisterRequest registration;
    registration.set_phone(phone.toStdString());
    registration.set_nickname(nickname.toStdString());
    registration.set_password(password.toStdString());

    sdkws::SdkWSReq request;
    request.set_type(100);           // WSSession::handleReg
    request.set_requestid(m_pendingRequestId.toStdString());
    request.set_data(registration.SerializeAsString());
    m_bin = request.SerializeAsString();
    return true;
}

model::RegResult RemoteRegDao::parseResp(const sdkws::SdkWSResp& resp) const
{
    model::RegResult result;
    const QString code = QString::fromStdString(resp.errorcode());
    result.serverCode = code;
    result.message = QString::fromStdString(resp.errormsg());

    if (code == QStringLiteral("PARSE_ERROR")) {          // 对应服务器解析请求失败
        result.error = model::RegError::InvalidInput;
        result.message = QString::fromStdString(resp.errormsg());   // QStringLiteral("服务器解析请求错误");
        return result;
    } else if(code == QStringLiteral("IMCOMPLETE_INFO")){        // 对应服务器判断关键参数为空
        result.error = model::RegError::InvalidInput;
        result.message = QString::fromStdString(resp.errormsg());  // QStringLiteral("注册参数不完整");
        return result;
    } else if(code == QStringLiteral("INVALID_INFO")){       // 对应服务器判断参数不符合约束
        result.error = model::RegError::InvalidInput;
        result.message = QString::fromStdString(resp.errormsg());   // QStringLiteral("注册参数不合法");
        return result;
    } else if(code == QStringLiteral("ACCOUNT_POOL_UNAVAILABLE")){
        result.error = model::RegError::ServerRejected;
        result.message = QString::fromStdString(resp.errormsg());       // QStringLiteral("账号池不可用");
        return result;
    } else if(code == QStringLiteral("REFILL_INVALID_PARAM")){
        result.error = model::RegError::ServerRejected;
        result.message = QString::fromStdString(resp.errormsg());       // QStringLiteral("账号池补号错误");
        return result;
    } else if(code == QStringLiteral("POOL_INCONSISTENT")){
        result.error = model::RegError::ServerRejected;
        result.message = QString::fromStdString(resp.errormsg());       // QStringLiteral("账号池数据不一致");
        return result;
    } else if(code == QStringLiteral("DB_ERROR")){
        result.error = model::RegError::ServerRejected;
        result.message = QString::fromStdString(resp.errormsg());       // QStringLiteral("服务器数据库错误");
        return result;
    }  else if(code == QStringLiteral("ACCOUNT_RANGE_EXHAUSTED")){
        result.error = model::RegError::ServerRejected;
        result.message = QString::fromStdString(resp.errormsg());       // QStringLiteral("当前发号已达上限");
        return result;
    } else if(code == QStringLiteral("REFILL_UNCERTAIN")){
        result.error = model::RegError::ServerRejected;
        result.message = QString::fromStdString(resp.errormsg());       // QStringLiteral("补号逻辑未确认");
        return result;
    } else if(code == QStringLiteral("REGISTRATION_BUSY")){
        result.error = model::RegError::ServerRejected;
        result.message = QString::fromStdString(resp.errormsg());       // QStringLiteral("注册繁忙，稍后再试");
        return result;
    } else if(code == QStringLiteral("PHONE_EXISTS")){
        result.error = model::RegError::InvalidInput;
        result.message = QString::fromStdString(resp.errormsg());       // QStringLiteral("当前手机号已注册");
        return result;
    } else if(code == QStringLiteral("REGISTRATION_UNCERTAIN")){
        result.error = model::RegError::ServerRejected;
        result.message = QString::fromStdString(resp.errormsg());       // QStringLiteral("注册未确认，请稍后再试");
        return result;
    } else if(code == QStringLiteral("REG_CLIENT_ERROR")){
        result.error = model::RegError::ServerRejected;
        result.message = QString::fromStdString(resp.errormsg());       // QStringLiteral("注册rpc错误");
        return result;
    }  else if (code == QStringLiteral("IDEMPOTENCY_CONFLICT")) {
        result.error = model::RegError::InvalidInput;
        result.message = QStringLiteral("上次注册使用了不同的信息，请重新开始注册。");
        return result;
    } else if (code.isEmpty() || code == QStringLiteral("0")) {                 // 注册成功
        if (resp.account() == 0) {
            result.error = model::RegError::InvalidResponse;
            result.message = QStringLiteral("注册响应未返回有效账号");
        } else {
            result.error = model::RegError::None;
            result.account = resp.account();
        }
        return result;
    } else{
        result.error = model::RegError::ServerRejected;
        result.message = QString::fromStdString(resp.errormsg());       // QStringLiteral("未知错误");
        return result;
    }
}

} // namespace dao
