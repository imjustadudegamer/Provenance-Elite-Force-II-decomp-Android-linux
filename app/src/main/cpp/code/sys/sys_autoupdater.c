
#ifdef USE_AUTOUPDATER
#  ifndef AUTOUPDATER_BIN
#    error The build system should have defined AUTOUPDATER_BIN
#  endif

#  ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN 1
#    include <windows.h>
#  else
#    include <unistd.h>
#  endif

#  include <stdio.h>
#  include <string.h>

void Sys_LaunchAutoupdater(int argc, char **argv)
{
	#ifdef _WIN32
	PROCESS_INFORMATION procinfo;
	STARTUPINFO startinfo;
	char cmdline[128];
	memset(&procinfo, '\0', sizeof (procinfo));
	memset(&startinfo, '\0', sizeof (startinfo));
	startinfo.cb = sizeof (startinfo);
	sprintf(cmdline, "" AUTOUPDATER_BIN " --waitpid %u", (unsigned int) GetCurrentProcessId());

	if (CreateProcessA(AUTOUPDATER_BIN, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startinfo, &procinfo))
	{
		CloseHandle(procinfo.hProcess);
		CloseHandle(procinfo.hThread);
	}
	#else
	int updater_pipes[2];
	if (pipe(updater_pipes) == 0)
	{
		pid_t pid = fork();
		if (pid == -1)  
		{
			close(updater_pipes[0]);
			close(updater_pipes[1]);
		}
		else if (pid == 0)  
		{
			close(updater_pipes[1]);  
			if (dup2(updater_pipes[0], 3) != -1)
			{
				char pidstr[64];
				char *ptr = strrchr(argv[0], '/');
				if (ptr)
					*ptr = '\0';
				if (chdir(argv[0]) == -1) {
					_exit(1);  
				}
				snprintf(pidstr, sizeof (pidstr), "%lld", (long long) getppid());
				execl(AUTOUPDATER_BIN, AUTOUPDATER_BIN, "--waitpid", pidstr, NULL);
			}
			_exit(0);  
		}
		else   
		{
			close(updater_pipes[0]);
		}
	}
	#endif
}

#endif 
