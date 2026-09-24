#ifndef AUTH_MODEL_H
#define AUTH_MODEL_H

#include <QObject>
#include <QString>

namespace model {

// Model：登录会话状态源，UI 订阅它的信号
class AuthModel : public QObject
{
    Q_OBJECT
public:
    enum class Status
    {
        Idle,       // 空闲 / 未登录
        LoggingIn,  // 登录中
        Success,    // 已登录
        Failed,     // 失败
    };

    explicit AuthModel(QObject* parent = nullptr);

    Status getStatus() const { return m_status; }
    QString getAccount() const { return m_account; }
    QString getErrorMessage() const { return m_errorMessage; }

    // 状态转移（由 Controller 调用）
    void setLoggingIn(const QString& account);
    void setSuccess(const QString& account);
    void setFailed(const QString& message);
    void reset();

signals:
    void statusChanged(Status status);
    void accountChanged(const QString& account);
    void errorMessageChanged(const QString& message);

private:
    Status m_status = Status::Idle;
    QString m_account;
    QString m_errorMessage;
};

} // namespace model

#endif // AUTH_MODEL_H
