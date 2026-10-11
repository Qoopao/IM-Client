#ifndef ACCOUNT_HISTORY_SERVICE_H
#define ACCOUNT_HISTORY_SERVICE_H

#include "../dao/interfaces/accountHistoryDao.h"

namespace service {

class AccountHistoryService final
{
public:
    explicit AccountHistoryService(dao::AccountHistoryDao& dao)
        : m_dao(dao)
    {
    }

    bool loadAccounts(QList<model::AccountHistory>& accounts, QString& error);
    bool recordSuccessfulLogin(const QString& account, QString& error);
    bool removeAccount(const QString& account, QString& error);

private:
    dao::AccountHistoryDao& m_dao;
};

}

#endif // ACCOUNT_HISTORY_SERVICE_H
