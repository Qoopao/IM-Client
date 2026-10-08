#ifndef REG_CONTROLLER_H
#define REG_CONTROLLER_H

#include "../service/regService.h"

namespace controller {

class RegController : public QObject
{
    Q_OBJECT
public:
    explicit RegController(QObject* parent = nullptr);
    bool validPhone(const QString& value) const;
    bool validNickname(const QString& value) const;
    bool validPassword(const QString& value) const;
    void registerAccount(const QString& phone, const QString& nickname, const QString& password);
    void cancel();
signals:
    void stateChanged(const model::RegModel& state);
private:
    service::RegService m_service;
};

}

#endif
