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

#include "KeePushConnect.h"

#include "core/Config.h"
#include "core/Entry.h"
#include "core/EntryAttributes.h"
#include "core/Totp.h"

#include <QRegularExpression>
#include <algorithm>

// Converts an application name to a safe socket/connection name slug:
// lowercase, spaces and special characters replaced with hyphens,
// leading/trailing hyphens removed.
QString KeePushConnect::toSocketName(const QString& appName)
{
    QString slug = appName.toLower();
    slug.replace(QRegularExpression("[^a-z0-9]+"), "-");
    slug.remove(QRegularExpression("^-+|-+$"));
    return slug;
}

QJsonObject KeePushConnect::buildV1Payload(const QString& url, const QString& username, const QString& password)
{
    QJsonObject payload;
    payload["version"] = 1;
    payload["url"] = url;
    payload["username"] = username;
    payload["passwordEnc"] = password;
    return payload;
}

QJsonObject KeePushConnect::buildV2Fields(const Entry* entry)
{
    QJsonObject fields;
    fields["Title"] = entry->resolveMultiplePlaceholders(entry->title());
    fields["Notes"] = entry->resolveMultiplePlaceholders(entry->notes());

    // Include current TOTP code if the entry has TOTP configured.
    if (entry->hasTotp()) {
        fields["TOTP"] = entry->totp();
    }

    // Include all custom string attributes, including user-marked protected fields.
    // TOTP seed and attachments are never included — TOTP code is sent separately above.
    const EntryAttributes* attrs = entry->attributes();
    for (const QString& key : attrs->customKeys()) {
        if (key == Totp::ATTRIBUTE_OTP) {
            continue; // exclude TOTP seed — only the generated code is sent
        }
        fields[key] = attrs->value(key);
    }
    return fields;
}

QJsonObject
KeePushConnect::buildV2Payload(const QString& url, const QString& username, const QString& password, const Entry* entry)
{
    QJsonObject payload;
    payload["version"] = 2;
    payload["url"] = url;
    payload["username"] = username;
    payload["passwordEnc"] = password;
    payload["fields"] = buildV2Fields(entry);
    return payload;
}

KeePushConnect::Result KeePushConnect::sendCredentials(const QString& appName,
                                                       const QString& socketName,
                                                       const QString& url,
                                                       const QString& username,
                                                       const QString& password)
{
    QJsonObject payload = buildV1Payload(url, username, password);
    const int timeoutMs = std::max(1, config()->get(Config::KeePush_Timeout).toInt()) * 1000;
    return protectAndSend(appName, socketName, payload, timeoutMs);
}

KeePushConnect::Result KeePushConnect::sendEntry(const QString& appName, const QString& socketName, const Entry* entry)
{
    Q_ASSERT(entry);
    if (!entry) {
        return Result::HostNotRunning;
    }

    const QString url = entry->resolveMultiplePlaceholders(entry->url());
    const QString username = entry->resolveMultiplePlaceholders(entry->username());
    const QString password = entry->resolveMultiplePlaceholders(entry->password());

    QJsonObject payload = buildV2Payload(url, username, password, entry);
    const int timeoutMs = std::max(1, config()->get(Config::KeePush_Timeout).toInt()) * 1000;
    return protectAndSend(appName, socketName, payload, timeoutMs);
}
