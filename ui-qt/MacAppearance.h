/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// The one piece of the window a QPalette cannot reach.
//
// On macOS the TITLE BAR is drawn by the system, not by Qt, and it follows the
// OS appearance. So a light theme on a Mac in Dark Mode left a dark title bar
// sitting on top of a fully light window - reported from the UI, twice, after
// the palette fix had already landed. There is no cross-platform Qt API for it;
// AppKit's NSApplication.appearance is the whole mechanism.
//
// Set on the APPLICATION rather than per window, so dialogs, popups and the
// menu bar move with it instead of each needing to be found and told.
//
// A no-op everywhere else, by design: X11 and Windows title bars follow their
// own conventions and neither has an equivalent knob. See doc/PORTING.md 6v.

#pragma once

namespace MacAppearance
{
	// bDark selects NSAppearanceNameDarkAqua, otherwise NSAppearanceNameAqua.
	// Does nothing off macOS.
	void Apply(bool bDark);
}
