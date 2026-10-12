#pragma once
#include <QWidget>
#include "../service/chatService.h"
class QLabel;
class QListWidget;
class QTextBrowser;
class QTextEdit;
class QLineEdit;
class QPushButton;
class mainIM : public QWidget
{
    Q_OBJECT
public:
    explicit mainIM(service::ChatService& chat, QWidget* parent = nullptr);
    void refresh();
signals:
    void logoutRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    service::ChatService& m_chat;
    QLabel *m_status, *m_title, *m_error;
    QListWidget* m_conversations;
    QTextBrowser* m_history;
    QTextEdit* m_input;
    QLineEdit* m_peer;
    QPushButton* m_send;
};
