#pragma once
#include <QWidget>
#include "../service/chatService.h"
#include "../controller/friendController.h"
#include "groupPanel.h"
#include <QMap>
class QLabel;
class QListWidget;
class QTextBrowser;
class QTextEdit;
class QLineEdit;
class QPushButton;
namespace Ui {
class mainIM;
}

class mainIM : public QWidget
{
    Q_OBJECT
public:
    explicit mainIM(service::ChatService& chat, controller::FriendController& friends, service::GroupService& groups, QWidget* parent = nullptr);
    ~mainIM() override;
    void refresh();
signals:
    void closing();

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateNavigation();
    void refreshFriends();
    void filterConversations();
    void filterContacts();
    void showContactDetails();
    void showApplicationDetails();
    void showGroupSearchResult();
    void openGroupChat(const QString& id);
    QString conversationVisibilityKey(const QString& id, bool group) const;
    void restoreConversation(const QString& id, bool group);

    Ui::mainIM* ui;
    service::ChatService& m_chat;
    controller::FriendController& m_friends;
    service::GroupService& m_groups;
    GroupPanel* m_groupPanel;
    bool m_groupVisible = false;
    bool m_conversationClosed = false;
    bool m_hasGroupSearchResult = false;
    bool m_groupApplicationSent = false;
    groupchat::Group m_groupSearchResult;
    QString m_lookupNumber;
    QString m_groupStatus;
    QString m_selectedFriend;
    quint64 m_selectedApplication = 0;
    QString m_requestsButtonText;
    bool m_addFriendVisible = false;
    QMap<QString, bool> m_expandedContactGroups;
    QLabel* m_status;
    QListWidget* m_conversations;
    QTextBrowser* m_history;
    QTextEdit* m_input;
    QPushButton* m_send;
};
