#pragma once
#include "../../../storage/wcdb/localDatabase.h"
#include "../../../storage/wcdb/groupRow.h"
#include <QList>
#include <QByteArray>
namespace dao {
class WcdbGroupDao {
public:
    explicit WcdbGroupDao(storage::LocalDatabase& database) : m_database(database) {}
    bool load(const QString& owner, QList<storage::GroupRow>& rows, QString& error);
    bool save(const QString& owner, const QString& kind, const QString& id, const QByteArray& data, QString& error);
    bool remove(const QString& owner, const QString& kind, const QString& id, QString& error);
private:
    bool initialize(QString& error);
    storage::LocalDatabase& m_database;
    bool m_initialized = false;
};
}
