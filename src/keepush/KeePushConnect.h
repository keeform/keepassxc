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
#ifndef KEEPUSHCONNECT_H
#define KEEPUSHCONNECT_H

#include <QJsonObject>
#include <QString>

class Entry;

class KeePushConnect
{
public:
    enum class Result
    {
        Success,
        HostNotRunning,
        Timeout
    };

    enum class Status
    {
        NotRunning,
        SocketUnsafe,
        Detected
    };

    // Converts an application name to a safe socket name slug.
    static QString toSocketName(const QString& appName);

    // Returns true if the KeePush host is running and ready.
    static bool isHostRunning(const QString& appName, const QString& socketName);

    // Returns detailed host status for UI display.
    static Status checkStatus(const QString& appName, const QString& socketName);

    // Sends credentials (protocol v1) to the KeePush host.
    static Result sendCredentials(const QString& appName,
                                  const QString& socketName,
                                  const QString& url,
                                  const QString& username,
                                  const QString& password);

    // Sends full entry fields (protocol v2) to the KeePush host.
    static Result sendEntry(const QString& appName, const QString& socketName, const Entry* entry);

private:
    // Shared protocol construction
    static QJsonObject buildV1Payload(const QString& url, const QString& username, const QString& password);
    static QJsonObject buildV2Fields(const Entry* entry);
    static QJsonObject
    buildV2Payload(const QString& url, const QString& username, const QString& password, const Entry* entry);

    // Platform-specific: protect payload (encrypt on Windows) and send.
    static Result protectAndSend(const QString& appName, const QString& socketName, QJsonObject payload, int timeoutMs);
};

#endif // KEEPUSHCONNECT_H
