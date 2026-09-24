#include "authModel.h"

namespace model {

AuthModel::AuthModel(QObject* parent)
    : QObject(parent)
{
}

void AuthModel::setLoggingIn(const QString& account)
{
    m_account = account;
    m_errorMessage.clear();
    m_status = Status::LoggingIn;
    emit accountChanged(m_account);
    emit errorMessageChanged(m_errorMessage);
    emit statusChanged(m_status);
}

void AuthModel::setSuccess(const QString& account)
{
    m_account = account;
    m_errorMessage.clear();
    m_status = Status::Success;
    emit accountChanged(m_account);
    emit errorMessageChanged(m_errorMessage);
    emit statusChanged(m_status);
}

void AuthModel::setFailed(const QString& message)
{
    m_errorMessage = message;
    m_status = Status::Failed;
    emit errorMessageChanged(m_errorMessage);
    emit statusChanged(m_status);
}

void AuthModel::reset()
{
    m_account.clear();
    m_errorMessage.clear();
    m_status = Status::Idle;
    emit accountChanged(m_account);
    emit errorMessageChanged(m_errorMessage);
    emit statusChanged(m_status);
}

} // namespace model
