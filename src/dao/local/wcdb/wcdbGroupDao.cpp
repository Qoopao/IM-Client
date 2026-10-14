#include "wcdbGroupDao.h"
namespace dao {
bool WcdbGroupDao::initialize(QString& error) {
    if (!m_database.initialize(error)) return false;
    if (!m_initialized) m_initialized=m_database.database().createTable<storage::GroupRow>(storage::groupTable);
    if (!m_initialized) error=QStringLiteral("无法初始化群聊缓存");
    return m_initialized;
}
bool WcdbGroupDao::load(const QString& owner, QList<storage::GroupRow>& rows, QString& error) {
    if(!initialize(error)) return false;
    using Row=storage::GroupRow;
    auto result=m_database.database().getAllObjects<Row>(storage::groupTable,WCDB_FIELD(Row::owner)==owner.toStdString());
    if(!result.hasValue()) { error=QStringLiteral("无法读取群聊缓存"); return false; }
    rows.clear(); for(const auto& row:result.value()) rows.append(row); return true;
}
bool WcdbGroupDao::save(const QString& owner,const QString& kind,const QString& id,const QByteArray& data,QString& error) {
    if(!initialize(error)) return false;
    storage::GroupRow row;
    row.key=(owner+":"+kind+":"+id).toStdString(); row.owner=owner.toStdString();
    row.kind=kind.toStdString(); row.payload=data.toBase64().toStdString();
    if(!m_database.database().insertOrReplaceObject(row,storage::groupTable)) { error=QStringLiteral("群聊数据保存失败，尚未确认接收"); return false; }
    return true;
}
bool WcdbGroupDao::remove(const QString& owner,const QString& kind,const QString& id,QString& error) {
    if(!initialize(error)) return false;
    using Row=storage::GroupRow;
    if(!m_database.database().deleteObjects(storage::groupTable,WCDB_FIELD(Row::key)==(owner+":"+kind+":"+id).toStdString())) { error=QStringLiteral("无法更新群请求状态"); return false; }
    return true;
}
}
