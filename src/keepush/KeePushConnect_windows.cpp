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

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>

#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>

// KeePush data file entropy — matches KeeForm host for v1 compatibility.
// Must match the value in the host application.
static const BYTE keepushEntropy[] = {0x4B, 0x65, 0x65, 0x46, 0x30, 0x72, 0x6D, 0xEE, 0xF0};

// KeePass password entropy — used for passwordEnc field, matches KeeForm/KeePass convention.
static const BYTE keepassEntropy[] = {0xA5, 0x74, 0x2E, 0xEC};

static QString dataFilePath(const QString& appName, const QString& socketName)
{
    // The KeePush host writes its connection metadata under the current user's
    // LocalAppData profile directory, in a folder named after the application.
    // File is named <socketName>_host.exe.data to match the host convention.
    // The file contents are DPAPI-protected before this process reads them.
    QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (localAppData.isEmpty()) {
        return {};
    }
    return localAppData + "/" + appName + "/" + socketName + "_host.exe.data";
}

static QByteArray dpApiDecrypt(const QByteArray& encrypted, const BYTE* entropy, DWORD entropyLen)
{
    DATA_BLOB input, entropyBlob, output;
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.data()));
    input.cbData = static_cast<DWORD>(encrypted.size());
    entropyBlob.pbData = const_cast<BYTE*>(entropy);
    entropyBlob.cbData = entropyLen;

    // DPAPI binds the data to the current Windows user profile. The additional
    // entropy must match the host; otherwise decryption fails.
    if (!CryptUnprotectData(&input, nullptr, &entropyBlob, nullptr, nullptr, 0, &output)) {
        return {};
    }

    QByteArray result(reinterpret_cast<char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return result;
}

static QByteArray dpApiEncrypt(const QByteArray& plaintext, const BYTE* entropy, DWORD entropyLen)
{
    DATA_BLOB input, entropyBlob, output;
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data()));
    input.cbData = static_cast<DWORD>(plaintext.size());
    entropyBlob.pbData = const_cast<BYTE*>(entropy);
    entropyBlob.cbData = entropyLen;

    // Returning an empty array makes encryption failure explicit to the caller.
    if (!CryptProtectData(&input, nullptr, &entropyBlob, nullptr, nullptr, 0, &output)) {
        return {};
    }

    QByteArray result(reinterpret_cast<char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return result;
}

bool KeePushConnect::isHostRunning(const QString& appName, const QString& socketName)
{
    return checkStatus(appName, socketName) == Status::Detected;
}

KeePushConnect::Status KeePushConnect::checkStatus(const QString& appName, const QString& socketName)
{
    // The KeePush host owns a named mutex while running.
    // Mutex name follows the convention: <AppName>HostMutex
    const QString mutexName = appName + "HostMutex";
    HANDLE h = OpenMutexW(SYNCHRONIZE, FALSE, mutexName.toStdWString().c_str());
    if (h == nullptr) {
        return Status::NotRunning;
    }
    CloseHandle(h);

    // Also verify the data file exists and is readable — if not, something
    // is wrong even though the mutex exists.
    if (!QFile::exists(dataFilePath(appName, socketName))) {
        return Status::SocketUnsafe;
    }

    return Status::Detected;
}

KeePushConnect::Result
KeePushConnect::protectAndSend(const QString& appName, const QString& socketName, QJsonObject payload, int timeoutMs)
{
    // The host stores its named-pipe endpoint in a DPAPI-protected data file.
    // If the file is missing or unreadable, treat the host as unavailable.
    QFile file(dataFilePath(appName, socketName));
    if (!file.open(QIODevice::ReadOnly)) {
        return Result::HostNotRunning;
    }
    QByteArray encrypted = file.readAll();
    file.close();

    QByteArray decrypted = dpApiDecrypt(encrypted, keepushEntropy, sizeof(keepushEntropy));
    if (decrypted.isEmpty()) {
        return Result::HostNotRunning;
    }

    // The decrypted file format is "pipeName\n...". Only the pipe name is needed here.
    QString pipeName = QString::fromUtf8(decrypted).section('\n', 0, 0).trimmed();
    if (pipeName.isEmpty()) {
        return Result::HostNotRunning;
    }

    // On Windows, encrypt the password field and fields object with DPAPI.
    const QString plainPassword = payload["passwordEnc"].toString();
    QByteArray encryptedPassword = dpApiEncrypt(plainPassword.toUtf8(), keepassEntropy, sizeof(keepassEntropy));
    if (encryptedPassword.isEmpty()) {
        return Result::HostNotRunning;
    }
    payload["passwordEnc"] = QString::fromLatin1(encryptedPassword.toBase64());

    if (payload.contains("fields")) {
        QByteArray fieldsJson = QJsonDocument(payload["fields"].toObject()).toJson(QJsonDocument::Compact);
        QByteArray encryptedFields = dpApiEncrypt(fieldsJson, keepushEntropy, sizeof(keepushEntropy));
        if (encryptedFields.isEmpty()) {
            return Result::HostNotRunning;
        }
        payload.remove("fields");
        payload["fieldsEnc"] = QString::fromLatin1(encryptedFields.toBase64());
    }

    // The host reads one compact JSON object per line.
    QByteArray data = QJsonDocument(payload).toJson(QJsonDocument::Compact) + "\n";

    // QLocalSocket maps to a Windows named pipe for this platform build.
    // No additional ACL check is needed here: the pipe name was obtained from
    // a DPAPI-protected file (bound to this Windows user session), making it
    // unguessable by other processes. The password is also DPAPI-encrypted
    // before entering the pipe payload, so interception by another process
    // would still not expose plaintext.
    QLocalSocket socket;
    socket.connectToServer(pipeName);
    if (!socket.waitForConnected(timeoutMs)) {
        return Result::Timeout;
    }

    // Treat short writes and timeout/failure to flush the Qt socket buffer as
    // failure. The host expects a complete JSON line.
    if (socket.write(data) != data.size() || !socket.waitForBytesWritten(timeoutMs)) {
        socket.disconnectFromServer();
        return Result::HostNotRunning;
    }
    socket.disconnectFromServer();
    return Result::Success;
}
