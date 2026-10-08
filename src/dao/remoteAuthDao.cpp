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

    // 证书加进默认 SSL 配置
    loadTrustAnchor();

}

RemoteAuthDao::~RemoteAuthDao(){
    m_socket->deleteLater();
    m_socket = nullptr;
}

void RemoteAuthDao::sendAuth(uint64_t account, const QString& password, Callback callback)
{

    if (m_callback) return;   // 防并发

    m_callback = std::move(callback);
    m_timedOut = false;
    m_account  = account;

    // 创建 WebSocket
    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_socket->setSslConfiguration(QSslConfiguration::defaultConfiguration());

    // 连接slot
    connect(m_socket, &QWebSocket::connected, this, &RemoteAuthDao::onConnected);
    connect(m_socket, &QWebSocket::disconnected, this, &RemoteAuthDao::onDisconnected);
    connect(m_socket, &QWebSocket::binaryMessageReceived, this, &RemoteAuthDao::onAuthRespReceived);
    connect(m_socket,QOverload<const QList<QSslError>&>::of(&QWebSocket::sslErrors), this, &RemoteAuthDao::onSslErrors);

    // 发起连接（先 TLS，再 WebSocket 握手，由QT内部完成）
    genBin(account,password);
    m_socket->open(QUrl(QString::fromUtf8(kWsUrl)));        // 异步的
    m_timeoutTimer.start(kTimeoutMs);                               // 开始连接计时
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
    m_token = QString::fromStdString(resp.token());

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
    const QString err = QString::fromStdString(resp.errormsg());

    if (code == "" && err == "") {
        result.error = model::LoginError::None;
        return result;
    } else if (code == "INVALID_PARAM") {
        // 后端认证帧字段缺失
        result.error = model::LoginError::InvalidCredentials;
        result.message = QString::fromStdString(resp.errormsg());
        return result;
    } else if(code == "USER_NOT_FOUND"){
        result.error = model::LoginError::InvalidCredentials;
        result.message = QString::fromStdString(resp.errormsg());
        return result;
    } else if(code == "WRONG_PASSWORD"){
        result.error = model::LoginError::InvalidCredentials;
        result.message = QString::fromStdString(resp.errormsg());
        return result;
    } else if(code == "INTERNAL_ERROR"){
        result.error = model::LoginError::ServerRejected;
        result.message = QString::fromStdString(resp.errormsg());
        return result;
    }else{
        result.error = model::LoginError::ServerRejected;
        result.message = QString::fromStdString(resp.errormsg());
        return result;
    }
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
        r.message = QStringLiteral("登录超时，请稍后重试");
        cb(r);
    }

    m_socket->deleteLater();
    m_socket = nullptr;
}

void RemoteAuthDao::cancel()
{
    m_timeoutTimer.stop();
    m_callback = nullptr;
    m_pendingRequestId.clear();
    m_timedOut = false;

    m_socket->deleteLater();
    m_socket = nullptr;
}

void RemoteAuthDao::onConnected()
{
    qInfo() << "RemoteAuthDao: WebSocket connected";            // TLS握手+websocket握手成功
    m_socket->sendBinaryMessage(QByteArray(m_bin.data(), static_cast<int>(m_bin.size())));

}

void RemoteAuthDao::onDisconnected()
{

    qInfo() << "RemoteAuthDao: WebSocket disconnected. "
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
        r.message = QStringLiteral("服务器连接失败");
        cb(r);
    }

    m_socket->close(QWebSocketProtocol::CloseCodeNormal, "client close");
    m_socket->deleteLater();
    m_socket = nullptr;
}

void RemoteAuthDao::onSslErrors(const QList<QSslError>& errors)
{
    m_hadSslError = true;
    if (!errors.isEmpty()){
        m_sslErrorString = errors.first().errorString();
    }

    if (m_callback) {
        Callback cb = std::move(m_callback);
        m_callback = nullptr;
        m_pendingRequestId.clear();
        m_timeoutTimer.stop();

        model::LoginResult r;
        r.error = model::LoginError::NetworkError;
        r.message = QStringLiteral("SSL握手失败");
        cb(r);
    }

    m_socket->close(QWebSocketProtocol::CloseCodeNormal, "client close");
    m_socket->deleteLater();
    m_socket = nullptr;
}

void RemoteAuthDao::loadTrustAnchor()
{
    // 证书固定：把 demo 自签名证书加入信任锚。生产环境删除此逻辑，
    // 使用系统 CA + 域名校验即可。
    const QString certPath = findCertFile();
    if (certPath.isEmpty()) {
        qWarning() << "RemoteAuthDao: 未找到 server.crt";
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

void RemoteAuthDao::genBin(uint64_t account, const QString& password){
    // 生成 requestId（后端会原样回填，用来匹配响应）
    m_pendingRequestId = QString::number(QDateTime::currentMSecsSinceEpoch());

    sdkws::SdkWSReq req;
    req.set_account(account);
    req.set_data(password.toStdString());
    req.set_type(101);
    req.set_requestid(m_pendingRequestId.toStdString());

    m_bin = req.SerializeAsString();
}

} // namespace dao
