#include "src/application/applicationController.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("IM-client"));
    QCoreApplication::setApplicationName(QStringLiteral("IM-client"));

    application::ApplicationController controller;
    controller.start();

    return QCoreApplication::exec();
}
