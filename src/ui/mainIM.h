#pragma once
#include <QWidget>
#include "../service/chatService.h"
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
    explicit mainIM(service::ChatService& chat, QWidget* parent = nullptr);
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

    Ui::mainIM* ui;
    service::ChatService& m_chat;
    QLabel* m_status;
    QListWidget* m_conversations;
    QTextBrowser* m_history;
    QTextEdit* m_input;
    QLineEdit* m_peer;
    QPushButton* m_send;
};
