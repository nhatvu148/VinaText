/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "MacAppearance.h"

// Everywhere that is not macOS. The title bar there is drawn by the window
// manager or by Qt itself and follows the palette or the desktop's own theme;
// there is no equivalent to NSApplication.appearance to set.
void MacAppearance::Apply(bool)
{
}
