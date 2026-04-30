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
#include "KeePushSettingsPage.h"
#include "KeePushSettingsWidget.h"
#include "gui/Icons.h"

QString KeePushSettingsPage::name()
{
    return QObject::tr("KeePush Integration");
}

QIcon KeePushSettingsPage::icon()
{
    return icons()->icon("keepush");
}

QWidget* KeePushSettingsPage::createWidget()
{
    return new KeePushSettingsWidget();
}

void KeePushSettingsPage::loadSettings(QWidget* widget)
{
    auto* w = qobject_cast<KeePushSettingsWidget*>(widget);
    Q_ASSERT(w);
    if (w) {
        w->loadSettings();
    }
}

void KeePushSettingsPage::saveSettings(QWidget* widget)
{
    auto* w = qobject_cast<KeePushSettingsWidget*>(widget);
    Q_ASSERT(w);
    if (w) {
        w->saveSettings();
    }
}
