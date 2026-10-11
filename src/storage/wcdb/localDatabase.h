#ifndef LOCAL_DATABASE_H
#define LOCAL_DATABASE_H

#include "wcdbHeaders.h"
#include <QString>
#include <memory>

namespace storage {

class LocalDatabase final
{
public:
    // An explicit path can be supplied for tests or a separate profile.
    explicit LocalDatabase(QString path = {});
    bool initialize(QString& error);
    WCDB::Database& database();

private:
    QString m_path;
    std::unique_ptr<WCDB::Database> m_database;
    bool m_initialized = false;
};

}

#endif // LOCAL_DATABASE_H
