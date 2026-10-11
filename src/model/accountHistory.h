#ifndef ACCOUNT_HISTORY_H
#define ACCOUNT_HISTORY_H

#include <QString>
#include <QtGlobal>

namespace model {

struct AccountHistory
{
    QString account;
    qint64 lastLoginTime = 0;
};

}

#endif // ACCOUNT_HISTORY_H
