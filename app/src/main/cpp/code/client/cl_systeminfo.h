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
#pragma once

#include "str.h"

enum cpuType_t {
    CPU_CELERON = 0,
    CPU_PENTIUM2,
    CPU_PENTIUM3,
    CPU_PENTIUM4,
    CPU_K5,
    CPU_K6,
    CPU_K7,
    CPU_DURON,
    CPU_ATHLON,
    CPU_THUNDERBIRD,
    CPU_POWERPC,
    CPU_UNKNOWN,

    CPU_MAXTYPE
};

class SystemInfo
{
public:
    SystemInfo();
    virtual ~SystemInfo();

    void detectSystemInformation(void);
    void detectCPUFrequency(void);
    void detectCPUType(void);
    void detectPhysicalRam(void);
    void detectVideoRam(void);
    void getCPUTypeInText(str& text);

    unsigned long long frequency(void) const { return m_frequency; }
    int                cpuType(void) const { return m_cpuType; }
    int                physicalRamMB(void) const { return m_physicalRamMB; }
    int                videoRamMB(void) const { return m_videoRamMB; }

protected:
    unsigned long long m_frequency;
    int                m_cpuType;
    int                m_physicalRamMB;
    int                m_videoRamMB;
};

extern SystemInfo systemInfo;

void CL_SetupSystem(void);
void CL_SetupSystem_f(void);
void CL_DetectSystemInfo(void);
