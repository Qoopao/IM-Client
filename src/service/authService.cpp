#include "authService.h"
#include <QRegularExpression>

namespace service {

AuthService::AuthService(QObject* parent) : QObject(parent) {}

void AuthService::login(const QString& account, const QString& password)
{
    if (m_state.status == model::AuthModel::Status::LoggingIn)
        return;

    const QString normalized = account.trimmed();
    bool ok = false;
    const auto number = normalized.toULongLong(&ok);
    QString error;
    if (normalized.isEmpty())
        error = QStringLiteral("账号不能为空");
    else if (!ok || !QRegularExpression(QStringLiteral("^[0-9]+$")).match(normalized).hasMatch())
        error = QStringLiteral("账号格式错误");
    else if (password.isEmpty())
        error = QStringLiteral("密码不能为空");

    if (!error.isEmpty()) {
        m_dao.cancel();
        m_state = {};
        m_state.errorMessage = error;
        m_state.status = model::AuthModel::Status::Failed;
        emit stateChanged(model::AuthModel(m_state));
        return;
    }

    m_dao.cancel();
    m_state = {};
    m_state.account = QString::number(number);
    m_state.status = model::AuthModel::Status::LoggingIn;

    m_dao.sendAuth(number, password, [this](const model::LoginResult& result) {
        if (result.error == model::LoginError::Cancelled) {
            m_state = {};
        } else if (result.error == model::LoginError::None) {
            m_state.status = model::AuthModel::Status::Success;
            m_state.token = result.token;
        } else {
            m_state.status = model::AuthModel::Status::Failed;
            m_state.errorMessage = result.message;
            if (m_state.errorMessage.isEmpty()) {
                switch (result.error) {
                case model::LoginError::InvalidCredentials:
                    m_state.errorMessage = QStringLiteral("账号或密码错误"); break;
                case model::LoginError::RateLimited:
                    m_state.errorMessage = QStringLiteral("尝试次数过多，请稍后再试"); break;
                case model::LoginError::NetworkError:
                    m_state.errorMessage = QStringLiteral("网络错误，请检查连接"); break;
                default:
                    m_state.errorMessage = QStringLiteral("服务端拒绝登录"); break;
                }
            }
        }
        emit stateChanged(model::AuthModel(m_state));
    });

    if (m_state.status == model::AuthModel::Status::LoggingIn)
        emit stateChanged(model::AuthModel(m_state));
}

void AuthService::logout()
{
    m_dao.cancel();
    m_state = {};
    emit stateChanged(model::AuthModel(m_state));
}

}
