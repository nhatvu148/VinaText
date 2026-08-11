/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// One running VinaText, not one per double-clicked file.
//
// CSingleInstanceApp holds a named Win32 mutex, and hands the filename over as
// a GLOBAL ATOM broadcast in a registered window message. None of that exists
// off Windows; QLocalServer is the mapping the brief's Phase 5 table already
// names, and it carries a payload directly instead of through the atom table.
//
// THE ORDER IS CONNECT-THEN-LISTEN, and that is not arbitrary. Trying to
// connect first answers "is someone there?" and, when nobody is, leaves a stale
// socket file from a crashed run detectable rather than fatal - on Unix the
// file outlives the process and listen() would fail with AddressInUseError
// forever after a crash. See doc/PORTING.md 6q.
//
// The headless modes bypass this completely. A --selftest that handed its file
// list to a running editor and exited would pass CI by not running.

#pragma once

#include <QObject>
#include <QStringList>

class QLocalServer;

class CSingleInstance final : public QObject
{
	Q_OBJECT

public:
	// strName defaults to SocketName(). The self-test passes its OWN name and
	// that is not a nicety: a check calling HandOff() under the real name
	// would connect to the user's actual running editor and open the test's
	// files in it.
	explicit CSingleInstance(QObject* pParent = nullptr, const QString& strName = QString());

	// Asks an already-running instance to take these files. Returns true when
	// one accepted, meaning this process has nothing left to do and should
	// exit - which is what CVinaTextApp::InitInstance's `return FALSE` does.
	static bool HandOff(const QStringList& files, const QString& strName = QString());

	// Becomes the instance that others hand off to. False if the socket cannot
	// be claimed, which is not fatal: the editor still runs, it just will not
	// be found by a later launch.
	bool Listen();

	// The socket name, which includes the user so two people on one machine do
	// not collide. Public for the self-test.
	static QString SocketName();
	// The lock that serialises the decide-to-become-server step. Public for
	// the self-test.
	static QString LockPath(const QString& strName = QString());

	// Runs the whole connect-then-listen decision under an OS-level lock, and
	// is the ONLY correct way to use this class.
	//
	// Without it, launching several files at once - which a file manager does
	// by spawning one process per file - has them all fail HandOff (nobody is
	// listening YET), then all race into Listen(), where each one's
	// removeServer() unlinks the previous winner's live socket. MEASURED: six
	// simultaneous launches left FOUR windows. The review that found this
	// estimated two.
	//
	// Returns true when this process should exit because another instance took
	// the files.
	bool TakeOverOrHandOff(const QStringList& files, bool& bBecameServer);

signals:
	// Files another launch wants opened. Empty means "just come to the front",
	// which is what a bare second launch should do - the MFC notifies only when
	// there is a file, so double-clicking its icon while it runs does nothing
	// at all.
	void FilesReceived(const QStringList& files);

private:
	QLocalServer* m_pServer = nullptr;
	QString m_strName;
};
