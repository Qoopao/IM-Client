#ifndef REG_SERVICE_H
#define REG_SERVICE_H

#include "../dao/remote/remoteRegDao.h"
#include "../model/regModel.h"

namespace service {

class RegService : public QObject
{
    Q_OBJECT
public:
    explicit RegService(QObject* parent = nullptr);
    static bool validPhone(const QString& value);
    static bool validNickname(const QString& value);
    static bool validPassword(const QString& value);
    const model::RegModel& state() const { return m_state; }
    void registerAccount(const QString& phone, const QString& nickname, const QString& password);
    void cancel();
signals:
    void stateChanged(const model::RegModel& state);
private:
    model::RegModel m_state;
    dao::RemoteRegDao m_dao;
};

}

#endif
