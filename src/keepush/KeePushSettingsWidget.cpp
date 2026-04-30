/*
 *  Copyright (C) 2026   dave_keepass at users.sourceforge.net
 *                       https://keeform.org
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "KeePushSettingsWidget.h"
#include "ui_KeePushSettingsWidget.h"

#include "KeePushConnect.h"
#include "core/Config.h"
#include <QCheckBox>
#include <QLineEdit>

KeePushSettingsWidget::KeePushSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::KeePushSettingsWidget())
{
    m_ui->setupUi(this);
    m_ui->infoLabel->setOpenExternalLinks(true);
    m_ui->infoLabel->setText(
        tr("KeePush sends credentials from KeePassXC to an external application via a local socket. "
           "The receiving application must implement the KeePush protocol."));

    connect(m_ui->enableKeePush, &QCheckBox::toggled, this, [this](bool checked) {
        m_ui->optionsGroup->setEnabled(checked);
    });
    connect(m_ui->appName, &QLineEdit::textChanged, this, &KeePushSettingsWidget::updateStatus);
}

KeePushSettingsWidget::~KeePushSettingsWidget() = default;

void KeePushSettingsWidget::loadSettings()
{
    m_ui->enableKeePush->setChecked(config()->get(Config::KeePush_Enabled).toBool());
    m_ui->appName->setText(config()->get(Config::KeePush_AppName).toString());
    m_ui->doubleClickOpens->setChecked(config()->get(Config::KeePush_DoubleClickOpens).toBool());

    int protocol = config()->get(Config::KeePush_Protocol).toInt();
    m_ui->protocolV2->setChecked(protocol == 2);
    m_ui->protocolV1->setChecked(protocol != 2);

    m_ui->timeout->setValue(config()->get(Config::KeePush_Timeout).toInt());
    m_ui->optionsGroup->setEnabled(m_ui->enableKeePush->isChecked());
    updateStatus();
}

void KeePushSettingsWidget::saveSettings()
{
    const QString slug = KeePushConnect::toSocketName(m_ui->appName->text());
    if (slug.isEmpty()) {
        m_ui->statusLabel->setText(tr("Host name is invalid — please enter a valid name."));
        m_ui->statusLabel->setStyleSheet("color: red;");
        return;
    }

    config()->set(Config::KeePush_Enabled, m_ui->enableKeePush->isChecked());
    config()->set(Config::KeePush_AppName, m_ui->appName->text());

    // Normalize the socket name to a safe slug before saving — it is used
    // in file paths, mutex names, and socket names.
    config()->set(Config::KeePush_SocketName, slug);

    config()->set(Config::KeePush_DoubleClickOpens, m_ui->doubleClickOpens->isChecked());
    config()->set(Config::KeePush_Protocol, m_ui->protocolV2->isChecked() ? 2 : 1);
    config()->set(Config::KeePush_Timeout, m_ui->timeout->value());
}

void KeePushSettingsWidget::updateStatus()
{
    const QString socketName = KeePushConnect::toSocketName(m_ui->appName->text());
    const QString appName = m_ui->appName->text();

    if (socketName.isEmpty()) {
        m_ui->statusLabel->setText(tr("No socket name configured"));
        m_ui->statusLabel->setStyleSheet("color: gray;");
        return;
    }

    const auto status = KeePushConnect::checkStatus(appName, socketName);
    switch (status) {
    case KeePushConnect::Status::Detected:
        m_ui->statusLabel->setText(tr("%1: host ready ✓").arg(appName));
        m_ui->statusLabel->setStyleSheet("color: green;");
        break;
    case KeePushConnect::Status::SocketUnsafe:
        m_ui->statusLabel->setText(tr("%1: connection refused for security reasons ⚠").arg(appName));
        m_ui->statusLabel->setStyleSheet("color: red;");
        break;
    case KeePushConnect::Status::NotRunning:
    default:
        m_ui->statusLabel->setText(tr("%1: not running").arg(appName));
        m_ui->statusLabel->setStyleSheet("color: gray;");
        break;
    }
}
