#include "wcdbAccountHistoryDao.h"
#include "../../../storage/wcdb/localDatabase.h"
#include "../../../storage/wcdb/accountHistoryRow.h"

namespace dao {

WcdbAccountHistoryDao::WcdbAccountHistoryDao(storage::LocalDatabase& database)
    : m_database(database)
{
}

bool WcdbAccountHistoryDao::loadAccounts(QList<model::AccountHistory>& accounts, QString& error)
{
    if (!m_database.initialize(error))
        return false;

    using Row = storage::AccountHistoryRow;
    const auto rows = m_database.database().getAllObjects<Row>(storage::accountHistoryTable, {},
        {WCDB_FIELD(Row::lastLoginTime).asOrder(WCDB::Order::DESC),
         WCDB_FIELD(Row::account).asOrder(WCDB::Order::ASC)});
    if (!rows.hasValue()) {
        error = QStringLiteral("无法读取历史账号");
        return false;
    }
    QList<model::AccountHistory> result;
    for (const auto& row : rows.value())
        result.append({QString::fromStdString(row.account), row.lastLoginTime});
    accounts = std::move(result);
    return true;
}

bool WcdbAccountHistoryDao::saveSuccessfulLogin(const model::AccountHistory& account, QString& error)
{
    if (!m_database.initialize(error))
        return false;
    storage::AccountHistoryRow row;
    row.account = account.account.toStdString();
    row.lastLoginTime = account.lastLoginTime;
    if (!m_database.database().insertOrReplaceObject(row, storage::accountHistoryTable)) {
        error = QStringLiteral("无法保存历史账号");
        return false;
    }
    return true;
}

bool WcdbAccountHistoryDao::removeAccount(const QString& account, QString& error)
{
    if (!m_database.initialize(error))
        return false;
    if (!m_database.database().deleteObjects(storage::accountHistoryTable,
        WCDB_FIELD(storage::AccountHistoryRow::account) == account.toStdString())) {
        error = QStringLiteral("无法删除历史账号");
        return false;
    }
    return true;
}

}

