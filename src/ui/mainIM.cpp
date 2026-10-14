#include "mainIM.h"
#include "ui_mainIM.h"
#include "ui_conversationItem.h"
#include <QMenu>
#include <QSettings>
#include <QLabel>
#include <QListWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QDateTime>
#include <QVariant>
#include <QEvent>
#include <QMap>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QMouseEvent>
#include <QWindow>
#include <QIcon>
#include <QCloseEvent>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

mainIM::mainIM(service::ChatService& chat, controller::FriendController& friends, service::GroupService& groups, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::mainIM)
    , m_chat(chat)
    , m_friends(friends)
    , m_groups(groups)
{
    ui->setupUi(this);
    m_groupPanel=new GroupPanel(groups,this);
    connect(m_groupPanel,&GroupPanel::chatRequested,this,&mainIM::openGroupChat);
    connect(ui->groupInfoButton,&QPushButton::clicked,this,[this] {
        m_groupPanel->open(m_groups.current());
    });
    connect(ui->createGroupButton,&QPushButton::clicked,this,[this] {
        ui->createGroupStatusLabel->clear();
        const auto name=ui->createGroupNameEdit->text().trimmed();
        if(name.isEmpty()) return;
        groupchat::Command c; c.set_operation(groupchat::Create); c.set_text(name.toStdString()); m_groups.command(c);
    });
    connect(ui->myGroupApplicationsButton,&QPushButton::clicked,this,[this] {
        groupchat::Command c; c.set_operation(groupchat::MyApplications); m_groups.command(c);
    });
    connect(ui->groupSearchActionButton,&QPushButton::clicked,this,[this] {
        if(!m_hasGroupSearchResult) return;
        if(m_groupSearchResult.joined()) { openGroupChat(QString::fromStdString(m_groupSearchResult.id())); return; }
        groupchat::Command c; c.set_operation(groupchat::Apply); c.set_groupid(m_groupSearchResult.id()); c.set_text(ui->groupApplicationMessageEdit->text().toStdString()); m_groups.command(c);
    });
    connect(&groups,&service::GroupService::result,this,[this](int op,const groupchat::Reply& reply) {
        if(op==groupchat::Search && reply.groups_size() && QString::fromStdString(reply.groups(0).number())==m_lookupNumber) {
            m_groupSearchResult=reply.groups(0); m_hasGroupSearchResult=true;
            ui->groupSearchStatusLabel->clear(); showGroupSearchResult();
        } else if(op==groupchat::Apply && reply.groupid()==m_groupSearchResult.id()) {
            m_groupApplicationSent=true; ui->groupSearchStatusLabel->setText(QStringLiteral("入群申请已提交，等待群主审核")); showGroupSearchResult();
        } else if(op==groupchat::Create && reply.groups_size()) {
            ui->createGroupNameEdit->clear(); openGroupChat(QString::fromStdString(reply.groups(0).id()));
        } else if(op==groupchat::Send) m_groupStatus=QStringLiteral("消息已由服务端保存");
    });
    connect(&groups,&service::GroupService::operationFailed,this,[this](int op,const QString& group,const QString& error) {
        if(op==groupchat::Search && group==m_lookupNumber) { m_hasGroupSearchResult=false; ui->groupSearchStatusLabel->setText(error); showGroupSearchResult(); }
        else if(op==groupchat::Apply && group==QString::fromStdString(m_groupSearchResult.id())) ui->groupSearchStatusLabel->setText(error);
        else if(op==groupchat::Create) ui->createGroupStatusLabel->setText(error);
        else if(op==groupchat::MyApplications) ui->groupSearchStatusLabel->setText(error);
    });
    connect(&groups,&service::GroupService::errorOccurred,this,[this](const QString& error) { m_groupStatus=error; if(m_groupVisible) m_status->setText(error); });
    connect(&groups,&service::GroupService::changed,this,[this] {
        auto* root=ui->contactsTree->topLevelItem(1);
        QSignalBlocker blocker(ui->contactsTree);
        qDeleteAll(root->takeChildren());
        for(const auto& state:m_groups.groups()) {
            const auto& g=state.group();
            if(!g.joined()) continue;
            auto* item=new QTreeWidgetItem(root,{QString::fromStdString(g.name())+QStringLiteral(" (%1)").arg(QString::fromStdString(g.number()))});
            item->setData(0,Qt::UserRole,QString::fromStdString(g.id()));
            item->setData(0,Qt::UserRole+1,true);
            item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
        }
        if(!root->childCount()) { auto* item=new QTreeWidgetItem(root,{QStringLiteral("暂无群聊")}); item->setFlags(Qt::NoItemFlags); }
        filterContacts(); showGroupSearchResult(); refresh();
    });
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint |
                   Qt::WindowMaximizeButtonHint);
    setWindowIcon(QIcon(":/jpg/ape"));
    installEventFilter(this);
    ui->headerWidget->installEventFilter(this);
    ui->statusLabel->installEventFilter(this);
    ui->minimizeButton->setToolTip(QStringLiteral("最小化"));
    ui->maximizeButton->setToolTip(QStringLiteral("最大化 / 还原"));
    ui->shutdownButton->setToolTip(QStringLiteral("关闭"));
    ui->listStack->setCurrentWidget(ui->conversationPanel);

    m_status = ui->statusLabel;
    m_conversations = ui->conversationList;
    m_conversations->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_conversations, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* item = m_conversations->itemAt(pos);
        if (!item) return;
        const auto id = item->data(Qt::UserRole).toString();
        const bool group = item->data(Qt::UserRole+1).toBool();
        const auto seq = item->data(Qt::UserRole+3).toLongLong();
        const auto time = item->data(Qt::UserRole+4).toLongLong();
        QMenu menu(this);
        menu.setStyleSheet(QStringLiteral("QMenu { background: white; color: #333; border: 1px solid #dce5ee; padding: 4px; } QMenu::item { padding: 7px 20px; } QMenu::item:selected { background: #e6f4ff; }"));
        auto* remove = menu.addAction(QStringLiteral("删除会话"));
        if (menu.exec(m_conversations->viewport()->mapToGlobal(pos)) != remove) return;
        // Local list visibility only: never delete messages or submit a server deletion.
        QSettings settings;
        const auto key = conversationVisibilityKey(id, group);
        settings.setValue(key + "/seq", seq);
        settings.setValue(key + "/time", time);
        if (group == m_groupVisible && id == (group ? m_groups.current() : m_chat.currentPeer()))
            m_conversationClosed = true;
        refresh();
    });
    m_history = ui->historyBrowser;
    m_input = ui->messageEdit;
    m_send = ui->sendButton;

    ui->chatSplitter->setStretchFactor(0, 1);
    ui->chatSplitter->setStretchFactor(1, 4);
    ui->chatSplitter->setSizes({200, 800});
    m_requestsButtonText = ui->friendRequestsButton->text();
    ui->contactsTree->collapseAll();
    ui->contactsTree->viewport()->installEventFilter(this);
    for (int i = 0; i < ui->contactsTree->topLevelItemCount(); ++i) {
        auto* group = ui->contactsTree->topLevelItem(i);
        group->setFlags(Qt::ItemIsEnabled);
        for (int j = 0; j < group->childCount(); ++j)
            group->child(j)->setFlags(Qt::NoItemFlags);
    }

    connect(ui->minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(ui->maximizeButton, &QPushButton::clicked, this, [this] {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
    });
    connect(ui->shutdownButton, &QPushButton::clicked, this, &QWidget::close);

    connect(ui->conversationButton, &QPushButton::clicked, this,
            [this] { ui->listStack->setCurrentWidget(ui->conversationPanel); updateNavigation(); });
    connect(ui->contactsButton, &QPushButton::clicked, this,
            [this] { ui->listStack->setCurrentWidget(ui->contactsPanel); });
    connect(ui->listStack, &QStackedWidget::currentChanged, this, [this](int) { updateNavigation(); });

    connect(ui->contactsSearchEdit, &QLineEdit::textChanged, this, [this] { filterContacts(); });
    connect(ui->friendRequestsButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked)
            m_addFriendVisible = false;
        updateNavigation();
    });
    connect(ui->contactsTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        if(item->parent() && item->data(0,Qt::UserRole+1).toBool()) {
            m_groupPanel->open(item->data(0,Qt::UserRole).toString()); return;
        }
        m_groupVisible=false;
        m_addFriendVisible = false;
        ui->friendRequestsButton->setChecked(false);
        if (item->parent())
            m_selectedFriend = item->data(0, Qt::UserRole).toString();
        showContactDetails();
        updateNavigation();
    });
    connect(ui->addFriendButton, &QPushButton::clicked, this, [this] {
        m_addFriendVisible = true;
        ui->friendRequestsButton->setChecked(false);
        updateNavigation();
    });
    connect(ui->refreshFriendsButton, &QPushButton::clicked, &m_friends, &controller::FriendController::refresh);
    connect(ui->lookupUserButton, &QPushButton::clicked, this, [this] {
        ui->friendResultLabel->clear();
        m_lookupNumber=ui->friendAccountEdit->text().trimmed();
        m_hasGroupSearchResult=false; m_groupApplicationSent=false;
        ui->groupSearchStatusLabel->setText(QStringLiteral("正在查找群聊…")); showGroupSearchResult();
        m_friends.queryUser(m_lookupNumber);
        groupchat::Command c; c.set_operation(groupchat::Search); c.set_groupid(m_lookupNumber.toStdString()); m_groups.command(c);
    });
    connect(ui->friendAccountEdit, &QLineEdit::returnPressed, ui->lookupUserButton, &QPushButton::click);
    connect(ui->sendApplicationButton, &QPushButton::clicked, this, [this] {
        ui->friendResultLabel->clear();
        m_friends.apply(ui->applicationMessageEdit->text());
    });
    connect(ui->friendRequestsList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        m_selectedApplication = item ? item->data(Qt::UserRole).toULongLong() : 0;
        showApplicationDetails();
    });
    connect(ui->acceptApplicationButton, &QPushButton::clicked, this,
            [this] { m_friends.handleApplication(m_selectedApplication, true); });
    connect(ui->rejectApplicationButton, &QPushButton::clicked, this,
            [this] { m_friends.handleApplication(m_selectedApplication, false); });
    connect(ui->deleteFriendButton, &QPushButton::clicked, this, [this] {
        m_friends.deleteFriend(m_selectedFriend);
    });
    connect(ui->messageFriendButton, &QPushButton::clicked, this, [this] {
        m_groupVisible=false;
        restoreConversation(m_selectedFriend, false);
        m_conversationClosed = false;
        if (m_chat.openPeer(m_selectedFriend))
            ui->listStack->setCurrentWidget(ui->conversationPanel);
    });
    connect(&m_friends, &controller::FriendController::changed, this, [this] {
        refreshFriends();
        refresh();
    });
    connect(&m_friends, &controller::FriendController::operationFinished, this, [this](int operation) {
        if (operation == static_cast<int>(friendpb::Apply))
            ui->friendResultLabel->setText(QStringLiteral("好友申请已发送"));
    });

    connect(ui->conversationSearchEdit, &QLineEdit::textChanged, this, &mainIM::filterConversations);
    connect(m_conversations, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (item) m_conversationClosed = false;
        if(item && item->data(Qt::UserRole+1).toBool()) {
            openGroupChat(item->data(Qt::UserRole).toString());
        } else if (item) {
            m_groupVisible=false;
            m_chat.openPeer(item->data(Qt::UserRole).toString());
        }
    });
    connect(m_send, &QPushButton::clicked, this, [this] {
        if (m_groupVisible ? m_groups.sendText(m_input->toPlainText()) : m_chat.sendText(m_input->toPlainText()))
            m_input->clear();
    });
    connect(&chat, &service::ChatService::changed, this, &mainIM::refresh);
    connect(&chat, &service::ChatService::errorOccurred, this,
            [this](const QString& error) { qWarning().noquote() << "ChatService:" << error; });
    refreshFriends();
    showGroupSearchResult();
    updateNavigation();
    refresh();
}

mainIM::~mainIM()
{
    delete ui;
}

void mainIM::updateNavigation()
{
    const bool viewingConversation = ui->listStack->currentWidget() == ui->conversationPanel;
    ui->conversationButton->setChecked(viewingConversation);
    ui->contactsButton->setChecked(!viewingConversation);
    const bool hasConversation = !m_conversationClosed && (m_groupVisible ? !m_groups.current().isEmpty() : !m_chat.currentPeer().isEmpty());
    if (viewingConversation)
        ui->detailStack->setCurrentWidget(hasConversation ? ui->chatWidget : ui->emptyChatWidget);
    else if (m_addFriendVisible)
        ui->detailStack->setCurrentWidget(ui->addFriendPage);
    else if (ui->friendRequestsButton->isChecked())
        ui->detailStack->setCurrentWidget(ui->friendRequestsPage);
    else
        ui->detailStack->setCurrentWidget(ui->contactInfoWidget);
    ui->groupInfoButton->setVisible(m_groupVisible);
    m_chat.setViewing(isActiveWindow() && viewingConversation && hasConversation && !m_groupVisible);
    m_groups.setViewing(isActiveWindow() && viewingConversation && hasConversation && m_groupVisible);

}

void mainIM::refreshFriends()
{
    const auto& state = m_friends.state();
    auto* group = ui->contactsTree->topLevelItem(0);
    {
        QSignalBlocker blocker(ui->contactsTree);
        qDeleteAll(group->takeChildren());
        for (const auto& entry : state.friends) {
            QString name = entry.remark.isEmpty() ? entry.user.nickname : entry.remark;
            if (name.isEmpty())
                name = entry.user.account;
            auto* item = new QTreeWidgetItem(group, {name + QStringLiteral(" (%1)").arg(entry.user.account)});
            item->setData(0, Qt::UserRole, entry.user.account);
            if (entry.user.account == m_selectedFriend)
                ui->contactsTree->setCurrentItem(item);
        }
        if (state.friends.isEmpty()) {
            auto* placeholder = new QTreeWidgetItem(group, {QStringLiteral("暂无好友")});
            placeholder->setFlags(Qt::NoItemFlags);
        }
    }
    filterContacts();
    int pending = 0;
    for (const auto& application : state.applications) {
        if (application.recipient.account == state.account &&
            application.status == model::FriendApplicationStatus::Pending)
            ++pending;
    }
    ui->friendRequestsButton->setText(m_requestsButtonText +
                                      (pending ? QStringLiteral(" (%1)").arg(pending) : QString()));
    {
        QSignalBlocker blocker(ui->friendRequestsList);
        ui->friendRequestsList->clear();
        auto applications = state.applications;
        std::sort(applications.begin(), applications.end(), [](const auto& a, const auto& b) {
            return a.createdTime == b.createdTime ? a.id > b.id : a.createdTime > b.createdTime;
        });
        for (const auto& application : applications) {
            const bool incoming = application.recipient.account == state.account;
            const auto& peer = incoming ? application.sender : application.recipient;
            QString name = peer.nickname.isEmpty() ? peer.account : peer.nickname;
            const auto status = application.status == model::FriendApplicationStatus::Pending ? QStringLiteral("待处理")
                                : application.status == model::FriendApplicationStatus::Accepted
                                    ? QStringLiteral("已接受")
                                    : QStringLiteral("已拒绝");
            auto* item = new QListWidgetItem(QStringLiteral("%1 %2 (%3) · %4\n%5")
                                                 .arg(incoming ? QStringLiteral("收到") : QStringLiteral("发给"), name,
                                                      peer.account, status, application.message),
                                             ui->friendRequestsList);
            item->setData(Qt::UserRole, QVariant::fromValue(application.id));
            if (application.id == m_selectedApplication)
                ui->friendRequestsList->setCurrentItem(item);
        }
        if (!ui->friendRequestsList->currentItem() && ui->friendRequestsList->count())
            ui->friendRequestsList->setCurrentRow(0);
        auto* selected = ui->friendRequestsList->currentItem();
        m_selectedApplication = selected ? selected->data(Qt::UserRole).toULongLong() : 0;
    }
    ui->friendRequestsEmptyLabel->setVisible(state.applications.isEmpty());
    ui->refreshFriendsButton->setEnabled(!state.syncing);
    ui->lookupUserButton->setEnabled(!state.querying && !state.applying && !m_groups.busy(groupchat::Search,m_lookupNumber));
    ui->friendAccountEdit->setEnabled(!state.querying && !state.applying);
    ui->lookupDetailsWidget->setVisible(state.hasSearchResult && state.searchResult.account==m_lookupNumber);
    if (state.hasSearchResult) {
        ui->lookupUserNameLabel->setText(state.searchResult.nickname.isEmpty() ? state.searchResult.account
                                                                               : state.searchResult.nickname);
        ui->lookupUserAccountLabel->setText(QStringLiteral("账号：%1").arg(state.searchResult.account));
    }
    bool alreadyFriend = false;
    for (const auto& entry : state.friends)
        alreadyFriend = alreadyFriend || entry.user.account == state.searchResult.account;
    ui->sendApplicationButton->setText(alreadyFriend ? QStringLiteral("已是好友") : QStringLiteral("发送好友申请"));
    ui->sendApplicationButton->setEnabled(state.hasSearchResult && !state.applying && !alreadyFriend);
    ui->applicationMessageEdit->setEnabled(!state.applying);
    showContactDetails();
    showApplicationDetails();
}

void mainIM::filterConversations()
{
    const auto keyword = ui->conversationSearchEdit->text().trimmed();
    for (int i = 0; i < m_conversations->count(); ++i) {
        auto *item = m_conversations->item(i);
        item->setHidden(!keyword.isEmpty() && !item->data(Qt::UserRole+5).toString().contains(keyword, Qt::CaseInsensitive) &&
                        !item->data(Qt::UserRole).toString().contains(keyword) && !item->data(Qt::UserRole+2).toString().contains(keyword));
    }
}

void mainIM::filterContacts()
{
    const auto keyword = ui->contactsSearchEdit->text().trimmed();
    for (int i = 0; i < ui->contactsTree->topLevelItemCount(); ++i) {
        auto* group = ui->contactsTree->topLevelItem(i);
        const auto key = group->text(0);
        if (!keyword.isEmpty() && !m_expandedContactGroups.contains(key))
            m_expandedContactGroups.insert(key, group->isExpanded());
        bool hasMatch = false;
        for (int j = 0; j < group->childCount(); ++j) {
            auto* item = group->child(j);
            const bool matches = keyword.isEmpty() || group->text(0).contains(keyword, Qt::CaseInsensitive) ||
                                 item->text(0).contains(keyword, Qt::CaseInsensitive);
            item->setHidden(!matches);
            hasMatch = hasMatch || matches;
        }
        group->setHidden(!keyword.isEmpty() && !hasMatch);
        if (!keyword.isEmpty() && hasMatch)
            group->setExpanded(true);
        else if (keyword.isEmpty() && m_expandedContactGroups.contains(key))
            group->setExpanded(m_expandedContactGroups.value(key));
    }
    if (keyword.isEmpty())
        m_expandedContactGroups.clear();
}

void mainIM::showContactDetails()
{
    bool found = false;
    for (const auto& entry : m_friends.state().friends) {
        if (entry.user.account != m_selectedFriend)
            continue;
        found = true;
        ui->contactNameLabel->setText(entry.user.nickname.isEmpty() ? entry.user.account : entry.user.nickname);
        ui->contactAccountLabel->setText(QStringLiteral("账号：%1").arg(entry.user.account));
        ui->contactRemarkLabel->setText(entry.remark.isEmpty() ? QString()
                                                               : QStringLiteral("备注：%1").arg(entry.remark));
        break;
    }
    ui->contactDetailsWidget->setVisible(found);
    const bool deleting = m_friends.state().deleting.contains(m_selectedFriend);
    ui->deleteFriendButton->setEnabled(found && !deleting && !m_friends.state().syncing);
    ui->messageFriendButton->setEnabled(found && !deleting);
    if (!found)
        m_selectedFriend.clear();
}

void mainIM::showApplicationDetails()
{
    bool found = false;
    const auto& state = m_friends.state();
    for (const auto& application : state.applications) {
        if (application.id != m_selectedApplication)
            continue;
        found = true;
        const bool incoming = application.recipient.account == state.account;
        const bool pending = application.status == model::FriendApplicationStatus::Pending;
        ui->applicationDetailsLabel->setText(QStringLiteral("申请说明：%1").arg(application.message));
        ui->acceptApplicationButton->setVisible(incoming && pending);
        ui->rejectApplicationButton->setVisible(incoming && pending);
        ui->acceptApplicationButton->setEnabled(!state.handling.contains(application.id) && !state.syncing);
        ui->rejectApplicationButton->setEnabled(!state.handling.contains(application.id) && !state.syncing);
        break;
    }
    ui->applicationActionsWidget->setVisible(found);
}

bool mainIM::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->contactsTree->viewport() &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        auto* item = ui->contactsTree->itemAt(mouseEvent->position().toPoint());
        if (mouseEvent->button() == Qt::LeftButton && item && !item->parent() && item->childCount() > 0) {
            if (event->type() == QEvent::MouseButtonPress) {
                m_addFriendVisible = false;
                ui->friendRequestsButton->setChecked(false);
                updateNavigation();
                item->setExpanded(!item->isExpanded());
                ui->contactsTree->setFocus();
            }
            return true;
        }
    }
    if (watched == this && event->type() == QEvent::MouseButtonPress && !isMaximized()) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && windowHandle()) {
            const auto pos = mouseEvent->position().toPoint();
            Qt::Edges edges;
            if (pos.x() < 6)
                edges |= Qt::LeftEdge;
            if (pos.x() >= width() - 6)
                edges |= Qt::RightEdge;
            if (pos.y() < 6)
                edges |= Qt::TopEdge;
            if (pos.y() >= height() - 6)
                edges |= Qt::BottomEdge;
            if (edges)
                return windowHandle()->startSystemResize(edges);
        }
    }
    if (watched == ui->headerWidget || watched == ui->statusLabel) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && windowHandle())
                return windowHandle()->startSystemMove();
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                ui->maximizeButton->click();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void mainIM::closeEvent(QCloseEvent* event)
{
    QWidget::closeEvent(event);
    if (event->isAccepted())
        emit closing();
}

void mainIM::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ActivationChange)
        updateNavigation();
}

void mainIM::refresh()
{
    setWindowTitle(QStringLiteral("IM — %1").arg(m_chat.account()));
    m_status->setText(m_groupVisible && !m_groupStatus.isEmpty() ? m_groupStatus : m_chat.status());
    const auto peer = m_chat.currentPeer();
    auto displayName = [this](const QString& account) {
        const auto nickname = m_friends.nickname(account);
        return nickname.isEmpty() ? (account == m_chat.account() ? QStringLiteral("我") : QStringLiteral("用户"))
                                  : nickname;
    };
    const auto group=m_groups.groups().value(m_groups.current()).group();
    ui->chatPeerLabel->setText(m_groupVisible ? QString::fromStdString(group.name())+(group.dismissed()?QStringLiteral("（群已解散）"):!group.joined()?QStringLiteral("（已退出或被移出）"):QString()) : peer.isEmpty()?QStringLiteral("请选择会话"):displayName(peer));
    const bool writable=!m_conversationClosed && (m_groupVisible ? group.joined() && !group.dismissed() : !peer.isEmpty());
    m_send->setEnabled(writable); m_input->setEnabled(writable);
    updateNavigation();
    struct Summary
    {
        QString preview;
        qint64 time = 0;
        int unread = 0;
        bool pinned = false;
        qint64 seq = 0;
    };
    QMap<QString, Summary> summaries;
    QList<model::ChatMessage> messages = m_chat.messages();
    std::stable_sort(messages.begin(), messages.end(),
                     [](const auto& a, const auto& b) { return a.time == b.time ? a.seq < b.seq : a.time < b.time; });
    for (const auto& m : messages) {
        auto& summary = summaries[m.peer];
        summary.seq = std::max(summary.seq, m.seq);
        summary.time = m.time;
        summary.preview = m.text.simplified().left(35);
        summary.unread += m.unread ? 1 : 0;
    }
    for (const auto& item : m_chat.conversations()) {
        const auto p = QString::number(item.userid());
        if (p.isEmpty() || p == m_chat.account())
            continue;
        auto& summary = summaries[p];
        summary.seq = std::max(summary.seq, item.maxseq());
        summary.pinned = item.ispinned();
        summary.unread = static_cast<int>(item.unreadcount());
        if (item.has_latestmsg() && item.latestmsg().latestmsgrecvtime() >= summary.time) {
            summary.time = item.latestmsg().latestmsgrecvtime();
            const auto raw = QByteArray::fromStdString(item.latestmsg().content());
            const auto json = QJsonDocument::fromJson(raw);
            summary.preview = json.isObject() && json.object().value("content").isString()
                                  ? json.object().value("content").toString().simplified().left(35)
                                  : QString::fromUtf8(raw).simplified().left(35);
        }
    }
    if (!peer.isEmpty() && !summaries.contains(peer))
        summaries.insert(peer, {});
    struct Row { QString id,name,preview,number; qint64 time=0; quint64 unread=0; bool pinned=false,isGroup=false; qint64 seq=0; };
    QList<Row> rows;
    for(auto it=summaries.cbegin();it!=summaries.cend();++it) rows.append({it.key(),displayName(it.key()),it->preview,{},it->time,static_cast<quint64>(it->unread),it->pinned,false,it->seq});
    for(const auto& state:m_groups.groups()) {
        const auto& g=state.group(); Row row; row.id=QString::fromStdString(g.id()); row.number=QString::fromStdString(g.number()); row.name=QStringLiteral("[群] %1").arg(QString::fromStdString(g.name())); row.isGroup=true; row.unread=g.dismissed()?0:g.unread(); row.seq=static_cast<qint64>(g.lastseq());
        if(g.dismissed()) row.name+=QStringLiteral("（群已解散）"); else if(!g.joined()) row.name+=QStringLiteral("（已退出或被移出）");
        const auto history=m_groups.messages(row.id);
        if(!history.isEmpty()) { row.time=history.last().time(); row.preview=QString::fromStdString(history.last().text()).simplified().left(35); }
        rows.append(row);
    }
    std::stable_sort(rows.begin(),rows.end(),[](const Row& a,const Row& b) { if(a.pinned!=b.pinned) return a.pinned; if(a.time!=b.time) return a.time>b.time; return a.name<b.name; });
    {
        QSignalBlocker blocker(m_conversations); m_conversations->clear();
        for(const auto& row:rows) {
            QSettings settings;
            const auto key = conversationVisibilityKey(row.id, row.isGroup);
            if (settings.contains(key + "/seq")) {
                if (row.seq <= settings.value(key + "/seq").toLongLong() &&
                    row.time <= settings.value(key + "/time").toLongLong()) continue;
                settings.remove(key); // New messages restore a locally hidden conversation.
            }
            const auto title = row.name+(row.unread?QStringLiteral(" (%1 条未读)").arg(row.unread):QString());
            auto* item=new QListWidgetItem(m_conversations);
            item->setData(Qt::UserRole+5,title+"\n"+row.preview);
            auto* widget = new QWidget(m_conversations);
            Ui::conversationItem itemUi;
            itemUi.setupUi(widget);
            itemUi.nameLabel->setText(title);
            itemUi.previewLabel->setText(row.preview);
            widget->setAttribute(Qt::WA_TransparentForMouseEvents);
            item->setSizeHint(QSize(0, 64));
            m_conversations->setItemWidget(item, widget);
            item->setData(Qt::UserRole+3,row.seq); item->setData(Qt::UserRole+4,row.time);
            item->setData(Qt::UserRole,row.id); item->setData(Qt::UserRole+1,row.isGroup); item->setData(Qt::UserRole+2,row.number);
            if(row.isGroup==m_groupVisible && row.id==(m_groupVisible?m_groups.current():peer)) m_conversations->setCurrentItem(item);
        }
    }
    filterConversations();
    QString html;
    if(m_groupVisible) {
        for(const auto& message:m_groups.messages(m_groups.current())) {
            const bool own=message.sender()==m_groups.account().toULongLong();
            html+=QStringLiteral("<p align='%1'><b>%2 (%3)</b> <span style='color:#777'>%4</span><br>%5</p>")
                .arg(own?"right":"left",QString::fromStdString(message.nickname()).toHtmlEscaped(),QString::number(message.sender()),QDateTime::fromMSecsSinceEpoch(message.time()).toString("yyyy-MM-dd HH:mm:ss"),QString::fromStdString(message.text()).toHtmlEscaped().replace("\n","<br>"));
        }
        for(const auto& pending:m_groups.pendingMessages(m_groups.current())) html+=QStringLiteral("<p align='right'><b>我 · 待确认</b><br>%1</p>").arg(QString::fromStdString(pending.text()).toHtmlEscaped().replace("\n","<br>"));
    } else {
        for(const auto& message:messages) if(message.peer==peer)
            html+=QStringLiteral("<p align='%4'><b>%1</b> <span style='color:#777'>%2</span><br>%3</p>")
                .arg(displayName(message.outgoing?m_chat.account():message.peer).toHtmlEscaped(),QDateTime::fromMSecsSinceEpoch(message.time).toString("yyyy-MM-dd HH:mm:ss"),message.text.toHtmlEscaped().replace("\n","<br>"),message.outgoing?QStringLiteral("right"):QStringLiteral("left"));
    }
    const auto target=m_groupVisible?"g:"+m_groups.current():"u:"+peer;
    const auto scroll = m_history->verticalScrollBar()->value();
    const bool bottom = scroll >= m_history->verticalScrollBar()->maximum() - 10;
    const auto oldPeer = m_history->property("peer").toString();
    m_history->setHtml(html);
    m_history->setProperty("peer", target);
    m_history->verticalScrollBar()->setValue(bottom || oldPeer != target ? m_history->verticalScrollBar()->maximum()
                                                                       : scroll);
}

QString mainIM::conversationVisibilityKey(const QString& id, bool group) const {
    return "localConversations/" + m_chat.account() + "/" + (group ? "group/" : "single/") + id;
}
void mainIM::restoreConversation(const QString& id, bool group) {
    QSettings settings;
    settings.remove(conversationVisibilityKey(id, group));
}
void mainIM::openGroupChat(const QString& id) {
    restoreConversation(id, true);
    m_conversationClosed = false;
    ui->friendRequestsButton->setChecked(false);
    m_groupVisible=true; m_addFriendVisible=false;
    ui->listStack->setCurrentWidget(ui->conversationPanel); m_groups.open(id); updateNavigation(); refresh();
}
void mainIM::showGroupSearchResult() {
    if(m_hasGroupSearchResult && m_groups.groups().contains(QString::fromStdString(m_groupSearchResult.id())))
        m_groupSearchResult=m_groups.groups().value(QString::fromStdString(m_groupSearchResult.id())).group();
    for(int i=0;i<ui->groupSearchResultLayout->count();++i)
        if(auto* widget=ui->groupSearchResultLayout->itemAt(i)->widget())
            widget->setVisible(m_hasGroupSearchResult);
    ui->myGroupApplicationsButton->setEnabled(!m_groups.busy(groupchat::MyApplications));
    ui->createGroupButton->setEnabled(!m_groups.busy(groupchat::Create));
    ui->lookupUserButton->setEnabled(!m_friends.state().querying && !m_friends.state().applying && !m_groups.busy(groupchat::Search,m_lookupNumber));
    if(!m_hasGroupSearchResult) return;
    const auto& g=m_groupSearchResult;
    ui->searchGroupNameLabel->setText(QString::fromStdString(g.name()));
    ui->searchGroupNumberLabel->setText(QStringLiteral("群号：%1").arg(QString::fromStdString(g.number())));
    ui->searchGroupMembersLabel->setText(QStringLiteral("成员：%1 人%2").arg(g.membercount()).arg(g.dismissed()?QStringLiteral(" · 群已解散"):QString()));
    ui->groupApplicationMessageEdit->setVisible(!g.joined() && !g.dismissed());
    ui->groupSearchActionButton->setText(g.dismissed()?QStringLiteral("群已解散"):g.joined()?QStringLiteral("发消息"):m_groupApplicationSent?QStringLiteral("已申请"):QStringLiteral("申请加入"));
    ui->groupSearchActionButton->setEnabled(!g.dismissed() && (!m_groupApplicationSent || g.joined()) && !m_groups.busy(groupchat::Apply,QString::fromStdString(g.id())));
}
