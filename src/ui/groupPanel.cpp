#include "groupPanel.h"
#include "ui_groupPanel.h"
#include <QListWidget>
#include "confirmationDialog.h"

GroupPanel::GroupPanel(QWidget* parent):QWidget(parent),ui(new Ui::GroupPanel) {
    ui->setupUi(this);
    setWindowFlag(Qt::Window);
    connect(ui->membersBackButton,&QPushButton::clicked,this,[this] { ui->groupDetailsStack->setCurrentWidget(ui->groupSummaryPage); });
    connect(ui->applicationsBackButton,&QPushButton::clicked,this,[this] { ui->groupDetailsStack->setCurrentWidget(ui->groupSummaryPage); });
    connect(ui->myApplicationsBackButton,&QPushButton::clicked,this,&QWidget::close);
}
GroupPanel::GroupPanel(service::GroupService& groups,QWidget* parent):GroupPanel(parent) { bind(groups); }
GroupPanel::~GroupPanel() { delete ui; }
void GroupPanel::bind(service::GroupService& groups) {
    if(m_groups) return;
    m_groups=&groups;
    connect(ui->groupMessageButton,&QPushButton::clicked,this,[this] { emit chatRequested(m_groupId); close(); });
    connect(ui->groupMembersButton,&QPushButton::clicked,this,[this] { operation(groupchat::Members); });
    connect(ui->groupApplicationsButton,&QPushButton::clicked,this,[this] { operation(groupchat::Applications); });
    connect(ui->renameGroupButton,&QPushButton::clicked,this,[this] { operation(groupchat::Rename,ui->groupNameEdit->text().trimmed()); });
    connect(ui->leaveGroupButton,&QPushButton::clicked,this,[this] {
        operation(groupchat::Leave);
    });
    connect(ui->dismissGroupButton,&QPushButton::clicked,this,[this] {
        operation(groupchat::Dismiss);
    });
    connect(ui->groupMembersList,&QListWidget::currentItemChanged,this,[this] { refresh(); });
    connect(ui->groupApplicationsList,&QListWidget::currentItemChanged,this,[this] { refresh(); });
    connect(ui->kickMemberButton,&QPushButton::clicked,this,[this] {
        auto* item=ui->groupMembersList->currentItem(); if(!item) return;
        if(ui::confirm(this,QStringLiteral("移出成员"),QStringLiteral("确认将选中成员移出群聊？"))!=QMessageBox::Yes) return;
        groupchat::Command c; c.set_operation(groupchat::Kick); c.set_groupid(m_groupId.toStdString()); c.set_target(item->data(Qt::UserRole).toULongLong()); m_groups->command(c);
    });
    auto review=[this](bool accept) {
        auto* item=ui->groupApplicationsList->currentItem(); if(!item) return;
        groupchat::Command c; c.set_operation(groupchat::Review); c.set_groupid(m_groupId.toStdString()); c.set_applicationid(item->data(Qt::UserRole).toULongLong()); c.set_accept(accept); m_groups->command(c);
    };
    connect(ui->acceptGroupApplicationButton,&QPushButton::clicked,this,[review] { review(true); });
    connect(ui->rejectGroupApplicationButton,&QPushButton::clicked,this,[review] { review(false); });
    connect(&groups,&service::GroupService::changed,this,&GroupPanel::refresh);
    connect(&groups,&service::GroupService::operationFailed,this,[this](int op,const QString& id,const QString& error) {
        if(id==m_groupId || op==groupchat::MyApplications) ui->groupStatusLabel->setText(error);
    });
    connect(&groups,&service::GroupService::result,this,[this](int op,const groupchat::Reply& reply) {
        if(op==groupchat::Members && reply.groupid()==m_groupId.toStdString()) showMembers(reply);
        else if(op==groupchat::Applications && reply.groupid()==m_groupId.toStdString()) showApplications(reply);
        else if(op==groupchat::MyApplications) {
            ui->groupStatusLabel->clear(); setWindowTitle(QStringLiteral("我的入群申请"));
            ui->myGroupApplicationsList->clear();
            const QStringList states={QStringLiteral("待审核"),QStringLiteral("已同意"),QStringLiteral("已拒绝"),QStringLiteral("已关闭")};
            for(const auto& a:reply.applications())
                ui->myGroupApplicationsList->addItem(QStringLiteral("%1 · 群号 %2 · %3\n%4").arg(QString::fromStdString(a.groupname()),QString::fromStdString(a.groupnumber()),states.value(a.status()),QString::fromStdString(a.reason())));
            ui->myGroupApplicationsEmptyLabel->setVisible(!reply.applications_size());
            ui->groupDetailsStack->setCurrentWidget(ui->myGroupApplicationsPage);
            show(); raise(); activateWindow();
        } else if(reply.groupid()==m_groupId.toStdString()) {
            if(op==groupchat::Review) { ui->groupStatusLabel->setText(QStringLiteral("入群申请已处理")); operation(groupchat::Applications); }
            else if(op==groupchat::Kick) { ui->groupStatusLabel->setText(QStringLiteral("成员已移出")); operation(groupchat::Members); }
            else if(op==groupchat::Rename || op==groupchat::Leave || op==groupchat::Dismiss) ui->groupStatusLabel->setText(QStringLiteral("操作已完成"));
        }
    });
    refresh();
}
void GroupPanel::open(const QString& id) {
    if(!m_groups) return;
    ui->groupStatusLabel->clear(); ui->groupMembersList->clear(); ui->groupApplicationsList->clear();
    m_groupId=id;
    setWindowTitle(QStringLiteral("群资料"));
    ui->groupDetailsStack->setCurrentWidget(ui->groupSummaryPage); refresh();
    show(); raise(); activateWindow();
}
void GroupPanel::operation(groupchat::Operation op,const QString& text) {
    if(!m_groups || m_groupId.isEmpty() || m_groups->busy(op,m_groupId)) return;
    const auto g=m_groups->groups().value(m_groupId).group();
    const bool active=g.joined() && !g.dismissed(),owner=g.owner()==m_groups->account().toULongLong();
    const auto name=text.trimmed();
    if(op==groupchat::Rename || op==groupchat::Dismiss) {
        if(!active || !owner) return;
    }
    if(op==groupchat::Rename) {
        if(name.isEmpty() || name.toUtf8().size()>256) {
            ui->groupStatusLabel->setText(QStringLiteral("群名称不能为空，且不能超过 256 字节")); return;
        }
        if(name==QString::fromStdString(g.name()).trimmed()) {
            ui->groupStatusLabel->setText(QStringLiteral("新群名称不能与当前名称相同")); return;
        }
        if(ui::confirm(this,QStringLiteral("修改群名称"),QStringLiteral("确认将群名称从“%1”修改为“%2”？").arg(QString::fromStdString(g.name()),name))!=QMessageBox::Yes) return;
    } else if(op==groupchat::Leave) {
        if(!active) return;
        if(owner) { ui->groupStatusLabel->setText(QStringLiteral("群主不能退出群聊，请使用解散群聊")); return; }
        if(ui::confirm(this,QStringLiteral("退出群聊"),QStringLiteral("确认退出“%1”？退出后保留本地历史，但无法再发送或拉取消息。").arg(QString::fromStdString(g.name())))!=QMessageBox::Yes) return;
    } else if(op==groupchat::Dismiss) {
        if(ui::confirm(this,QStringLiteral("解散群聊"),QStringLiteral("确认解散“%1”？所有成员将无法继续聊天，本地历史保留。").arg(QString::fromStdString(g.name())))!=QMessageBox::Yes) return;
    }
    groupchat::Command c; c.set_operation(op); c.set_groupid(m_groupId.toStdString()); c.set_text(name.toStdString());
    if(!m_groups->command(c)) ui->groupStatusLabel->setText(QStringLiteral("操作未提交，请检查连接状态后重试"));
}
void GroupPanel::refresh() {
    if(!m_groups) return;
    const auto id=m_groupId; const auto g=m_groups->groups().value(id).group();
    const bool found=m_groups->groups().contains(id),active=found && g.joined() && !g.dismissed(),owner=active && g.owner()==m_groups->account().toULongLong();
    ui->groupNameLabel->setText(found?QString::fromStdString(g.name()):QStringLiteral("请选择群聊"));
    ui->groupNumberLabel->setText(QStringLiteral("群号：%1").arg(g.number().empty()?QStringLiteral("同步中"):QString::fromStdString(g.number())));
    ui->groupMemberCountLabel->setText(QStringLiteral("成员：%1 人").arg(g.membercount()));
    ui->groupStateLabel->setText(g.dismissed()?QStringLiteral("群已解散，历史记录保留"):!g.joined()?QStringLiteral("已退出或被移出，历史记录保留"):owner?QStringLiteral("我的身份：群主"):QStringLiteral("我的身份：群成员"));
    ui->groupMessageButton->setEnabled(found); ui->groupMembersButton->setEnabled(active && !m_groups->busy(groupchat::Members,id));
    ui->groupNameEdit->setVisible(owner);
    ui->renameGroupButton->setVisible(owner);
    ui->groupApplicationsButton->setVisible(owner);
    ui->dismissGroupButton->setVisible(owner);
    ui->leaveGroupButton->setVisible(active && !owner);
    ui->groupApplicationsButton->setEnabled(!m_groups->busy(groupchat::Applications,id));
    ui->renameGroupButton->setEnabled(!m_groups->busy(groupchat::Rename,id));
    ui->leaveGroupButton->setEnabled(active && !owner && !m_groups->busy(groupchat::Leave,id)); ui->dismissGroupButton->setEnabled(owner && !m_groups->busy(groupchat::Dismiss,id));
    if(!ui->groupNameEdit->hasFocus()) ui->groupNameEdit->setText(QString::fromStdString(g.name()));
    auto* member=ui->groupMembersList->currentItem();
    ui->kickMemberButton->setVisible(owner); ui->kickMemberButton->setEnabled(owner && member && member->data(Qt::UserRole).toULongLong()!=g.owner() && !m_groups->busy(groupchat::Kick,id));
    const bool reviewing=m_groups->busy(groupchat::Review,id);
    ui->acceptGroupApplicationButton->setEnabled(owner && ui->groupApplicationsList->currentItem() && !reviewing);
    ui->rejectGroupApplicationButton->setEnabled(ui->acceptGroupApplicationButton->isEnabled());
}
void GroupPanel::showMembers(const groupchat::Reply& reply) {
    ui->groupMembersList->clear();
    for(const auto& member:reply.members()) {
        auto* item=new QListWidgetItem(QStringLiteral("%1 (%2)%3").arg(QString::fromStdString(member.nickname())).arg(member.account()).arg(member.owner()?QStringLiteral(" · 群主"):QString()),ui->groupMembersList);
        item->setData(Qt::UserRole,QVariant::fromValue<qulonglong>(member.account()));
    }
    ui->groupDetailsStack->setCurrentWidget(ui->groupMembersPage); refresh();
}
void GroupPanel::showApplications(const groupchat::Reply& reply) {
    ui->groupApplicationsList->clear();
    for(const auto& a:reply.applications()) {
        auto* item=new QListWidgetItem(QStringLiteral("%1 (%2)\n%3").arg(QString::fromStdString(a.nickname())).arg(a.account()).arg(QString::fromStdString(a.reason())),ui->groupApplicationsList);
        item->setData(Qt::UserRole,QVariant::fromValue<qulonglong>(a.id()));
    }
    ui->groupApplicationsEmptyLabel->setVisible(!reply.applications_size());
    ui->groupDetailsStack->setCurrentWidget(ui->groupApplicationsPage); refresh();
}
