#include "groupService.h"
#include "../proto/sdkws.pb.h"
#include <QDateTime>
#include <QUuid>
#include <algorithm>
namespace service {
namespace {
template<class T> QByteArray bytes(const T& value) { return QByteArray::fromStdString(value.SerializeAsString()); }
bool mutation(groupchat::Operation op) {
    return op==groupchat::Create || op==groupchat::Apply || op==groupchat::Review || op==groupchat::Rename || op==groupchat::Leave || op==groupchat::Kick || op==groupchat::Dismiss || op==groupchat::Send;
}
}
GroupService::GroupService(network::WebSocketSession& session,dao::WcdbGroupDao& storage,QObject* parent)
    : QObject(parent),m_session(session),m_storage(storage) {
    connect(&session,&network::WebSocketSession::responseReceived,this,&GroupService::receive);
    connect(&session,&network::WebSocketSession::connectionLost,this,[this] {
        if(m_account.isEmpty()) return;
        for(auto it=m_pending.begin();it!=m_pending.end();) {
            if(!it->durable) it=m_pending.erase(it); else { it->deadline=0; ++it; }
        }
        emit errorOccurred(QStringLiteral("连接已断开，群请求保留，重新登录后确认结果")); emit changed();
    });
    m_timer.setInterval(1000);
    connect(&m_timer,&QTimer::timeout,this,[this] {
        if(m_account.isEmpty() || !m_session.isOnline()) return;
        const auto time=QDateTime::currentMSecsSinceEpoch();
        for(auto it=m_pending.begin();it!=m_pending.end();) {
            if(it->deadline>time) { ++it; continue; }
            if(it->durable) {
                it->deadline=time+12000;
                m_session.sendRequest(250,bytes(it->command),it.key()); ++it;
            } else {
                const auto op=it->command.operation();
                if(op!=groupchat::List && op!=groupchat::Pull && op!=groupchat::Ack && op!=groupchat::Read)
                    emit errorOccurred(QStringLiteral("群请求超时，请重试"));
                it=m_pending.erase(it); emit changed();
            }
        }
        if(time>=m_nextRefresh) { m_nextRefresh=time+10000; refresh(); }
    });
}
void GroupService::start(const QString& account) {
    stop(); m_account=account;
    QString error; QList<storage::GroupRow> rows;
    if(!m_storage.load(account,rows,error)) emit errorOccurred(error);
    for(const auto& row:rows) {
        const auto data=QByteArray::fromBase64(QByteArray::fromStdString(row.payload));
        if(row.kind=="state") {
            groupchat::LocalState state;
            if(state.ParseFromArray(data.constData(),data.size())) m_groups.insert(QString::fromStdString(state.group().id()),state);
            else emit errorOccurred(QStringLiteral("群资料缓存损坏"));
        } else if(row.kind=="message") {
            groupchat::Message message;
            if(message.ParseFromArray(data.constData(),data.size())) m_messages[QString::fromStdString(message.groupid())].insert(message.seq(),message);
            else emit errorOccurred(QStringLiteral("群消息缓存损坏"));
        } else if(row.kind=="request") {
            groupchat::Command cmd;
            if(cmd.ParseFromArray(data.constData(),data.size())) {
                Pending p; p.command=cmd; p.durable=true;
                m_pending.insert(QString::fromStdString(cmd.clientid()),p);
            }
        }
    }
    // A saved cursor is trusted only if its entire current membership range is present.
    for(auto it=m_groups.begin();it!=m_groups.end();++it) {
        quint64 cursor=it->group().joinseq(); const auto& messages=m_messages[it.key()];
        while(messages.contains(cursor+1)) ++cursor;
        it->set_cursor(cursor);
    }
    m_timer.start(); refresh(); emit changed();
}
void GroupService::stop() {
    m_timer.stop(); m_pending.clear(); m_groups.clear(); m_messages.clear(); m_account.clear(); m_current.clear(); m_viewing=false; m_nextRefresh=0; m_refreshAgain=false;
}
bool GroupService::busy(groupchat::Operation op,const QString& group) const {
    for(const auto& p:m_pending) if(p.command.operation()==op && QString::fromStdString(p.command.groupid())==group) return true;
    return false;
}
bool GroupService::request(groupchat::Command cmd,bool durable) {
    if(m_account.isEmpty() || !m_session.isOnline()) { emit errorOccurred(QStringLiteral("请重新登录后操作")); return false; }
    cmd.set_actor(m_account.toULongLong());
    if(cmd.clientid().empty()) cmd.set_clientid(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
    if(!cmd.limit()) cmd.set_limit(50);
    QString id=QString::fromStdString(cmd.clientid()),error;
    if(durable && !m_storage.save(m_account,"request",id,bytes(cmd),error)) { emit errorOccurred(error); return false; }
    Pending p; p.command=cmd; p.deadline=QDateTime::currentMSecsSinceEpoch()+12000; p.durable=durable;
    m_pending.insert(id,p);
    const bool sent=m_session.sendRequest(250,bytes(cmd),id);
    if(!sent && !durable) m_pending.remove(id);
    emit changed(); return sent || durable;
}
bool GroupService::command(groupchat::Command cmd) {
    const auto group=QString::fromStdString(cmd.groupid());
    if(cmd.operation()!=groupchat::Send && busy(cmd.operation(),group)) return false;
    if(m_groups.contains(group)) cmd.set_epoch(m_groups[group].group().epoch());
    return request(cmd,mutation(cmd.operation()));
}
void GroupService::refresh() {
    if(busy(groupchat::List)) { m_refreshAgain=true; return; }
    groupchat::Command cmd; cmd.set_operation(groupchat::List); request(cmd);
}
void GroupService::open(const QString& id) { m_current=id; sync(id); markRead(); emit changed(); }
void GroupService::setViewing(bool viewing) { m_viewing=viewing; if(viewing) markRead(); }
QList<groupchat::Message> GroupService::messages(const QString& id) const { return m_messages.value(id).values(); }
QList<groupchat::Command> GroupService::pendingMessages(const QString& id) const {
    QList<groupchat::Command> result;
    for(const auto& p:m_pending) if(p.command.operation()==groupchat::Send && QString::fromStdString(p.command.groupid())==id) {
        bool saved=false;
        for(const auto& message:m_messages.value(id)) if(message.sender()==m_account.toULongLong() && message.clientid()==p.command.clientid()) { saved=true; break; }
        if(!saved) result.append(p.command);
    }
    return result;
}
bool GroupService::sendText(const QString& text) {
    if(text.trimmed().isEmpty() || m_current.isEmpty()) return false;
    if(!m_groups.contains(m_current) || !m_groups[m_current].group().joined()) { emit errorOccurred(QStringLiteral("当前群已解散或你已退出")); return false; }
    if(text.toUtf8().size()>65536) { emit errorOccurred(QStringLiteral("消息不能超过64 KiB")); return false; }
    groupchat::Command cmd; cmd.set_operation(groupchat::Send); cmd.set_groupid(m_current.toStdString()); cmd.set_text(text.toStdString()); return command(cmd);
}
bool GroupService::saveState(const QString& id) {
    QString error;
    if(!m_storage.save(m_account,"state",id,bytes(m_groups[id]),error)) { emit errorOccurred(error); return false; }
    return true;
}
bool GroupService::storeMessage(const groupchat::Message& message) {
    const auto id=QString::fromStdString(message.groupid());
    if(m_messages[id].contains(message.seq())) return true;
    QString error;
    if(!m_storage.save(m_account,"message",id+":"+QString::number(message.seq()),bytes(message),error)) { emit errorOccurred(error); return false; }
    m_messages[id].insert(message.seq(),message); return true;
}
bool GroupService::advance(const QString& id) {
    if(!m_groups.contains(id)) return false;
    auto& state=m_groups[id]; quint64 cursor=std::max<quint64>(state.cursor(),state.group().joinseq());
    while(m_messages[id].contains(cursor+1)) ++cursor;
    const auto old=state.cursor(); state.set_cursor(cursor);
    quint64 unread=0;
    for(const auto& message:m_messages[id])
        if(message.seq()>std::max(state.group().joinseq(),state.group().readseq()) && message.sender()!=m_account.toULongLong()) ++unread;
    if(cursor>=state.group().lastseq()) state.mutable_group()->set_unread(unread);
    if(!saveState(id)) { state.set_cursor(old); return false; }
    return true;
}
void GroupService::acknowledge(const QString& id) {
    if(!m_groups.contains(id) || !m_groups[id].group().joined() || busy(groupchat::Ack,id)) return;
    groupchat::Command cmd; cmd.set_operation(groupchat::Ack); cmd.set_groupid(id.toStdString());
    cmd.set_after(m_groups[id].cursor()); cmd.set_epoch(m_groups[id].group().epoch()); request(cmd);
}
void GroupService::sync(const QString& id) {
    if(!m_groups.contains(id) || !m_groups[id].group().joined() || busy(groupchat::Pull,id)) return;
    groupchat::Command cmd; cmd.set_operation(groupchat::Pull); cmd.set_groupid(id.toStdString());
    cmd.set_after(m_groups[id].cursor()); cmd.set_epoch(m_groups[id].group().epoch()); request(cmd);
}
void GroupService::markRead() {
    if(!m_viewing || !m_groups.contains(m_current) || !m_groups[m_current].group().joined() || busy(groupchat::Read,m_current)) return;
    auto& state=m_groups[m_current];
    if(state.cursor()<=state.group().readseq()) return;
    groupchat::Command cmd; cmd.set_operation(groupchat::Read); cmd.set_groupid(m_current.toStdString()); cmd.set_after(state.cursor()); cmd.set_epoch(state.group().epoch()); request(cmd);
}
void GroupService::receive(const QByteArray& data) {
    if(m_account.isEmpty()) return;
    sdkws::SdkWSResp frame;
    if(!frame.ParseFromArray(data.constData(),data.size()) || frame.account()!=m_account.toULongLong()) return;
    if(frame.type()==251) {
        groupchat::Notice notice; if(!notice.ParseFromString(frame.data())) return;
        const auto id=QString::fromStdString(notice.groupid());
        if(!m_groups.contains(id) || notice.refresh() || notice.epoch()!=m_groups[id].group().epoch()) { refresh(); return; }
        if(!m_groups[id].group().joined()) return;
        m_groups[id].mutable_group()->set_lastseq(std::max(m_groups[id].group().lastseq(),notice.lastseq()));
        for(const auto& message:notice.messages()) if(message.seq()>m_groups[id].group().joinseq() && !storeMessage(message)) return;
        if(!advance(id)) return;
        acknowledge(id);
        if(m_groups[id].cursor()<notice.lastseq()) sync(id);
        markRead(); emit changed(); return;
    }
    if(frame.type()!=250) return;
    auto it=m_pending.find(QString::fromStdString(frame.requestid()));
    if(it==m_pending.end()) return;
    groupchat::Reply reply;
    if(!reply.ParseFromString(frame.data())) { emit errorOccurred(QStringLiteral("群响应内容无效")); return; }
    Pending pending=*it; const auto cmd=pending.command; const auto op=cmd.operation(); const auto id=QString::fromStdString(cmd.groupid());
    if(reply.errorcode()!="0") {
        const auto error=QString::fromStdString(reply.errormessage());
        emit operationFailed(op,id,error);
        if(op!=groupchat::Search) emit errorOccurred(error);
        if(pending.durable && reply.errorcode()=="DB_ERROR") { it->deadline=QDateTime::currentMSecsSinceEpoch()+12000; return; }
        if(pending.durable) { QString error; if(!m_storage.remove(m_account,"request",QString::fromStdString(cmd.clientid()),error)) { emit errorOccurred(error); return; } }
        m_pending.erase(it);
        if(op==groupchat::List) { m_refreshAgain=false; m_nextRefresh=QDateTime::currentMSecsSinceEpoch()+10000; }
        else if(op!=groupchat::Search) refresh();
        emit changed(); return;
    }
    // Pulls and push notifications can cross membership changes. Discard stale pages.
    if((op==groupchat::Pull || op==groupchat::Ack || op==groupchat::Read) &&
       (!m_groups.contains(id) || !m_groups[id].group().joined() || cmd.epoch()!=m_groups[id].group().epoch())) {
        m_pending.erase(it); return;
    }
    if(op==groupchat::Pull || op==groupchat::Send) {
        for(const auto& message:reply.messages()) if(!storeMessage(message)) return;
        if(m_groups.contains(id) && !advance(id)) return;
    }
    // Publish lists only after all pages succeed; keep the old cache on failures.
    if(op==groupchat::List || op==groupchat::Members || op==groupchat::Applications || op==groupchat::MyApplications) {
        for(const auto& g:reply.groups()) *pending.pages.add_groups()=g;
        for(const auto& m:reply.members()) *pending.pages.add_members()=m;
        for(const auto& a:reply.applications()) *pending.pages.add_applications()=a;
        if(reply.hasmore()) {
            pending.command.set_after(reply.cursor()); pending.command.set_snapshot(reply.snapshot());
            const auto requestId=QUuid::createUuid().toString(QUuid::WithoutBraces);
            pending.command.set_clientid(requestId.toStdString()); pending.deadline=QDateTime::currentMSecsSinceEpoch()+12000;
            m_pending.erase(it); m_pending.insert(requestId,pending);
            m_session.sendRequest(250,bytes(pending.command),requestId); return;
        }
        reply=pending.pages; reply.set_errorcode("0"); reply.set_groupid(cmd.groupid());
    }
    if(pending.durable) {
        QString error;
        if(!m_storage.remove(m_account,"request",QString::fromStdString(cmd.clientid()),error)) { emit errorOccurred(error); return; }
    }
    m_pending.remove(QString::fromStdString(frame.requestid()));
    if(op==groupchat::List || op==groupchat::Create || op==groupchat::Search) {
        for(const auto& g:reply.groups()) {
            const auto key=QString::fromStdString(g.id());
            if(op==groupchat::Search && !g.joined() && !m_groups.contains(key)) continue;
            auto state=m_groups.value(key);
            const auto oldRead=state.group().epoch()==g.epoch()?state.group().readseq():0;
            if(state.group().epoch()!=g.epoch()) state.set_cursor(g.joinseq());
            *state.mutable_group()=g;
            state.mutable_group()->set_readseq(std::max(oldRead,g.readseq()));
            m_groups.insert(key,state); advance(key);
        }
        markRead();
        if(op==groupchat::List && m_refreshAgain) { m_refreshAgain=false; refresh(); }
        for(const auto& key:m_groups.keys()) {
            if(m_groups[key].group().joined()) {
                if(m_groups[key].cursor()<m_groups[key].group().lastseq()) sync(key);
                else acknowledge(key);
            }
        }
    }
    if(op==groupchat::Pull) {
        acknowledge(id);
        if(reply.hasmore()) {
            auto next=cmd; next.set_after(reply.cursor()); next.set_snapshot(reply.snapshot()); next.clear_clientid(); request(next);
        } else if(m_groups[id].cursor()<m_groups[id].group().lastseq()) sync(id);
        markRead();
    } else if(op==groupchat::Read) {
        auto& state=m_groups[id]; state.mutable_group()->set_readseq(std::max(state.group().readseq(),cmd.after()));
        quint64 unread=0; for(const auto& message:m_messages[id]) if(message.seq()>state.group().readseq() && message.sender()!=m_account.toULongLong()) ++unread;
        state.mutable_group()->set_unread(unread); saveState(id); markRead();
    } else if(pending.durable && op!=groupchat::Create) refresh();
    emit result(op,reply); emit changed();
}
}
