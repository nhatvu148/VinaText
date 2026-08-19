/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "MacAppearance.h"

#import <AppKit/AppKit.h>

void MacAppearance::Apply(bool bDark)
{
	// NSAppearanceNameDarkAqua is 10.14+. Older systems have no dark mode to
	// disagree with, so leaving the appearance nil there is not a gap - it is
	// the only state those systems have.
	if (@available(macOS 10.14, *))
	{
		NSAppearance* appearance = [NSAppearance appearanceNamed:
			(bDark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua)];
		[NSApplication sharedApplication].appearance = appearance;
	}
}
