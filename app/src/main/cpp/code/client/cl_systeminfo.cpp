
// quantity, and they are the only three lines in the file that are not retail.

#include "client.h"
#include "cl_systeminfo.h"
#include "qcommon.h"
#include "container.h"

#if defined( __linux__ )
#include <unistd.h>
#include <cstdio>
#endif

extern cvar_t *cl_systemsetup;

static const char *const s_cpuTypeNames[CPU_MAXTYPE + 1] = {
    "Celeron",     
    "Pentium 2",   
    "Pentium 3",   
    "Pentium 4",   
    "K5",          
    "K6",          
    "K7",          
    "Duron",       
    "Athlon",      
    "Thunderbird", 
    "PowerPC",     
    "Unknown",     
};

static const char *const s_cpuTypeKeywordValues[CPU_MAXTYPE - 1] = {
    "CELERON", "PENTIUM2", "PENTIUM3", "PENTIUM4", "K5", "K6", "K7",
    "DURON", "ATHLON", "THUNDERBIRD", "POWERPC",
};

#define CPU_CONFIG_FILE     "global/cpuconfig.sys"     
#define MEMORY_CONFIG_FILE  "global/memoryconfig.sys"  
#define CPU_READ_WARNING    "Unable to read cpu config file: %s"     
#define MEMORY_READ_WARNING "Unable to read memory config file: %s"  
#define CPU_KEYWORD     "config"    
#define CPU_KEYWORD_CPU "cpu"       
#define CPU_KEYWORD_DEF "default"   
#define MEM_KEYWORD_RAM "ram"       
#define EXEC_PREFIX     "exec "     
#define EXEC_SUFFIX     "\n"        

SystemInfo::SystemInfo()
    : m_frequency(0)
    , m_cpuType(CPU_UNKNOWN)
    , m_physicalRamMB(0)
    , m_videoRamMB(0)
{
}

SystemInfo::~SystemInfo() {}

void SystemInfo::detectCPUFrequency(void)
{
    m_frequency = 0;

#if defined( __linux__ )
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        return;
    }

    char  line[256];
    float mhz = 0.0f;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "cpu MHz\t\t: %f", &mhz) == 1) {
            break;
        }
    }
    fclose(fp);

    m_frequency = (unsigned long long)(mhz * 1000000.0f);
#endif
}

void SystemInfo::detectCPUType(void)
{
    m_cpuType = CPU_UNKNOWN;
}

void SystemInfo::detectPhysicalRam(void)
{
    m_physicalRamMB = 0;

#if defined( __linux__ )
    long pages = sysconf(_SC_PHYS_PAGES);
    long size  = sysconf(_SC_PAGESIZE);
    if (pages > 0 && size > 0) {
        m_physicalRamMB = (int)((unsigned long long)pages * (unsigned long long)size / 1024ull / 1024ull);
    }
#endif
}

void SystemInfo::detectVideoRam(void)
{
    m_videoRamMB = 0;
}

void SystemInfo::getCPUTypeInText(str& text)
{
    if (m_cpuType >= 0 && m_cpuType < CPU_MAXTYPE) {
        text = s_cpuTypeNames[m_cpuType];
    } else {
        text = s_cpuTypeNames[CPU_MAXTYPE];
    }
}

void SystemInfo::detectSystemInformation(void)
{
    detectCPUFrequency();
    detectCPUType();

    /* The startup identity line: the CPU and the device model, and nothing else. */
    Com_Printf("CPU: %s\n", Sys_GetCPUModel());
    if (Sys_GetDeviceModel()[0]) {
        Com_Printf("Device: %s\n", Sys_GetDeviceModel());
    }

    detectPhysicalRam();
    detectVideoRam();
}

SystemInfo systemInfo;

void CL_DetectSystemInfo(void)
{
    systemInfo.detectSystemInformation();
}

class CPUInfo
{
public:
    int   cpuType;
    float minFrequency;
};

class CPUSetupEntry
{
public:
    CPUSetupEntry();
    ~CPUSetupEntry();

    bool doesMatch(int cpuType, float frequency) const;

    str                        command;
    Container<CPUInfo *>       infos;
};

class MemorySetupEntry
{
public:
    MemorySetupEntry();
    ~MemorySetupEntry();

    bool doesMatch(int mb) const;

    str           command;
    Container<int> values;
};

CPUSetupEntry::CPUSetupEntry() {}

CPUSetupEntry::~CPUSetupEntry()
{
    for (int i = 1; i <= infos.NumObjects(); i++) {
        delete infos.ObjectAt(i);
    }

    infos.ClearObjectList();
}

bool CPUSetupEntry::doesMatch(int cpuType, float frequency) const
{
    for (int i = 1; i <= infos.NumObjects(); i++) {
        CPUInfo *info = infos.ObjectAt(i);
        if (info && cpuType == info->cpuType && info->minFrequency <= frequency) {
            return true;
        }
    }

    return false;
}

MemorySetupEntry::MemorySetupEntry() {}

MemorySetupEntry::~MemorySetupEntry()
{
    values.ClearObjectList();
}

bool MemorySetupEntry::doesMatch(int mb) const
{
    for (int i = 1; i <= values.NumObjects(); i++) {
        if (values.ObjectAt(i) <= mb) {
            return true;
        }
    }

    return false;
}

class SystemSetup
{
public:
    SystemSetup();
    ~SystemSetup();

    bool readCPUSetup(const str& filename);
    bool readMemorySetup(const str& filename);
    void configureOptions(void);
    void configureCPUOptions(void);
    void configureMemoryOptions(void);
    void configureVideoOptions(void);
    int  getCPUType(const str& name) const;

    Container<CPUSetupEntry *>    cpuEntries;       
    Container<MemorySetupEntry *> memoryEntries;    
    str                           defaultCommand;   
};

SystemSetup::SystemSetup() {}

SystemSetup::~SystemSetup()
{
    for (int i = 1; i <= cpuEntries.NumObjects(); i++) {
        delete cpuEntries.ObjectAt(i);
    }

    cpuEntries.ClearObjectList();

    for (int i = 1; i <= memoryEntries.NumObjects(); i++) {
        delete memoryEntries.ObjectAt(i);
    }

    memoryEntries.ClearObjectList();
}

int SystemSetup::getCPUType(const str& name) const
{
    str upper = name;

    upper.toupper();

    for (int i = 0; i < CPU_MAXTYPE - 1; i++) {
        if (!strcmp(upper.c_str(), s_cpuTypeKeywordValues[i])) {
            return i;
        }
    }

    return CPU_UNKNOWN;
}

bool SystemSetup::readCPUSetup(const str& filename)
{
    byte  *buffer = NULL;
    char  *text;
    char  *token;
    str    keyword;
    str    value;
    CPUSetupEntry *entry = NULL;

    FS_ReadFileEx(filename.c_str(), (void **)&buffer, qtrue);
    if (!buffer) {
        return false;
    }

    text = (char *)buffer;

    while (1) {
        if (!text) {
            FS_FreeFile(buffer);
            return true;
        }

        token = COM_ParseExt(&text, qtrue);
        if (!token || !token[0]) {
            FS_FreeFile(buffer);
            return true;
        }

        keyword = token;
        keyword.tolower();

        if (!strcmp(keyword.c_str(), CPU_KEYWORD)) {
            token = COM_ParseExt(&text, qtrue);
            if (!token || !token[0]) {
                FS_FreeFile(buffer);
                return false;
            }

            entry = new CPUSetupEntry;
            entry->command = token;
            cpuEntries.AddObject(entry);
            continue;
        }

        if (!strcmp(keyword.c_str(), CPU_KEYWORD_CPU)) {
            if (!entry) {
                FS_FreeFile(buffer);
                return false;
            }

            token = COM_ParseExt(&text, qtrue);
            if (!token || !token[0]) {
                FS_FreeFile(buffer);
                return false;
            }

            value = token;
            int cpuType = getCPUType(value);

            token = COM_ParseExt(&text, qtrue);
            if (!token || !token[0]) {
                FS_FreeFile(buffer);
                return false;
            }

            CPUInfo *info = new CPUInfo;
            info->cpuType      = cpuType;
            info->minFrequency = (float)atof(token);
            entry->infos.AddObject(info);
            continue;
        }

        if (!strcmp(keyword.c_str(), CPU_KEYWORD_DEF)) {
            token = COM_ParseExt(&text, qtrue);
            if (!token || !token[0]) {
                break;
            }

            defaultCommand = token;
        }
    }

    FS_FreeFile(buffer);
    return false;
}

bool SystemSetup::readMemorySetup(const str& filename)
{
    byte  *buffer = NULL;
    char  *text;
    char  *token;
    str    keyword;
    MemorySetupEntry *entry = NULL;

    FS_ReadFileEx(filename.c_str(), (void **)&buffer, qtrue);
    if (!buffer) {
        return false;
    }

    text = (char *)buffer;

    while (1) {
        if (!text) {
            FS_FreeFile(buffer);
            return true;
        }

        token = COM_ParseExt(&text, qtrue);
        if (!token || !token[0]) {
            FS_FreeFile(buffer);
            return true;
        }

        keyword = token;
        keyword.tolower();

        if (!strcmp(keyword.c_str(), CPU_KEYWORD)) {
            token = COM_ParseExt(&text, qtrue);
            if (!token || !token[0]) {
                FS_FreeFile(buffer);
                return false;
            }

            entry = new MemorySetupEntry;
            entry->command = token;
            memoryEntries.AddObject(entry);
            continue;
        }

        if (!strcmp(keyword.c_str(), MEM_KEYWORD_RAM)) {
            if (!entry) {
                FS_FreeFile(buffer);
                return false;
            }

            token = COM_ParseExt(&text, qtrue);
            if (!token || !token[0]) {
                FS_FreeFile(buffer);
                return false;
            }

            entry->values.AddObject(atoi(token));
            continue;
        }

        if (!strcmp(keyword.c_str(), CPU_KEYWORD_DEF)) {
            token = COM_ParseExt(&text, qtrue);
            if (!token || !token[0]) {
                break;
            }

            defaultCommand = token;
        }
    }

    FS_FreeFile(buffer);
    return false;
}

void SystemSetup::configureCPUOptions(void)
{
    CPUSetupEntry *match = NULL;

    for (int i = 1; i <= cpuEntries.NumObjects(); i++) {
        CPUSetupEntry *entry = cpuEntries.ObjectAt(i);
        if (!entry) {
            continue;
        }

        if (entry->doesMatch(systemInfo.cpuType(), (float)systemInfo.frequency())) {
            match = entry;
            break;
        }
    }

    if (match) {
        Cbuf_AddText(va("%s%s%s", EXEC_PREFIX, match->command.c_str(), EXEC_SUFFIX));
    } else if (cpuEntries.NumObjects() > 0) {
        // SANCTIONED PORT DIVERGENCE (platform glue) -- never land on `slow'.
        // host that reaches this arm is never the 300 MHz / 32 MB machine retail's
        Cbuf_AddText(va("%s%s%s", EXEC_PREFIX, cpuEntries.ObjectAt(1)->command.c_str(), EXEC_SUFFIX));
    } else {
        Cbuf_AddText(va("%s%s%s", EXEC_PREFIX, defaultCommand.c_str(), EXEC_SUFFIX));
    }

    Cbuf_Execute(0);
}

void SystemSetup::configureMemoryOptions(void)
{
    MemorySetupEntry *match = NULL;

    for (int i = 1; i <= memoryEntries.NumObjects(); i++) {
        MemorySetupEntry *entry = memoryEntries.ObjectAt(i);
        if (!entry) {
            continue;
        }

        if (entry->doesMatch(systemInfo.videoRamMB())) {
            match = entry;
            break;
        }
    }

    if (match) {
        Cbuf_AddText(va("%s%s%s", EXEC_PREFIX, match->command.c_str(), EXEC_SUFFIX));
    } else if (memoryEntries.NumObjects() > 0) {
        // SANCTIONED PORT DIVERGENCE (platform glue) -- never land on `memorylow'.
        // global/memoryconfig.sys in pak0.pk3) can never match and retail's
        Cbuf_AddText(va("%s%s%s", EXEC_PREFIX, memoryEntries.ObjectAt(1)->command.c_str(), EXEC_SUFFIX));
    } else {
        Cbuf_AddText(va("%s%s%s", EXEC_PREFIX, defaultCommand.c_str(), EXEC_SUFFIX));
    }

    Cbuf_Execute(0);
}

void SystemSetup::configureVideoOptions(void) {}

void SystemSetup::configureOptions(void)
{
    str cpuFile = CPU_CONFIG_FILE;

    if (readCPUSetup(cpuFile)) {
        configureCPUOptions();
    } else {
        Com_WPrintf(CPU_READ_WARNING, cpuFile.c_str());
    }

    str videoFile = "global/videoconfig.sys";
    configureVideoOptions();

    str memoryFile = MEMORY_CONFIG_FILE;
    if (readMemorySetup(memoryFile)) {
        configureMemoryOptions();
    } else {
        Com_WPrintf(MEMORY_READ_WARNING, memoryFile.c_str());
    }
}

void CL_SetupSystem(void)
{
    if (!cl_systemsetup) {
        return;
    }

    if (cl_systemsetup->value == 0.0f) {
        return;
    }

    SystemSetup setup;

    setup.configureOptions();

    Cvar_Set("cl_systemsetup", "0");
}

void CL_SetupSystem_f(void)
{
    Cvar_Set("cl_systemsetup", "1");
    CL_SetupSystem();
}
