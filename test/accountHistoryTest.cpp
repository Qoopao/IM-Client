#include "../src/storage/wcdb/localDatabase.h"
#include "../src/dao/local/wcdb/wcdbAccountHistoryDao.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>

#define CHECK(condition) do { if (!(condition)) { std::cerr << "Failed at line " << __LINE__ << std::endl; return 1; } } while (false)
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    const auto path = temporary.filePath("nested/client.db");
    QString error;
    QList<model::AccountHistory> accounts;
    {
        storage::LocalDatabase db(path);
        dao::WcdbAccountHistoryDao dao(db);
        CHECK(dao.loadAccounts(accounts, error));
        CHECK(accounts.isEmpty());
        CHECK(dao.saveSuccessfulLogin({"123", 100}, error));
        CHECK(dao.saveSuccessfulLogin({"456", 200}, error));
        CHECK(dao.saveSuccessfulLogin({"123", 300}, error));
        CHECK(dao.loadAccounts(accounts, error));
        CHECK(accounts.size() == 2 && accounts[0].account == "123");
        CHECK(accounts[0].lastLoginTime == 300);
    }
    {
        storage::LocalDatabase db(path);
        dao::WcdbAccountHistoryDao dao(db);
        CHECK(dao.loadAccounts(accounts, error));
        CHECK(accounts.size() == 2 && accounts[0].account == "123");
        CHECK(dao.removeAccount("123", error));
    }
    {
        storage::LocalDatabase db(path);
        dao::WcdbAccountHistoryDao dao(db);
        CHECK(dao.loadAccounts(accounts, error));
        CHECK(accounts.size() == 1 && accounts[0].account == "456");
        CHECK(dao.removeAccount("456", error));
        CHECK(dao.loadAccounts(accounts, error) && accounts.isEmpty());
    }
    QFile blocked(temporary.filePath("blocked"));
    CHECK(blocked.open(QIODevice::WriteOnly));
    blocked.close();
    storage::LocalDatabase badDb(blocked.fileName() + "/client.db");
    dao::WcdbAccountHistoryDao badDao(badDb);
    accounts.append({"preserved", 0});
    CHECK(!badDao.loadAccounts(accounts, error));
    CHECK(!error.isEmpty() && accounts[0].account == "preserved");
    std::cout << "Account history tests passed" << std::endl;
    return 0;
}
