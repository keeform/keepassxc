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

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QStandardPaths>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static QString runtimeDir(const QString& socketName)
{
#if defined(Q_OS_MACOS) || defined(Q_OS_DARWIN)
    // macOS has no XDG_RUNTIME_DIR equivalent. Use a UID-scoped subdirectory
    // of the system temp directory, matching what the Go host uses (os.TempDir()).
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + socketName + "-"
           + QString::number(getuid());
#else
    // Linux: use XDG_RUNTIME_DIR via Qt's RuntimeLocation abstraction.
    // Fall back to a UID-scoped temp subdirectory if not set.
    QString dir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + socketName + "-"
              + QString::number(getuid());
    }
    return dir;
#endif
}

static QString socketPath(const QString& socketName)
{
    // Socket lives in the per-user runtime directory, in a UID-scoped
    // subdirectory on macOS to match what the Go host computes via os.TempDir().
    return runtimeDir(socketName) + "/" + socketName + ".sock";
}

static QString lockPath(const QString& socketName)
{
    return runtimeDir(socketName) + "/" + socketName + ".lock";
}

static bool socketFileIsSafe(const QString& path)
{
    struct stat st;

    // The Unix payload contains a plaintext password, so verify the socket
    // pathname before connecting to it.
    if (stat(path.toUtf8().constData(), &st) != 0) {
        return false;
    }

    // Do not send credentials to an unexpected filesystem object.
    if (!S_ISSOCK(st.st_mode)) {
        return false;
    }

    // Block the common cross-user spoofing case: another local user creating
    // a socket at the expected path.
    if (st.st_uid != getuid()) {
        return false;
    }

    // Owner permission bits may vary by platform, but group/other users must
    // have no access to the socket because the password is plaintext on Unix.
    if ((st.st_mode & 0077) != 0) {
        return false;
    }

    return true;
}

static bool peerCredentialsAreSafe(const QLocalSocket& socket)
{
#ifdef SO_PEERCRED
    // Linux exposes Unix-domain socket peer credentials through SO_PEERCRED.
    // This verifies the process we actually connected to, closing the race
    // between checking the pathname and connecting to it.
    int fd = static_cast<int>(socket.socketDescriptor());
    if (fd < 0) {
        return false;
    }

    struct ucred cred;
    socklen_t len = sizeof(cred);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) {
        return false;
    }

    return cred.uid == getuid();
#elif defined(Q_OS_MACOS) || defined(Q_OS_DARWIN) || defined(Q_OS_BSD4)
    // macOS and BSD expose equivalent peer credentials through getpeereid().
    // As on Linux, accept only a host process running as the current user.
    int fd = static_cast<int>(socket.socketDescriptor());
    if (fd < 0) {
        return false;
    }

    uid_t uid;
    gid_t gid;
    if (getpeereid(fd, &uid, &gid) != 0) {
        return false;
    }

    return uid == getuid();
#else
    // Some Unix-like platforms do not expose portable peer credentials.
    // On those systems, fall back to the pathname ownership/mode checks above.
    Q_UNUSED(socket);
    return true;
#endif
}

static bool lockIsHeld(const QString& socketName)
{
    const QString path = lockPath(socketName);
    int fd = open(path.toUtf8().constData(), O_RDONLY);
    if (fd == -1) {
        return false;
    }
    bool locked = flock(fd, LOCK_EX | LOCK_NB) != 0;
    if (!locked) {
        flock(fd, LOCK_UN);
    }
    close(fd);
    return locked;
}

bool KeePushConnect::isHostRunning(const QString& appName, const QString& socketName)
{
    return checkStatus(appName, socketName) == Status::Detected;
}

KeePushConnect::Status KeePushConnect::checkStatus(const QString& appName, const QString& socketName)
{
    Q_UNUSED(appName)
    if (!lockIsHeld(socketName)) {
        return Status::NotRunning;
    }
    if (!socketFileIsSafe(socketPath(socketName))) {
        return Status::SocketUnsafe;
    }
    return Status::Detected;
}

KeePushConnect::Result
KeePushConnect::protectAndSend(const QString& appName, const QString& socketName, QJsonObject payload, int timeoutMs)
{
    Q_UNUSED(appName)
    // The host reads one compact JSON object per line.
    QByteArray data = QJsonDocument(payload).toJson(QJsonDocument::Compact) + "\n";

    const QString path = socketPath(socketName);

    // Refuse to send plaintext credentials unless the socket path is a real
    // socket owned by this user and not accessible by group or other users.
    if (!socketFileIsSafe(path)) {
        return Result::HostNotRunning;
    }

    QLocalSocket socket;
    socket.connectToServer(path);
    if (!socket.waitForConnected(timeoutMs)) {
        return Result::Timeout;
    }

    // Verify the connected peer where the platform supports it. This prevents
    // pathname replacement between socketFileIsSafe() and connectToServer().
    if (!peerCredentialsAreSafe(socket)) {
        socket.disconnectFromServer();
        return Result::HostNotRunning;
    }

    // Treat short writes and timeout/failure to flush the Qt socket buffer as
    // failure. The payload contains credentials and must be delivered as a
    // complete JSON line or not at all.
    if (socket.write(data) != data.size() || !socket.waitForBytesWritten(timeoutMs)) {
        socket.disconnectFromServer();
        return Result::HostNotRunning;
    }
    socket.disconnectFromServer();
    return Result::Success;
}
