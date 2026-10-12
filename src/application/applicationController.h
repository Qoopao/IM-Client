#ifndef APPLICATION_CONTROLLER_H
#define APPLICATION_CONTROLLER_H

#include <QObject>
#include <QPointer>
#include <memory>
#include "../storage/wcdb/localDatabase.h"
#include "../dao/local/wcdb/wcdbAccountHistoryDao.h"
#include "../service/accountHistoryService.h"
#include "clientSession.h"

class LoginWindow;
class mainIM;

namespace application {

class ApplicationController : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationController(QObject* parent = nullptr);
    ~ApplicationController() override;

    void start();

private:
    // 应用控制器的状态仅内部使用，所以不考虑放在model下
    enum class Phase { Idle, Login, Chat, Stopped };

    void showLogin();
    void onLoginSucceeded(const QString& account);
    void shutdown();

    // 历史登陆账号相关
    storage::LocalDatabase m_database;
    dao::WcdbAccountHistoryDao m_historyDao;
    service::AccountHistoryService m_historyService;

    std::unique_ptr<ClientSession> m_session;
    QPointer<LoginWindow> m_loginWindow;
    QPointer<mainIM> m_mainWindow;
    Phase m_phase = Phase::Idle;
};

}

#endif // APPLICATION_CONTROLLER_H
