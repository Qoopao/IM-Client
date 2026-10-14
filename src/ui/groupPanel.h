#pragma once
#include <QWidget>
#include "../service/groupService.h"
namespace Ui { class GroupPanel; }
class GroupPanel : public QWidget {
    Q_OBJECT
public:
    explicit GroupPanel(QWidget* parent=nullptr);
    GroupPanel(service::GroupService& groups,QWidget* parent=nullptr);
    ~GroupPanel() override;
    void bind(service::GroupService& groups);
    void open(const QString& id);
    void refresh();
signals:
    void chatRequested(const QString& id);
private:
    void operation(groupchat::Operation op,const QString& text={});
    void showMembers(const groupchat::Reply& reply);
    void showApplications(const groupchat::Reply& reply);
    Ui::GroupPanel* ui;
    service::GroupService* m_groups=nullptr;
    QString m_groupId;
};
