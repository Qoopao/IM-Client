#include "authController.h"

namespace controller {

AuthController::AuthController(model::AuthModel* model, QObject* parent)
    : QObject(parent)
    , m_model(model)
{
}

void AuthController::login(const QString& account, const QString& password)
{
    // 防重复：请求进行中忽略
    if (m_model->getStatus() == model::AuthModel::Status::LoggingIn)
        return;

    // 输入校验，不发无效请求
    if (account.trimmed().isEmpty()) {
        m_model->setFailed(QStringLiteral("账号不能为空"));
        return;
    }
    if (password.isEmpty()) {
        m_model->setFailed(QStringLiteral("密码不能为空"));
        return;
    }

    bool ok = false;
    const uint64_t acc = account.trimmed().toULongLong(&ok);
    m_model->setLoggingIn(account);
    if (!ok) {
        m_model->setFailed(QStringLiteral("账号格式错误"));
        return;
    }


    m_remoteDao.sendAuth(acc, password,
        [this, acc](const model::LoginResult& result) {
            if (result.error == model::LoginError::None) {
                m_model->setSuccess(QString::number(acc));
            } else if (result.error == model::LoginError::Cancelled) {
                m_model->reset();
            } else if (result.error == model::LoginError::NetworkError){
                m_model->setFailed(result.message.isEmpty() ? fallbackMessage(result.error) : result.message);
            }else {
                m_model->setFailed(result.message.isEmpty() ? fallbackMessage(result.error) : result.message);
            }
        });
}

void AuthController::logout()
{
    m_model->reset();
}

QString AuthController::fallbackMessage(model::LoginError error)
{
    switch (error) {
    case model::LoginError::InvalidCredentials:
        return QStringLiteral("账号或密码错误");
    case model::LoginError::RateLimited:
        return QStringLiteral("尝试次数过多，请稍后再试");
    case model::LoginError::NetworkError:
        return QStringLiteral("网络错误，请检查连接");
    case model::LoginError::ServerRejected:
        return QStringLiteral("服务端拒绝通信");
    case model::LoginError::Cancelled:
        return QStringLiteral("用户取消登陆");
    default:
        return QStringLiteral("未知错误");
    }
}

} // namespace controller
