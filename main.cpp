#include "src/ui/login.h"
#include "src/storage/wcdb/localDatabase.h"
#include "src/dao/local/wcdb/wcdbAccountHistoryDao.h"
#include "src/service/accountHistoryService.h"
#include <QApplication>
#include <memory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("IM-client"));
    QCoreApplication::setApplicationName(QStringLiteral("IM-client"));

    storage::LocalDatabase database;
    dao::WcdbAccountHistoryDao historyDao(database);
    service::AccountHistoryService historyService(historyDao);
    auto loginW = std::make_unique<LoginWindow>(historyService);

    loginW->show();
    return QCoreApplication::exec();
}
