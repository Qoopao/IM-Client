#include "mainIM.h"
#include "ui_mainIM.h"
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
#include <QEvent>
#include <QMap>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QMouseEvent>
#include <QWindow>
#include <QIcon>
#include <QCloseEvent>
#include <QDebug>
#include <algorithm>

mainIM::mainIM(service::ChatService& chat, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::mainIM)
    , m_chat(chat)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint
                   | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
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
    m_history = ui->historyBrowser;
    m_input = ui->messageEdit;
    m_peer = ui->peerEdit;
    m_send = ui->sendButton;

    auto* open = ui->openChatButton;
    ui->chatSplitter->setStretchFactor(0, 1);
    ui->chatSplitter->setStretchFactor(1, 4);
    ui->chatSplitter->setSizes({200, 800});
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

    connect(ui->conversationButton, &QPushButton::clicked, this, [this] {
        ui->listStack->setCurrentWidget(ui->conversationPanel);
    });
    connect(ui->contactsButton, &QPushButton::clicked, this, [this] {
        ui->listStack->setCurrentWidget(ui->contactsPanel);
    });
    connect(ui->listStack, &QStackedWidget::currentChanged, this, [this](int) {
        updateNavigation();
    });

    connect(ui->contactsSearchEdit, &QLineEdit::textChanged, this,
            [this, expandedGroups = QMap<QTreeWidgetItem*, bool>()](const QString& text) mutable {
                const auto keyword = text.trimmed();
                for (int i = 0; i < ui->contactsTree->topLevelItemCount(); ++i) {
                    auto* group = ui->contactsTree->topLevelItem(i);
                    if (!keyword.isEmpty() && !expandedGroups.contains(group))
                        expandedGroups.insert(group, group->isExpanded());
                    bool hasMatch = false;
                    const bool groupMatches = group->text(0).contains(keyword, Qt::CaseInsensitive);
                    for (int j = 0; j < group->childCount(); ++j) {
                        auto* item = group->child(j);
                        const bool matches = keyword.isEmpty() || groupMatches
                            || item->text(0).contains(keyword, Qt::CaseInsensitive);
                        item->setHidden(!matches);
                        hasMatch = hasMatch || matches;
                    }
                    group->setHidden(!keyword.isEmpty() && !hasMatch);
                    if (!keyword.isEmpty() && hasMatch)
                        group->setExpanded(true);
                    else if (keyword.isEmpty() && expandedGroups.contains(group))
                        group->setExpanded(expandedGroups.value(group));
                }
                if (keyword.isEmpty())
                    expandedGroups.clear();
            });

    connect(ui->friendRequestsButton, &QPushButton::toggled, this, [this](bool) {
        updateNavigation();
    });
    connect(ui->contactsTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem*, int) {
        ui->friendRequestsButton->setChecked(false);
    });

    connect(open, &QPushButton::clicked, this, [this] { m_chat.openPeer(m_peer->text()); });
    connect(m_peer, &QLineEdit::returnPressed, open, &QPushButton::click);
    connect(m_conversations, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (item)
            m_chat.openPeer(item->data(Qt::UserRole).toString());
    });
    connect(m_send, &QPushButton::clicked, this, [this] {
        if (m_chat.sendText(m_input->toPlainText()))
            m_input->clear();
    });
    connect(&chat, &service::ChatService::changed, this, &mainIM::refresh);
    connect(&chat, &service::ChatService::errorOccurred, this,
            [this](const QString& error) {
                qWarning().noquote() << "ChatService:" << error;
            });
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
    const bool hasConversation = !m_chat.currentPeer().isEmpty();
    if (viewingConversation)
        ui->detailStack->setCurrentWidget(hasConversation ? ui->chatWidget : ui->emptyChatWidget);
    else if (ui->friendRequestsButton->isChecked())
        ui->detailStack->setCurrentWidget(ui->friendRequestsPage);
    else
        ui->detailStack->setCurrentWidget(ui->contactInfoWidget);
    m_chat.setViewing(isActiveWindow() && viewingConversation && hasConversation);
}

bool mainIM::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->contactsTree->viewport()
        && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        auto* item = ui->contactsTree->itemAt(mouseEvent->position().toPoint());
        if (mouseEvent->button() == Qt::LeftButton && item && !item->parent() && item->childCount() > 0) {
            if (event->type() == QEvent::MouseButtonPress) {
                ui->friendRequestsButton->setChecked(false);
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
        m_chat.setViewing(isActiveWindow() && ui->listStack->currentWidget() == ui->conversationPanel
                          && !m_chat.currentPeer().isEmpty());
}

void mainIM::refresh()
{
    setWindowTitle(QStringLiteral("IM — %1").arg(m_chat.account()));
    m_status->setText(m_chat.status());
    const auto peer = m_chat.currentPeer();
    ui->chatPeerLabel->setText(peer.isEmpty() ? QStringLiteral("请选择会话") : peer);
    m_send->setEnabled(!peer.isEmpty());
    m_input->setEnabled(!peer.isEmpty());
    struct Summary
    {
        QString preview;
        qint64 time = 0;
        int unread = 0;
    };
    QMap<QString, Summary> summaries;
    QList<model::ChatMessage> messages = m_chat.messages();
    std::stable_sort(messages.begin(), messages.end(),
                     [](const auto& a, const auto& b) { return a.time == b.time ? a.seq < b.seq : a.time < b.time; });
    for (const auto& m : messages) {
        auto& summary = summaries[m.peer];
        summary.time = m.time;
        summary.preview = m.text.simplified().left(35);
        summary.unread += m.unread ? 1 : 0;
    }
    if (!peer.isEmpty() && !summaries.contains(peer))
        summaries.insert(peer, {});
    auto peers = summaries.keys();
    std::stable_sort(peers.begin(), peers.end(),
                     [&](const auto& a, const auto& b) { return summaries[a].time > summaries[b].time; });
    {
        QSignalBlocker blocker(m_conversations);
        m_conversations->clear();
        for (const auto& p : peers) {
            const auto& summary = summaries[p];
            auto* item = new QListWidgetItem(
                p + (summary.unread ? QStringLiteral(" (%1 条未读)").arg(summary.unread) : QString()) + "\n" +
                    summary.preview,
                m_conversations);
            item->setData(Qt::UserRole, p);
            if (p == peer)
                m_conversations->setCurrentItem(item);
        }
    }
    QString html;
    for (const auto& m : messages)
        if (m.peer == peer) {
            QString state;
            if (m.outgoing)
                state = m.status == 1   ? QStringLiteral("发送中")
                        : m.status == 2 ? QStringLiteral("服务端已接收")
                                        : QStringLiteral("发送失败或结果未确认");
            html += QStringLiteral("<p><b>%1</b> <span style='color:#777'>%2 %3</span><br>%4</p>")
                        .arg(m.outgoing ? QStringLiteral("我") : m.peer.toHtmlEscaped(),
                             QDateTime::fromMSecsSinceEpoch(m.time).toString("yyyy-MM-dd HH:mm:ss"), state,
                             m.text.toHtmlEscaped().replace("\n", "<br>"));
        }
    const auto scroll = m_history->verticalScrollBar()->value();
    const bool bottom = scroll >= m_history->verticalScrollBar()->maximum() - 10;
    const auto oldPeer = m_history->property("peer").toString();
    m_history->setHtml(html);
    m_history->setProperty("peer", peer);
    m_history->verticalScrollBar()->setValue(bottom || oldPeer != peer ? m_history->verticalScrollBar()->maximum()
                                                                       : scroll);
}
