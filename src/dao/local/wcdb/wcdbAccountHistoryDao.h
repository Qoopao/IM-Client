#ifndef WCDB_ACCOUNT_HISTORY_DAO_H
#define WCDB_ACCOUNT_HISTORY_DAO_H

#include "../../interfaces/accountHistoryDao.h"

namespace storage {

class LocalDatabase;

}
namespace dao {

class WcdbAccountHistoryDao final : public AccountHistoryDao
{
public:
    explicit WcdbAccountHistoryDao(storage::LocalDatabase& database);
    bool loadAccounts(QList<model::AccountHistory>& accounts, QString& error) override;
    bool saveSuccessfulLogin(const model::AccountHistory& account, QString& error) override;
    bool removeAccount(const QString& account, QString& error) override;

private:
    storage::LocalDatabase& m_database;
};

}

#endif // WCDB_ACCOUNT_HISTORY_DAO_H
