#include "../src/dao/local/wcdb/wcdbGroupDao.h"
#include "../src/service/groupService.h"
#include "../src/ui/groupPanel.h"
#include "../src/ui/mainIM.h"
#include <QListWidget>
#include <QPushButton>
#include <QTextBrowser>
#include <QStackedWidget>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>
#include <QTreeWidget>
#include <QApplication>
#include <QTemporaryDir>
#include <QByteArray>
#include <iostream>
void check(bool condition,const QString& message) { if(!condition) throw std::runtime_error(message.toStdString()); }
void checkGroupActions(QApplication& app, const QString& base) {
    for (const auto op : {groupchat::Rename,groupchat::Dismiss,groupchat::Leave}) {
        storage::LocalDatabase database(base+QString::number(op)+".db"); dao::WcdbGroupDao dao(database);
        groupchat::LocalState state; auto* g=state.mutable_group();
        g->set_id("42"); g->set_name("测试群"); g->set_joined(true); g->set_epoch(1); g->set_owner(op==groupchat::Leave?11:10);
        QString error; check(dao.save("10","state","42",QByteArray::fromStdString(state.SerializeAsString()),error),error);
        network::WebSocketSession session; service::GroupService service(session,dao); service.start("10");
        int offlineRequests=0;
        QObject::connect(&service,&service::GroupService::errorOccurred,&service,[&](const QString&) { ++offlineRequests; });
        GroupPanel panel(service); panel.open("42"); app.processEvents();
        check(panel.isWindow() && service.current().isEmpty(),"群资料不应嵌入或修改当前会话");
        auto* rename=panel.findChild<QPushButton*>("renameGroupButton");
        auto* name=panel.findChild<QLineEdit*>("groupNameEdit");
        if(op==groupchat::Rename) {
            auto* leave=panel.findChild<QPushButton*>("leaveGroupButton");
            check(!leave->isEnabled() && !leave->isVisible(),"群主不能使用退出群聊");
            leave->click(); check(!service.busy(groupchat::Leave,"42"),"群主退出请求被发送");
            for(const auto& value : {QString("   "),QStringLiteral(" 测试群 "),QString(100,QChar(0x4E2D))}) {
                name->setText(value); rename->click();
                check(!panel.findChild<QLabel*>("groupStatusLabel")->text().isEmpty() && !service.busy(op,"42"),"无效群名称未拦截");
            }
            name->setText(QStringLiteral("新的群名称"));
        }
        auto* button=op==groupchat::Rename?rename:panel.findChild<QPushButton*>(op==groupchat::Dismiss?"dismissGroupButton":"leaveGroupButton");
        for(const auto answer : {QMessageBox::No,QMessageBox::Yes}) {
            bool shown=false,defaultNo=false;
            QTimer::singleShot(0,[&] {
                for(auto* widget:QApplication::topLevelWidgets()) if(auto* box=qobject_cast<QMessageBox*>(widget); box && box->isVisible()) {
                    shown=true; defaultNo=box->defaultButton()==box->button(QMessageBox::No); box->button(answer)->click();
                }
            });
            button->click();
            check(shown && defaultNo,"群操作缺少二次确认或默认取消");
            check(offlineRequests==int(answer==QMessageBox::Yes),"确认/取消结果未正确处理");
        }
    }
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        QTemporaryDir dir; check(dir.isValid(),QStringLiteral("无法创建测试目录"));
        const auto path=dir.path()+"/group.db";
        checkGroupActions(app,dir.path()+"/actions-");
        groupchat::Message message; message.set_id("message-1"); message.set_clientid("client-1"); message.set_groupid("123"); message.set_sender(10); message.set_nickname("昵称"); message.set_text("中文消息\n第二行"); message.set_seq(1); message.set_time(123456);
        QString error;
        {
            storage::LocalDatabase database(path); dao::WcdbGroupDao dao(database);
            groupchat::LocalState state; state.mutable_group()->set_id("123"); state.mutable_group()->set_name("群聊"); state.mutable_group()->set_epoch(1); state.mutable_group()->set_joined(true); state.set_cursor(1);
            state.mutable_group()->set_number("1000000000"); state.mutable_group()->set_membercount(2);
            check(dao.save("10","state","123",QByteArray::fromStdString(state.SerializeAsString()),error),error);
            check(dao.save("10","message","123:1",QByteArray::fromStdString(message.SerializeAsString()),error),error);
            check(dao.save("11","message","123:1",QByteArray::fromStdString(message.SerializeAsString()),error),error);
            groupchat::Command command; command.set_operation(groupchat::Send); command.set_clientid("durable-send"); command.set_groupid("123"); command.set_text("待确认消息");
            check(dao.save("10","request","durable-send",QByteArray::fromStdString(command.SerializeAsString()),error),error);
            check(dao.save("10","message","123:1",QByteArray::fromStdString(message.SerializeAsString()),error),error);
            QList<storage::GroupRow> rows; check(dao.load("10",rows,error),error); check(rows.size()==3,"重复落盘创建重复消息");
            check(dao.load("11",rows,error),error); check(rows.size()==1,"缓存账号隔离失败");
        }
        {
            storage::LocalDatabase database(path); dao::WcdbGroupDao dao(database); network::WebSocketSession session;
            service::GroupService service(session,dao); GroupPanel panel(service); panel.resize(1000,600); panel.show();
            service.start("10"); app.processEvents();
            dao::WcdbChatDao chatDao(database); service::ChatService chat(session,chatDao);
            dao::RemoteFriendDao remote(session); dao::WcdbFriendDao friendDao(database);
            service::FriendService friends(remote,friendDao); controller::FriendController controller(friends);
            mainIM window(chat,controller,service); window.resize(1100,720); window.show(); app.processEvents();
            auto* list=window.findChild<QListWidget*>("conversationList");
            check(list && list->count()==1,"group missing from shared conversation list");
            auto* rowWidget=list->itemWidget(list->item(0));
            check(rowWidget!=nullptr,"conversation row widget missing");
            for(const auto& labelName : {"nameLabel","previewLabel"}) {
                auto* label=rowWidget->findChild<QLabel*>(labelName);
                check(label && label->height()>=label->fontMetrics().height(),"conversation text clipped vertically");
                check(rowWidget->rect().contains(label->geometry()),"conversation label exceeds row bounds");
            }
            list->setCurrentRow(0);
            app.processEvents();
            auto* stack=window.findChild<QStackedWidget*>("detailStack");
            check(stack->currentWidget()->objectName()=="chatWidget","group did not use shared chat widget");
            check(window.findChild<QTextBrowser*>("historyBrowser")->toPlainText().contains(QString::fromStdString(message.text())),"shared chat missing group history");
            window.grab().save("build/group-shared-chat.png");
            window.findChild<QPushButton*>("groupInfoButton")->click(); app.processEvents();
            auto* popup=window.findChild<GroupPanel*>();
            check(popup && popup->isWindow() && popup->isVisible(),"群资料未弹出独立窗口");
            check(stack->indexOf(popup)==-1 && stack->currentWidget()->objectName()=="chatWidget","群资料仍嵌入主窗口");
            popup->grab().save("build/group-contact-details.png"); popup->close();
            window.findChild<QPushButton*>("contactsButton")->click();
            auto* tree=window.findChild<QTreeWidget*>("contactsTree"); auto* groupItem=tree->topLevelItem(1)->child(0);
            check(QMetaObject::invokeMethod(tree,"itemClicked",Qt::DirectConnection,Q_ARG(QTreeWidgetItem*,groupItem),Q_ARG(int,0)),"联系人群条目点击失败");
            check(popup->isVisible() && service.current()=="123","联系人点击未打开独立资料窗口"); popup->close();
            window.findChild<QPushButton*>("addFriendButton")->click(); app.processEvents();
            check(window.findChild<QLineEdit*>("friendAccountEdit")->maxLength()==10,"lookup must accept ten-digit account/group number");
            check(window.findChild<QPushButton*>("createGroupButton")!=nullptr,"Designer create-group control missing");
            window.grab().save("build/group-add-contact.png");
            check(service.groups().contains("123"),"重启未恢复群资料");
            check(service.messages("123").size()==1,"重启未恢复消息");
            check(service.messages("123").front().text()==message.text(),"中文消息内容损坏");
            check(service.busy(groupchat::Send,"123"),"重启未恢复待确认请求");
            service.open("123"); app.processEvents();
            service.stop(); service.start("11"); app.processEvents();
            check(service.groups().isEmpty(),"切换账号保留了另一账号群资料");
            check(!service.busy(groupchat::Send,"123"),"切换账号保留了另一账号请求");
            check(dao.remove("10","request","durable-send",error),error);
            QList<storage::GroupRow> rows; check(dao.load("10",rows,error),error); check(rows.size()==2,"请求清理误删历史记录");
        }
        std::cout<<"PASS account isolation, persistence, independent group window, rename validation, confirmation/cancellation and owner leave protection\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<"\n"; return 1; }
}
