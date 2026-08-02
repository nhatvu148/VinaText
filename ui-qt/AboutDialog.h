/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// The About box, and the LGPLv3 attribution the port is obliged to carry.
//
// This is not merely the Qt counterpart of CAppAboutDlg (src/AppAboutDlg.cpp,
// 30 lines of scaffolding around IDD_ABOUTBOX). It is where two of D3's
// compliance items are discharged:
//
//   - "Add 'Uses Qt under LGPLv3' to AppAboutDlg, and state the exact Qt version"
//   - "Offer a link to that Qt version's corresponding source"
//
// Both are conditions of shipping a dynamically-linked LGPL binary, and under
// D10 this port ships to macOS and Linux. So this dialog is a release gate, not
// a nicety - which is why it is the first thing built after D10 was recorded.
//
// The strings it needs are exposed separately from the widget so the self-test
// can assert on them without opening a modal dialog nothing would dismiss.

#pragma once

#include <QDialog>
#include <QString>

namespace About
{
	// The Qt version actually LINKED AT RUN TIME, which is the one LGPL cares
	// about: the point of dynamic linking is that the user may swap it, so the
	// version compiled against is not necessarily the version they are running.
	// Both are reported when they differ.
	QString RuntimeQtVersion();
	QString CompiledQtVersion();

	// Where that exact Qt version's corresponding source can be obtained,
	// derived from the version rather than pinned, so it cannot go stale the way
	// IDD_ABOUTBOX's hard-coded "Release Version: 1.16" did.
	QString QtSourceUrl();

	// The whole attribution block, as plain text. The self-test asserts against
	// this rather than scraping a widget.
	QString AttributionText();
}

class CAboutDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit CAboutDialog(QWidget* pParent = nullptr);
};
