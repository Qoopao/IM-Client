#include "src/ui/login.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    std::unique_ptr<LoginWindow> loginW = std::make_unique<LoginWindow>();
    loginW->show();
    return QCoreApplication::exec();
}
