#ifndef AUTH_SERVICE_H
#define AUTH_SERVICE_H

#include <QObject>
#include "../dao/remoteAuthDao.h"
#include "../model/authModel.h"

namespace service {

class AuthService : public QObject
{
    Q_OBJECT
public:
    explicit AuthService(QObject* parent = nullptr);
    const model::AuthModel& state() const { return m_state; }
    void login(const QString& account, const QString& password);
    void logout();
signals:
    void stateChanged(const model::AuthModel& state);
private:
    model::AuthModel m_state;
    dao::RemoteAuthDao m_dao;
};

}

#endif
