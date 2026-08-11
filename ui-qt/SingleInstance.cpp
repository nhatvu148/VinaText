/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QTimer>

namespace
{
	// How long to wait for the other end. Short, because this runs before the
	// window appears and a hung peer must not hold up the launch - if it does
	// not answer promptly it is treated as absent and this process becomes the
	// instance instead.
	const int CONNECT_TIMEOUT_MS = 500;
	const int WRITE_TIMEOUT_MS = 1000;
}

QString CSingleInstance::SocketName()
{
	// The user's home path, hashed. Two people on one Linux box must not share
	// a socket - the first would open the second's files - and the raw path is
	// not a legal socket name everywhere.
	const QByteArray digest = QCryptographicHash::hash(
		QDir::homePath().toUtf8(), QCryptographicHash::Sha1).toHex().left(16);
	return QStringLiteral("vinatext-") + QString::fromLatin1(digest);
}

QString CSingleInstance::LockPath(const QString& strName)
{
	return QDir::tempPath() + QLatin1Char('/')
		+ (strName.isEmpty() ? SocketName() : strName) + QStringLiteral(".lock");
}

bool CSingleInstance::HandOff(const QStringList& files, const QString& strName)
{
	QLocalSocket socket;
	socket.connectToServer(strName.isEmpty() ? SocketName() : strName);
	if (!socket.waitForConnected(CONNECT_TIMEOUT_MS))
	{
		// Nobody listening. Either this is the first launch, or a previous one
		// crashed and left the socket file behind - Listen() handles both by
		// removing a stale name before claiming it.
		return false;
	}

	// Absolute paths: the running instance has its own working directory, and
	// a relative path would resolve against the wrong one.
	QStringList absolute;
	for (const QString& strFile : files)
	{
		absolute.append(QFileInfo(strFile).absoluteFilePath());
	}

	QByteArray payload;
	QDataStream stream(&payload, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_6_0);
	stream << absolute;

	socket.write(payload);
	if (!socket.waitForBytesWritten(WRITE_TIMEOUT_MS))
	{
		// It answered the connection and then did not take the bytes. Better to
		// start a second window than to drop the file on the floor.
		return false;
	}
	socket.disconnectFromServer();
	return true;
}

bool CSingleInstance::TakeOverOrHandOff(const QStringList& files, bool& bBecameServer)
{
	bBecameServer = false;

	// THE WHOLE DECISION UNDER ONE LOCK. Connect-then-listen is only safe if
	// no other process can interleave between the two - see the header for
	// what happens when six launches race. QLockFile is advisory but every
	// participant here is this same binary, and it survives a crashed holder
	// by recording the owning pid.
	QLockFile lock(LockPath(m_strName));
	lock.setStaleLockTime(0);				// judge staleness by the pid, not a timeout
	if (!lock.tryLock(2000))
	{
		// Someone is holding it far too long. Falling back to an unlocked
		// attempt keeps the editor usable - a second window is a much smaller
		// failure than refusing to start.
		qWarning("single instance: could not take the lock; starting unserialised");
		if (HandOff(files, m_strName)) { return true; }
		bBecameServer = Listen();
		return false;
	}

	if (HandOff(files, m_strName))
	{
		return true;					// someone already owns it; nothing to do
	}
	bBecameServer = Listen();
	return false;
}

CSingleInstance::CSingleInstance(QObject* pParent, const QString& strName)
	: QObject(pParent)
	, m_strName(strName.isEmpty() ? SocketName() : strName)
{
}

bool CSingleInstance::Listen()
{
	m_pServer = new QLocalServer(this);

	// THE STALE SOCKET. On Unix the socket file outlives a crashed process, so
	// listen() would fail with AddressInUseError from then on and every launch
	// would open a new window. A name still present is debris - PROVIDED
	// nobody is actually using it, which is what the lock below establishes.
	QLocalServer::removeServer(m_strName);
	if (!m_pServer->listen(m_strName))
	{
		return false;
	}

	connect(m_pServer, &QLocalServer::newConnection, this, [this]
	{
		QLocalSocket* pSocket = m_pServer->nextPendingConnection();
		if (pSocket == nullptr)
		{
			return;
		}
		connect(pSocket, &QLocalSocket::disconnected, pSocket, &QLocalSocket::deleteLater);

		// SIGNAL-DRIVEN, NOT waitForReadyRead. This runs on the GUI thread, so
		// a blocking wait let any local process freeze the editor for a full
		// second just by connecting and saying nothing. Found in review.
		//
		// The timer is the other half: a peer that connects and never writes
		// would otherwise leave the socket open forever. It fires once, treats
		// silence as "just come to the front", and goes away.
		QTimer* pDeadline = new QTimer(pSocket);
		pDeadline->setSingleShot(true);
		connect(pDeadline, &QTimer::timeout, this, [this, pSocket]
		{
			// A launch that connected and said nothing still means "come to
			// the front", so the signal is emitted with no files.
			emit FilesReceived(QStringList());
			pSocket->abort();
		});
		connect(pSocket, &QLocalSocket::readyRead, this, [this, pSocket, pDeadline]
		{
			pDeadline->stop();
			QStringList files;
			QDataStream stream(pSocket);
			stream.setVersion(QDataStream::Qt_6_0);
			stream >> files;
			// A short payload can arrive in pieces. Waiting for the next
			// readyRead is correct; acting on a half-read list would open
			// nothing and look like the handoff was lost.
			if (stream.status() != QDataStream::Ok)
			{
				pDeadline->start(WRITE_TIMEOUT_MS);
				return;
			}
			emit FilesReceived(files);
		});
		pDeadline->start(WRITE_TIMEOUT_MS);
	});
	return true;
}
