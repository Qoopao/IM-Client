#include "src/application/applicationController.h"
#include "src/ui/mainIM.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("IM-client"));
    QCoreApplication::setApplicationName(QStringLiteral("IM-client"));

    // 主程序启动
    // application::ApplicationController controller;
    // controller.start();

    storage::LocalDatabase database;
    application::ClientSession session(database);
    mainIM window(session.chatService());
    QObject::connect(&window, &mainIM::closing, &window, [&session] {
        session.shutdown();
    });
    window.show();


    return QCoreApplication::exec();
}
