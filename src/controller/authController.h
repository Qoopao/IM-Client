#ifndef AUTH_CONTROLLER_H
#define AUTH_CONTROLLER_H

#include <QObject>
#include <QString>

#include "../model/loginResult.h"
#include "../model/authModel.h"
#include "../dao/remoteAuthDao.h"

namespace controller {

// Controller：面向 UI 的动作入口，编排 DAO 完成登录用例
class AuthController : public QObject
{
    Q_OBJECT
public:
    explicit AuthController(model::AuthModel* model, QObject* parent = nullptr);

    void login(const QString& account, const QString& password);
    void logout();

private:
    static QString fallbackMessage(model::LoginError error);

    model::AuthModel* m_model;      // 外部持有（View 创建，Controller 与 View 共享）
    dao::RemoteAuthDao m_remoteDao; // 远程数据访问
};

} // namespace controller

#endif // AUTH_CONTROLLER_H
