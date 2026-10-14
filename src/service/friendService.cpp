#include "friendService.h"
#include <QDebug>
#include <QRegularExpression>
#include <QTimer>

namespace service {
namespace {

model::FriendUser user(const sdkws::PublicUserInfo& value)
{
    return {QString::number(value.userid()), QString::fromStdString(value.nickname()),
            QString::fromStdString(value.faceurl())};
}

}

FriendService::FriendService(dao::RemoteFriendDao& remote, dao::WcdbFriendDao& local, QObject* parent)
    : QObject(parent)
    , m_remote(remote)
    , m_local(local)
{
    connect(&remote, &dao::RemoteFriendDao::refreshRequested, this, &FriendService::refresh);
    connect(this, &FriendService::errorOccurred, this,
            [](const QString& error) { qWarning().noquote() << "FriendService:" << error; });
}

const model::FriendModel& FriendService::state() const
{
    return m_state;
}

void FriendService::start(const QString& account)
{
    stop();
    m_state.account = account;
    friendpb::Reply cache;
    QString error;
    if (m_local.load(account, cache, error))
        loadState(cache);
    else
        emit errorOccurred(error);
    emit changed();
    refresh();
}

void FriendService::stop()
{
    m_remote.cancel();
    m_nicknames.clear();
    m_requestedNicknames.clear();
    m_snapshot.Clear();
    m_state = {};
    m_refreshAgain = false;
}

bool FriendService::successful(const friendpb::Reply& reply)
{
    if (reply.errorcode() == "0")
        return true;
    emit errorOccurred(QString::fromStdString(reply.errorcode() + ": " + reply.errormessage()));
    return false;
}

void FriendService::loadState(const friendpb::Reply& snapshot)
{
    for (const auto& cleared : snapshot.clearedconversations())
        emit conversationCleared(QString::number(cleared.peer()), cleared.deletedseq());
    m_state.friends.clear();
    m_state.applications.clear();
    for (const auto& entry : snapshot.friends()) {
        model::FriendEntry friendEntry;
        friendEntry.user = {QString::number(entry.frienduser().userid()),
                            QString::fromStdString(entry.frienduser().nickname()),
                            QString::fromStdString(entry.frienduser().faceurl())};
        friendEntry.remark = QString::fromStdString(entry.remark());
        friendEntry.createdTime = entry.createtime();
        m_state.friends.append(friendEntry);
    }
    for (const auto& entry : snapshot.applications()) {
        model::FriendApplication application;
        application.id = entry.id();
        application.sender = user(entry.sender());
        application.recipient = user(entry.recipient());
        application.message = QString::fromStdString(entry.message());
        application.status = static_cast<model::FriendApplicationStatus>(entry.status());
        application.createdTime = entry.createdtime();
        application.handledTime = entry.handledtime();
        m_state.applications.append(application);
    }
}

void FriendService::refresh()
{
    if (m_state.account.isEmpty())
        return;
    if (m_state.syncing) {
        m_refreshAgain = true;
        return;
    }
    m_state.syncing = true;
    m_snapshot.Clear();
    emit changed();
    pullPage(friendpb::ListFriends);
}

void FriendService::pullPage(friendpb::Operation operation, quint64 cursor, quint64 snapshot)
{
    friendpb::Command command;
    command.set_actor(m_state.account.toULongLong());
    command.set_operation(operation);
    command.set_after(cursor);
    command.set_snapshot(snapshot);
    command.set_limit(100);
    m_remote.execute(command, [this, operation, cursor](const friendpb::Reply& reply) {
        if (!successful(reply)) {
            finishSync(false);
            return;
        }
        if (operation == friendpb::ListFriends) {
            if (cursor == 0)
                for (const auto& cleared : reply.clearedconversations())
                    m_snapshot.add_clearedconversations()->CopyFrom(cleared);
            for (const auto& entry : reply.friends())
                m_snapshot.add_friends()->CopyFrom(entry);
        } else {
            for (const auto& entry : reply.applications())
                m_snapshot.add_applications()->CopyFrom(entry);
        }
        if (reply.hasmore()) {
            if (reply.cursor() <= cursor) {
                emit errorOccurred(QStringLiteral("好友分页游标无效"));
                finishSync(false);
                return;
            }
            pullPage(operation, reply.cursor(), reply.snapshot());
        } else if (operation == friendpb::ListFriends) {
            pullPage(friendpb::ListApplications);
        } else {
            finishSync(true);
        }
    });
}

void FriendService::finishSync(bool success)
{
    m_state.syncing = false;
    if (success) {
        QString error;
        if (m_local.save(m_state.account, m_snapshot, error))
            loadState(m_snapshot);
        else
            emit errorOccurred(error);
    }
    m_snapshot.Clear();
    const bool refreshAgain = success && m_refreshAgain;
    m_refreshAgain = false;
    emit changed();
    if (refreshAgain)
        QTimer::singleShot(0, this, &FriendService::refresh);
}

QString FriendService::nickname(const QString& account)
{
    if (account.isEmpty() || m_state.account.isEmpty())
        return {};
    for (const auto& entry : m_state.friends)
        if (entry.user.account == account && !entry.user.nickname.isEmpty())
            return entry.user.nickname;
    for (const auto& application : m_state.applications) {
        if (application.sender.account == account && !application.sender.nickname.isEmpty())
            return application.sender.nickname;
        if (application.recipient.account == account && !application.recipient.nickname.isEmpty())
            return application.recipient.nickname;
    }
    if (m_state.hasSearchResult && m_state.searchResult.account == account &&
        !m_state.searchResult.nickname.isEmpty())
        return m_state.searchResult.nickname;
    if (m_nicknames.contains(account))
        return m_nicknames.value(account);
    if (!m_requestedNicknames.contains(account)) {
        m_requestedNicknames.insert(account);
        friendpb::Command command;
        command.set_operation(friendpb::LookupUser);
        command.set_target(account.toULongLong());
        m_remote.execute(command, [this, account](const friendpb::Reply& reply) {
            if (reply.errorcode() == "0" && reply.has_user() &&
                reply.user().userid() == account.toULongLong()) {
                m_nicknames.insert(account, QString::fromStdString(reply.user().nickname()));
                QTimer::singleShot(0, this, [this] { emit changed(); });
            } else {
                emit errorOccurred(QStringLiteral("Nickname lookup failed: ") +
                                   QString::fromStdString(reply.errormessage()));
            }
        });
    }
    return {};
}

void FriendService::queryUser(const QString& account)
{
    if (m_state.querying)
        return;
    m_state.hasSearchResult = false;
    m_state.searchResult = {};
    const auto input = account.trimmed();
    bool ok = false;
    const auto number = input.toULongLong(&ok);
    if (!ok || !number || !QRegularExpression(QStringLiteral("^[0-9]+$")).match(input).hasMatch() ||
        QString::number(number) == m_state.account) {
        emit errorOccurred(QStringLiteral("请输入有效的对方账号"));
        return;
    }
    m_state.hasSearchResult = false;
    m_state.querying = true;
    emit changed();
    friendpb::Command command;
    command.set_operation(friendpb::LookupUser);
    command.set_target(number);
    m_remote.execute(command, [this](const friendpb::Reply& reply) {
        m_state.querying = false;
        if (successful(reply) && reply.has_user() && reply.user().userid()) {
            m_state.searchResult = user(reply.user());
            m_state.hasSearchResult = true;
            emit operationFinished(static_cast<int>(friendpb::LookupUser));
        }
        emit changed();
    });
}

void FriendService::apply(const QString& message)
{
    if (m_state.applying || !m_state.hasSearchResult)
        return;
    if (message.toUtf8().size() > 512) {
        emit errorOccurred(QStringLiteral("申请说明不能超过 512 字节"));
        return;
    }
    m_state.applying = true;
    emit changed();
    friendpb::Command command;
    command.set_operation(friendpb::Apply);
    command.set_target(m_state.searchResult.account.toULongLong());
    command.set_message(message.toStdString());
    m_remote.execute(command, [this](const friendpb::Reply& reply) {
        m_state.applying = false;
        if (successful(reply)) {
            emit operationFinished(static_cast<int>(friendpb::Apply));
            refresh();
        } else if (reply.errorcode() != "OFFLINE") {
            refresh();
        }
        emit changed();
    });
}

void FriendService::handleApplication(quint64 id, bool accept)
{
    if (m_state.handling.contains(id))
        return;
    bool allowed = false;
    for (const auto& application : m_state.applications) {
        if (application.id == id && application.recipient.account == m_state.account &&
            application.status == model::FriendApplicationStatus::Pending)
            allowed = true;
    }
    if (!allowed)
        return;
    m_state.handling.insert(id);
    emit changed();
    friendpb::Command command;
    command.set_operation(friendpb::HandleApplication);
    command.set_applicationid(id);
    command.set_accept(accept);
    m_remote.execute(command, [this, id](const friendpb::Reply& reply) {
        m_state.handling.remove(id);
        if (successful(reply)) {
            emit operationFinished(static_cast<int>(friendpb::HandleApplication));
            refresh();
        } else if (reply.errorcode() != "OFFLINE") {
            refresh();
        }
        emit changed();
    });
}

void FriendService::deleteFriend(const QString& account)
{
    if (m_state.deleting.contains(account))
        return;
    bool exists = false;
    for (const auto& entry : m_state.friends)
        exists = exists || entry.user.account == account;
    if (!exists)
        return;
    m_state.deleting.insert(account);
    emit changed();
    friendpb::Command command;
    command.set_operation(friendpb::DeleteFriend);
    command.set_target(account.toULongLong());
    m_remote.execute(command, [this, account](const friendpb::Reply& reply) {
        m_state.deleting.remove(account);
        if (successful(reply)) {
            for (const auto& cleared : reply.clearedconversations())
                emit conversationCleared(QString::number(cleared.peer()), cleared.deletedseq());
            emit operationFinished(static_cast<int>(friendpb::DeleteFriend));
            refresh();
        } else if (reply.errorcode() != "OFFLINE") {
            refresh();
        }
        emit changed();
    });
}

}
