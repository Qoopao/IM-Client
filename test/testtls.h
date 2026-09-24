#ifndef TESTTLS_H
#define TESTTLS_H

#include <QCoreApplication>
#include <QWebSocket>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslSocket>
#include <QDebug>
#include <QSslCipher>
#include <QFile>

class testTLS : public QObject
{
    Q_OBJECT
public:
    testTLS();

    QWebSocket* createPlainClient(const QUrl &url);
    QWebSocket* createTlsClient(const QUrl &url, const QString &certPath);
private:

};

#endif // TESTTLS_H
