#include "remoteAuthDao.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QUrl>

namespace dao {

RemoteAuthDao::RemoteAuthDao(QObject* parent)
    : QObject(parent)
{
    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &RemoteAuthDao::onTimeout);

    // 1. 证书固定：把自签名证书加进默认 SSL 配置
    loadTrustAnchor();

    // 2. 创建 WebSocket
    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_socket->setSslConfiguration(QSslConfiguration::defaultConfiguration());

    // 3. 连接slot
    connect(m_socket, &QWebSocket::connected, this, &RemoteAuthDao::onConnected);
    connect(m_socket, &QWebSocket::disconnected, this, &RemoteAuthDao::onDisconnected);
    connect(m_socket, &QWebSocket::binaryMessageReceived, this, &RemoteAuthDao::onAuthRespReceived);
    connect(m_socket,QOverload<const QList<QSslError>&>::of(&QWebSocket::sslErrors), this, &RemoteAuthDao::onSslErrors);

    // 发起连接（先 TLS，再 WebSocket 握手，由QT内部完成）
    m_socket->open(QUrl(QString::fromUtf8(kWsUrl)));
}

void RemoteAuthDao::sendAuth(uint64_t account, const QString& password, Callback callback)
{


    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        model::LoginResult r;
        r.error = model::LoginError::NetworkError;
        r.message = QStringLiteral("WebSocket 未连接");
        callback(r);
        return;
    }
    if (m_callback) return;   // 防并发

    m_callback = std::move(callback);
    m_timedOut = false;
    m_account  = account;

    // 生成 requestId（后端会原样回填，用来匹配响应）
    m_pendingRequestId = QString::number(QDateTime::currentMSecsSinceEpoch());

    sdkws::SdkWSReq req;
    req.set_account(account);
    req.set_data(password.toStdString());
    req.set_type(101);   // 后端 doReadAuth 不看 type，但保持和你原代码一致
    req.set_requestid(m_pendingRequestId.toStdString());

    sendReq(req);
    m_timeoutTimer.start(kTimeoutMs);
}

void RemoteAuthDao::sendReq(const sdkws::SdkWSReq& req)
{

    const std::string bin = req.SerializeAsString();

    qDebug() << "account=" << req.account()
             << "bin_size=" << bin.size()
             << "hex=" << QByteArray(bin.data(), bin.size()).toHex();

    m_socket->sendBinaryMessage(QByteArray(bin.data(), static_cast<int>(bin.size())));
}


void RemoteAuthDao::onAuthRespReceived(const QByteArray& msg)
{
    // 1. 反序列化
    sdkws::SdkWSResp resp;
    if (!resp.ParseFromArray(msg.constData(), msg.size())) {
        qWarning() << "RemoteAuthDao: 无法解析 SdkWSResp, size=" << msg.size();
        return;
    }

    const QString reqId = QString::fromStdString(resp.requestid());
    const int type = resp.type();
    const QString errCode = QString::fromStdString(resp.errorcode());

    // 2. 处理认证响应（登录发起的那次请求）
    if (m_callback && reqId == m_pendingRequestId) {
        m_timeoutTimer.stop();

        Callback cb = std::move(m_callback);
        m_callback = nullptr;
        m_pendingRequestId.clear();

        model::LoginResult result = parseResp(resp);
        if (result.error == model::LoginError::None) {
            m_token = QString::fromStdString(resp.token());
        }
        cb(result);
        return;
    }

    // 3. 处理其他服务端主动推送 / 异步消息
    //    type=200 离线消息推送；type=0 pong 心跳
    if (type == 200) {
        // TODO: 交给消息模块处理
        return;
    }
    if (type == 0 && errCode == "0") {
        // pong，心跳正常
        return;
    }
}


model::LoginResult RemoteAuthDao::parseResp(const sdkws::SdkWSResp& resp)
{
    model::LoginResult result;
    const QString code = QString::fromStdString(resp.errorcode());

    if (code == "0") {
        result.error = model::LoginError::None;
        return result;
    }
    if (code == "1") {
        // 后端认证帧字段缺失
        result.error = model::LoginError::InvalidCredentials;
        result.message = QString::fromStdString(resp.errormsg());
        return result;
    }
    if (code == "401") {
        result.error = model::LoginError::InvalidCredentials;
        result.message = QString::fromStdString(resp.errormsg());
        return result;
    }
    result.error = model::LoginError::ServerRejected;
    result.message = QString::fromStdString(resp.errormsg());
    return result;
}

void RemoteAuthDao::onTimeout()
{
    m_timedOut = true;

    Callback cb = std::move(m_callback);
    m_callback = nullptr;
    m_pendingRequestId.clear();

    if (cb) {
        model::LoginResult r;
        r.error = model::LoginError::NetworkError;
        r.message = QStringLiteral("登录超时");
        cb(r);
    }
}

void RemoteAuthDao::cancel()
{
    m_timeoutTimer.stop();
    m_callback = nullptr;
    m_pendingRequestId.clear();
    m_timedOut = false;
}

void RemoteAuthDao::onConnected()
{
    qInfo() << "RemoteAuthDao: WebSocket connected";
}

void RemoteAuthDao::onDisconnected()
{
    qInfo() << "RemoteAuthDao: WebSocket disconnected";

    qInfo() << "RemoteAuthDao: WebSocket disconnected"
            << "code =" << m_socket->closeCode()
            << "reason =" << m_socket->closeReason()
            << "error =" << m_socket->errorString();

    // 有等待中的回调，通知失败
    if (m_callback) {
        Callback cb = std::move(m_callback);
        m_callback = nullptr;
        m_pendingRequestId.clear();
        m_timeoutTimer.stop();

        model::LoginResult r;
        r.error = model::LoginError::NetworkError;
        r.message = QStringLiteral("连接已断开");
        cb(r);
    }
}

void RemoteAuthDao::onSslErrors(const QList<QSslError>& errors)
{
    m_hadSslError = true;
    if (!errors.isEmpty())
        m_sslErrorString = errors.first().errorString();
}

void RemoteAuthDao::loadTrustAnchor()
{
    // 证书固定：把 demo 自签名证书加入信任锚。生产环境删除此逻辑，
    // 使用系统 CA + 域名校验即可。
    const QString certPath = findCertFile();
    if (certPath.isEmpty()) {
        qWarning() << "RemoteAuthDao: 未找到 server.crt，TLS 校验将按系统 CA 进行"
                      "（自签名服务端将校验失败）";
        return;
    }
    const QList<QSslCertificate> certs =
        QSslCertificate::fromPath(certPath, QSsl::Pem);
    if (certs.isEmpty()) {
        qWarning() << "RemoteAuthDao: 无法解析证书:" << certPath;
        return;
    }
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setCaCertificates(certs);
    ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
    QSslConfiguration::setDefaultConfiguration(ssl);
}

QString RemoteAuthDao::findCertFile()
{
    // 依次尝试 exe 附近与源码树相对路径（build/ 与源码根目录两种布局）
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/tools/certs/ca.crt"),
        appDir + QStringLiteral("/../../tools/certs/ca.crt"),
        appDir + QStringLiteral("/../tools/certs/ca.crt"),
        QDir::currentPath() + QStringLiteral("/tools/certs/ca.crt"),
    };
    for (const QString& p : candidates) {
        if (QFileInfo::exists(p))
            return p;
    }
    return {};
}

} // namespace dao
