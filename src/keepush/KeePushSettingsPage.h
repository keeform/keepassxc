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
#ifndef KEEPUSHSETTINGSPAGE_H
#define KEEPUSHSETTINGSPAGE_H

#include "gui/ApplicationSettingsWidget.h"

class KeePushSettingsPage : public ISettingsPage
{
public:
    explicit KeePushSettingsPage() = default;
    ~KeePushSettingsPage() override = default;
    QString name() override;
    QIcon icon() override;
    QWidget* createWidget() override;
    void loadSettings(QWidget* widget) override;
    void saveSettings(QWidget* widget) override;
};

#endif // KEEPUSHSETTINGSPAGE_H
