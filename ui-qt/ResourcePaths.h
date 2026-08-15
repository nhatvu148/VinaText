/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Where the app finds the files it does not compile in: the extracted JSON in
// Packages/data-packages, and the licence texts in license/.
//
// THIS IS THE PREREQUISITE FOR PACKAGING. Both paths were compiled in as
// ABSOLUTE paths into the source tree - VINATEXT_DATA_DIR and
// VINATEXT_LICENSE_DIR - which works only on the machine that built the binary.
// Copy the executable anywhere else and it looks for languages.json in a
// directory that does not exist there. No .app, .dmg or AppImage can work until
// this resolves relative to the executable.
//
// The licence half is not a nicety either. The About box tells the user the Qt
// licence text is at "license/License-Qt.txt", and D3's LGPL obligation is that
// the text actually ships. A path that was true only in the build tree makes
// that statement false in every distributed copy.
//
// The compiled-in paths STAY, last in the search order: a developer running
// ./qtbuild/ui-qt/vinatext-qt has no bundle and nothing next to the binary, and
// that has to keep working.

#pragma once

#include <QString>

namespace ResourcePaths
{
	// languages.json, theme-*.json and the .dat files. --data overrides it.
	QString DataDir();

	// License-*.txt.
	QString LicenseDir();

	// Every candidate for a leaf, in search order, whether or not it exists.
	// Public so the self-test can report WHICH ones were tried when none hit -
	// "data directory not found" without the list is a bad afternoon.
	QStringList Candidates(const QString& strLeaf);
}
