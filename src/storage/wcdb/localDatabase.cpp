#include "localDatabase.h"
#include "accountHistoryRow.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <utility>

namespace storage {

LocalDatabase::LocalDatabase(QString path)
    : m_path(std::move(path))
{
}

bool LocalDatabase::initialize(QString& error)
{
    error.clear();
    if (m_initialized)
        return true;
    if (m_path.isEmpty()) {
        const auto base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (base.isEmpty()) {
            error = QStringLiteral("无法获取应用数据目录");
            return false;
        }
        m_path = QDir(base).filePath(QStringLiteral("storage/client.db"));
    }
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        error = QStringLiteral("无法创建历史账号数据库目录");
        return false;
    }
    if (!m_database) {
        const auto path = m_path.toUtf8();
        m_database = std::make_unique<WCDB::Database>(path.constData());
    }
    if (!m_database->canOpen()
        || !m_database->createTable<AccountHistoryRow>(accountHistoryTable)) {
        error = QStringLiteral("无法打开或初始化历史账号数据库");
        return false;
    }
    m_initialized = true;
    return true;
}

WCDB::Database& LocalDatabase::database()
{
    return *m_database;
}

}
