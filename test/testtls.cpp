#include "testtls.h"

testTLS::testTLS() {
}

// ── 明文 WS 客户端 ──
QWebSocket* testTLS::createPlainClient(const QUrl &url)
{
    auto *socket = new QWebSocket();

    QObject::connect(socket, &QWebSocket::connected, [socket]() {
        qDebug() << "✅ 明文 WS 连接已建立（未加密）";
        socket->sendTextMessage("HELLO_PLAINTEXT_12345");
    });

    QObject::connect(socket, &QWebSocket::binaryMessageReceived,
                     [](const QByteArray &msg) {
                         qDebug() << "收到二进制消息，大小:" << msg.size();
                     });

    QObject::connect(socket, &QWebSocket::textMessageReceived,
                     [](const QString &msg) {
                         qDebug() << "收到文本消息:" << msg;
                     });

    QObject::connect(socket, &QWebSocket::errorOccurred,
                     [socket](QAbstractSocket::SocketError) {
                         qWarning() << "连接错误:" << socket->errorString();
                     });

    QObject::connect(socket, &QWebSocket::disconnected, []() {
        qDebug() << "连接已断开";
    });

    socket->open(url);
    return socket;
}

// ── TLS WSS 客户端 ──
QWebSocket* testTLS::createTlsClient(const QUrl &url, const QString &certPath)
{
    auto *socket = new QWebSocket();

    // 用 fromDevice 读资源，绕开 fromPath 的坑
    QFile certFile(certPath);
    if (!certFile.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开证书:" << certPath
                   << certFile.errorString();
        delete socket;
        return nullptr;
    }
    QList<QSslCertificate> certs =
        QSslCertificate::fromDevice(&certFile, QSsl::Pem);
    certFile.close();

    qDebug() << "加载证书数量:" << certs.size();
    if (certs.isEmpty()) {
        qWarning() << "证书解析失败:" << certPath;
        delete socket;
        return nullptr;
    }

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setCaCertificates(certs);
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
    socket->setSslConfiguration(sslConfig);

    qDebug() << "设置后 CA 数量:"
             << socket->sslConfiguration().caCertificates().size();

    QObject::connect(socket, &QWebSocket::connected, [socket]() {
        qDebug() << "✅ WSS 连接已建立";

        // 打印 TLS 版本和加密套件，证明真的加密了
        QSslCipher cipher = socket->sslConfiguration().sessionCipher();
        qDebug() << "TLS 版本:" << socket->sslConfiguration().sessionProtocol();
        qDebug() << "加密套件:" << cipher.name();
        qDebug() << "加密算法:" << cipher.encryptionMethod();

        socket->sendTextMessage("HELLO_TLS_12345");
    });

    QObject::connect(socket, &QWebSocket::binaryMessageReceived,
                     [](const QByteArray &msg) {
                         qDebug() << "收到二进制消息，大小:" << msg.size();
                     });

    QObject::connect(socket, &QWebSocket::textMessageReceived,
                     [](const QString &msg) {
                         qDebug() << "收到文本消息:" << msg;
                     });

    // SSL 错误诊断（不要 ignore，先看是什么错）
    QObject::connect(socket,
                     QOverload<const QList<QSslError>&>::of(&QWebSocket::sslErrors),
                     [](const QList<QSslError> &errors) {
                         for (const QSslError &e : errors)
                             qWarning() << "❌ SSL 错误:" << e.errorString();
                     });

    QObject::connect(socket, &QWebSocket::errorOccurred,
                     [socket](QAbstractSocket::SocketError) {
                         qWarning() << "连接错误:" << socket->errorString();
                     });

    QObject::connect(socket, &QWebSocket::disconnected, []() {
        qDebug() << "连接已断开";
    });

    socket->open(url);
    return socket;
}
