#include "mainIM.h"
#include <QLabel>
#include <QListWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QDateTime>
#include <QEvent>
#include <QMap>
#include <QSignalBlocker>
#include <QScrollBar>
#include <algorithm>

mainIM::mainIM(service::ChatService& chat, QWidget* parent)
    : QWidget(parent)
    , m_chat(chat)
{
    resize(1000, 700);
    setMinimumSize(720, 500);
    auto* root = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    m_status = new QLabel;
    header->addWidget(m_status, 1);
    auto* logout = new QPushButton(QStringLiteral("退出登录"));
    header->addWidget(logout);
    root->addLayout(header);
    m_error = new QLabel;
    m_error->setWordWrap(true);
    m_error->setStyleSheet("color:#b42318;");
    root->addWidget(m_error);
    auto* start = new QHBoxLayout;
    m_peer = new QLineEdit;
    m_peer->setPlaceholderText(QStringLiteral("输入对方账号"));
    auto* open = new QPushButton(QStringLiteral("发起聊天"));
    start->addWidget(m_peer);
    start->addWidget(open);
    root->addLayout(start);
    auto* splitter = new QSplitter;
    m_conversations = new QListWidget;
    splitter->addWidget(m_conversations);
    auto* right = new QWidget;
    auto* layout = new QVBoxLayout(right);
    m_title = new QLabel(QStringLiteral("选择会话或输入账号发起聊天"));
    layout->addWidget(m_title);
    m_history = new QTextBrowser;
    m_history->setOpenExternalLinks(false);
    layout->addWidget(m_history, 1);
    m_input = new QTextEdit;
    m_input->setPlaceholderText(QStringLiteral("输入文字消息"));
    m_input->setMaximumHeight(130);
    layout->addWidget(m_input);
    m_send = new QPushButton(QStringLiteral("发送"));
    layout->addWidget(m_send, 0, Qt::AlignRight);
    splitter->addWidget(right);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 760});
    root->addWidget(splitter, 1);
    connect(open, &QPushButton::clicked, this, [this] { m_chat.openPeer(m_peer->text()); });
    connect(m_peer, &QLineEdit::returnPressed, open, &QPushButton::click);
    connect(m_conversations, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (item)
            m_chat.openPeer(item->data(Qt::UserRole).toString());
    });
    connect(m_send, &QPushButton::clicked, this, [this] {
        m_error->clear();
        if (m_chat.sendText(m_input->toPlainText()))
            m_input->clear();
    });
    connect(logout, &QPushButton::clicked, this, &mainIM::logoutRequested);
    connect(&chat, &service::ChatService::changed, this, &mainIM::refresh);
    connect(&chat, &service::ChatService::errorOccurred, this,
            [this](const QString& error) { m_error->setText(error); });
    refresh();
}
void mainIM::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ActivationChange)
        m_chat.setViewing(isActiveWindow());
}
void mainIM::refresh()
{
    setWindowTitle(QStringLiteral("IM — %1").arg(m_chat.account()));
    m_status->setText(m_chat.status());
    const auto peer = m_chat.currentPeer();
    m_title->setText(peer.isEmpty() ? QStringLiteral("选择会话或输入账号发起聊天")
                                    : QStringLiteral("与 %1 聊天").arg(peer));
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
