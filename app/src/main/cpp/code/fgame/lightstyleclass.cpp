/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/
// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction
#include "lightstyleclass.h"

#include "g_local.h"
#include "archive.h"


LightStyleClass lightStyles;

CLASS_DECLARATION(Class, LightStyleClass, NULL) {
    {NULL, NULL}
};

void LightStyleClass::SetLightStyle(int index, str style)
{
    if ((index < 0) || (index >= MAX_LIGHTSTYLES)) {
        assert(0);
        return;
    }

    styles[index] = style;
    gi.SetLightStyle(index, style.c_str());
}

void LightStyleClass::Archive(Archiver& arc)
{
    int i;

    for (i = 0; i < MAX_LIGHTSTYLES; i++) {
        arc.ArchiveString(&styles[i]);
        if (arc.Loading() && styles[i].length()) {
            gi.SetLightStyle(i, styles[i].c_str());
        }
    }
}