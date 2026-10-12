#include "applicationController.h"
#include "../ui/login.h"
#include "../ui/mainIM.h"
#include <QApplication>
#include <QDebug>

namespace application {

ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent)
    , m_historyDao(m_database)
    , m_historyService(m_historyDao)
{
    connect(qApp, &QCoreApplication::aboutToQuit, this, &ApplicationController::shutdown);
}

ApplicationController::~ApplicationController()
{
    shutdown();
    if (m_loginWindow) {
        m_loginWindow->disconnect(this);
        delete m_loginWindow.data();
    }
    if (m_mainWindow) {
        m_mainWindow->disconnect(this);
        delete m_mainWindow.data();
    }
    m_session.reset();
}

void ApplicationController::start()
{
    if (m_phase != Phase::Idle)
        return;

    QApplication::setQuitOnLastWindowClosed(false);
    showLogin();
}

void ApplicationController::showLogin()
{
    m_phase = Phase::Login;
    m_session = std::make_unique<ClientSession>(m_database);
    m_loginWindow = new LoginWindow(m_historyService, m_session->authController());
    m_loginWindow->setAttribute(Qt::WA_DeleteOnClose);

    connect(&m_session->authController(), &controller::AuthController::stateChanged, this,
            [this](const model::AuthModel& state) {
                if (state.status == model::AuthModel::Status::Success)
                    onLoginSucceeded(state.account);
            });
    connect(m_loginWindow.data(), &QObject::destroyed, this, [this] {
        if (m_phase == Phase::Login && !m_loginWindow)
            QApplication::quit();
    });

    m_loginWindow->show();
}

void ApplicationController::onLoginSucceeded(const QString& account)
{
    if (m_phase != Phase::Login || !m_loginWindow)
        return;

    m_phase = Phase::Chat;
    m_mainWindow = new mainIM(m_session->chatService());
    m_mainWindow->setAttribute(Qt::WA_DeleteOnClose);

    connect(m_mainWindow.data(), &mainIM::closing, this, &ApplicationController::shutdown);
    connect(m_mainWindow.data(), &QObject::destroyed, this, [this] {
        if (m_phase == Phase::Chat || m_phase == Phase::Stopped)
            QApplication::quit();
    });

    m_session->activate(account);
    m_mainWindow->show();
    m_session->chatService().setViewing(m_mainWindow->isActiveWindow());

    m_loginWindow->hide();
    m_loginWindow->deleteLater();

    QString error;
    if (!m_historyService.recordSuccessfulLogin(account, error))
        qWarning().noquote() << QStringLiteral("历史账号保存失败：") << error;
}

void ApplicationController::shutdown()
{
    if (m_phase == Phase::Stopped)
        return;

    m_phase = Phase::Stopped;
    if (m_session)
        m_session->shutdown();
}

}
