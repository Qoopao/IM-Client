#include "wcdbFriendDao.h"
#include "../../../storage/wcdb/localDatabase.h"
#include "../../../storage/wcdb/friendSnapshotRow.h"
#include <QByteArray>

namespace dao {

WcdbFriendDao::WcdbFriendDao(storage::LocalDatabase& database)
    : m_database(database)
{
}

bool WcdbFriendDao::initialize(QString& error)
{
    if (!m_database.initialize(error))
        return false;
    if (!m_initialized)
        m_initialized = m_database.database().createTable<storage::FriendSnapshotRow>(storage::friendSnapshotTable);
    if (!m_initialized)
        error = QStringLiteral("无法初始化好友缓存");
    return m_initialized;
}

bool WcdbFriendDao::load(const QString& owner, friendpb::Reply& snapshot, QString& error)
{
    if (!initialize(error))
        return false;
    using Row = storage::FriendSnapshotRow;
    const auto rows = m_database.database().getAllObjects<Row>(storage::friendSnapshotTable,
                                                               WCDB_FIELD(Row::owner) == owner.toStdString());
    if (!rows.hasValue()) {
        error = QStringLiteral("无法读取好友缓存");
        return false;
    }
    snapshot.Clear();
    if (rows.value().empty())
        return true;
    const auto bin = QByteArray::fromBase64(QByteArray::fromStdString(rows.value().front().payload));
    if (!snapshot.ParseFromArray(bin.constData(), bin.size())) {
        error = QStringLiteral("好友缓存内容无效");
        return false;
    }
    return true;
}

bool WcdbFriendDao::save(const QString& owner, const friendpb::Reply& snapshot, QString& error)
{
    if (!initialize(error))
        return false;
    storage::FriendSnapshotRow row;
    row.owner = owner.toStdString();
    row.payload = QByteArray::fromStdString(snapshot.SerializeAsString()).toBase64().toStdString();
    if (!m_database.database().insertOrReplaceObject(row, storage::friendSnapshotTable)) {
        error = QStringLiteral("无法保存好友缓存");
        return false;
    }
    return true;
}

}
