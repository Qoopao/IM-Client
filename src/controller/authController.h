#ifndef AUTH_CONTROLLER_H
#define AUTH_CONTROLLER_H

#include "../service/authService.h"

namespace controller {

class AuthController : public QObject
{
    Q_OBJECT
public:
    explicit AuthController(QObject* parent = nullptr);
    const model::AuthModel& state() const { return m_service.state(); }
    void login(const QString& account, const QString& password);
    void logout();
signals:
    void stateChanged(const model::AuthModel& state);
private:
    service::AuthService m_service;
};

}

#endif
