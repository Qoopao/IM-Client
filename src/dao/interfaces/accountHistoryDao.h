#ifndef ACCOUNT_HISTORY_DAO_H
#define ACCOUNT_HISTORY_DAO_H

#include "../../model/accountHistory.h"
#include <QList>

namespace dao {

class AccountHistoryDao
{
public:
    virtual ~AccountHistoryDao() = default;

    // Output arguments are changed only on success. Errors are suitable for display.
    virtual bool loadAccounts(QList<model::AccountHistory>& accounts, QString& error) = 0;
    virtual bool saveSuccessfulLogin(const model::AccountHistory& account, QString& error) = 0;
    virtual bool removeAccount(const QString& account, QString& error) = 0;
};

}

#endif // ACCOUNT_HISTORY_DAO_H
