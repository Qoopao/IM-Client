#include "regService.h"
#include <QRegularExpression>

namespace service {

RegService::RegService(QObject* parent) : QObject(parent) {}

bool RegService::validPhone(const QString& value)
{
    return QRegularExpression(QStringLiteral("^1[3-9][0-9]{9}$")).match(value).hasMatch();
}

bool RegService::validNickname(const QString& value)
{
    return QRegularExpression(QStringLiteral(R"(^[\p{Han}a-zA-Z][\p{Han}a-zA-Z0-9_]{2,9}$)")).match(value).hasMatch();
}

bool RegService::validPassword(const QString& value)
{
    return QRegularExpression(QStringLiteral("^[A-Za-z0-9]{6,16}$")).match(value).hasMatch()
        && value.contains(QRegularExpression(QStringLiteral("[A-Za-z]")))
        && value.contains(QRegularExpression(QStringLiteral("[0-9]")));
}

void RegService::registerAccount(const QString& phone, const QString& nickname, const QString& password)
{
    if (m_state.status == model::RegModel::Status::Registering)
        return;
    QString error;
    if (!validPhone(phone))
        error = QStringLiteral("请正确填写手机号。");
    else if (!validNickname(nickname))
        error = QStringLiteral("用户名须为3至10位，以字母或汉字开头，只能包含汉字、字母、数字和下划线。");
    else if (!validPassword(password))
        error = QStringLiteral("密码必须同时包含字母与数字，长度为6至16位。");

    m_state = {};
    if (!error.isEmpty()) {
        m_state.status = model::RegModel::Status::Failed;
        m_state.result.error = model::RegError::InvalidInput;
        m_state.result.message = error;
        emit stateChanged(model::RegModel(m_state));
        return;
    }

    m_state.status = model::RegModel::Status::Registering;
    m_dao.sendReg(phone, nickname, password, [this](const model::RegResult& result) {
        m_state.result = result;
        m_state.status = result.error == model::RegError::None
            ? model::RegModel::Status::Success : model::RegModel::Status::Failed;
        if (m_state.status == model::RegModel::Status::Failed && m_state.result.message.isEmpty())
            m_state.result.message = QStringLiteral("注册失败，请稍后重试。");
        emit stateChanged(model::RegModel(m_state));
    });

    if (m_state.status == model::RegModel::Status::Registering)
        emit stateChanged(model::RegModel(m_state));
}
void RegService::cancel()
{
    m_dao.cancel();
    m_state = {};
    emit stateChanged(model::RegModel(m_state));
}

}
