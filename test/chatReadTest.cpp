#include "../src/service/chatService.h"
#include "../src/storage/wcdb/localDatabase.h"
#include "../src/proto/sdkws.pb.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

// In-memory transport: exercise real ChatService/SQLite without a login or server.
static QList<conversation::MarkConversationReadReq> reads;
namespace network {
WebSocketSession::WebSocketSession(QObject* parent) : QObject(parent) {}
WebSocketSession::~WebSocketSession() = default;
void WebSocketSession::open(uint64_t) {}
void WebSocketSession::close() {}
void WebSocketSession::setAuthenticated(const QString&) {}
bool WebSocketSession::isOnline() const { return true; }
bool WebSocketSession::sendRequest(int type, const QByteArray& data, const QString&) {
    if (type == 241) {
        conversation::MarkConversationReadReq request;
        if (!request.ParseFromArray(data.constData(), data.size())) return false;
        reads.append(request);
    }
    return true;
}
}
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void snapshot(network::WebSocketSession& session, qint64 max, qint64 read = 0, bool exists = true) {
    conversation::GetAllConversationsResp list;
    if (exists) {
        auto* item = list.add_conversations();
        item->set_owneruserid(1); item->set_userid(2337); item->set_conversationtype(1);
        item->set_conversationid("1_2337"); item->set_maxseq(max); item->set_readseq(read);
    }
    sdkws::SdkWSResp frame; frame.set_type(240); frame.set_errorcode("0"); frame.set_data(list.SerializeAsString());
    emit session.responseReceived(QByteArray::fromStdString(frame.SerializeAsString()));
}
int main(int argc, char** argv) {
    QCoreApplication app(argc,argv);
    try {
        QTemporaryDir dir; check(dir.isValid(),"temporary database unavailable");
        storage::LocalDatabase database(dir.path()+"/read.db"); dao::WcdbChatDao dao(database);
        QString error;
        for (qint64 seq : {3LL,9LL,15LL}) {
            model::ChatMessage message; message.id=QString::number(seq); message.serverId=message.id;
            message.peer="2337"; message.conversation="1_2337"; message.seq=seq;
            message.status=seq==15 ? 3 : 2; message.unread=true;
            check(dao.save("0001",message,error),"cache save failed");
        }
        network::WebSocketSession session; service::ChatService chat(session,dao);
        chat.start("0001"); chat.openPeer("02337"); chat.setViewing(true);
        check(reads.isEmpty(),"cached conversation submitted read before server snapshot");
        snapshot(session,0,0,false);
        check(reads.isEmpty(),"missing server conversation submitted read");
        snapshot(session,3);
        check(reads.size()==1 && reads.back().conversationid()=="1_2337" && reads.back().readseq()==3,
              "read exceeded server watermark or used a noncanonical ID");
        chat.openPeer("2337"); snapshot(session,3,3);
        check(reads.size()==1,"reopening already-read conversation repeated read");
        chat.setViewing(false); snapshot(session,9); check(reads.size()==1,"hidden chat marked read");
        chat.setViewing(true);
        check(reads.size()==2 && reads.back().readseq()==9,"new confirmed watermark was not read");
        snapshot(session,15,9);
        check(reads.size()==2,"failed/unconfirmed message advanced read");
        chat.stop(); reads.clear(); chat.start("0001"); chat.openPeer("2337"); chat.setViewing(true);
        snapshot(session,15,9);
        check(reads.isEmpty(),"restart resubmitted server-confirmed read");
        std::cout << "PASS missing conversation, stale cache, server watermark, canonical IDs, reopen, visibility and restart reads\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
