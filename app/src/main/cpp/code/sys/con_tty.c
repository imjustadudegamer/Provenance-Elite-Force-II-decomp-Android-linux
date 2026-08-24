
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#ifndef DEDICATED
#include "../client/client.h"
#endif

#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>


extern qboolean stdinIsATTY;
static qboolean stdin_active;
static qboolean ttycon_on = qfalse;
static int ttycon_hide = 0;
static int ttycon_show_overdue = 0;

static int TTY_erase;
static int TTY_eof;

static struct termios TTY_tc;

static field_t TTY_con;

#define CON_HISTORY 32
static field_t ttyEditLines[ CON_HISTORY ];
static int hist_current = -1, hist_count = 0;

#ifndef DEDICATED
#define TTY_CONSOLE_PROMPT "tty]"
#else
#define TTY_CONSOLE_PROMPT "]"
#endif

static void CON_Back( void )
{
	char key;
	size_t Q_UNUSED_VAR size;

	key = '\b';
	size = write(STDOUT_FILENO, &key, 1);
	key = ' ';
	size = write(STDOUT_FILENO, &key, 1);
	key = '\b';
	size = write(STDOUT_FILENO, &key, 1);
}

static void CON_Hide( void )
{
	if( ttycon_on )
	{
		int i;
		if (ttycon_hide)
		{
			ttycon_hide++;
			return;
		}
		if (TTY_con.cursor>0)
		{
			for (i=0; i<TTY_con.cursor; i++)
			{
				CON_Back();
			}
		}
		for (i = strlen(TTY_CONSOLE_PROMPT); i > 0; i--) {
			CON_Back();
		}
		ttycon_hide++;
	}
}

static void CON_Show( void )
{
	if( ttycon_on )
	{
		int i;

		assert(ttycon_hide>0);
		ttycon_hide--;
		if (ttycon_hide == 0)
		{
			size_t Q_UNUSED_VAR size;
			size = write(STDOUT_FILENO, TTY_CONSOLE_PROMPT, strlen(TTY_CONSOLE_PROMPT));
			if (TTY_con.cursor)
			{
				for (i=0; i<TTY_con.cursor; i++)
				{
					size = write(STDOUT_FILENO, TTY_con.buffer+i, 1);
				}
			}
		}
	}
}

void CON_Shutdown( void )
{
	if (ttycon_on)
	{
		CON_Hide();
		tcsetattr (STDIN_FILENO, TCSADRAIN, &TTY_tc);
	}

	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & ~O_NONBLOCK);
}

void Hist_Add(field_t *field)
{
	int i;

	if (!field->cursor)
		return;

	assert(hist_count <= CON_HISTORY);
	assert(hist_count >= 0);
	assert(hist_current >= -1);
	assert(hist_current <= hist_count);
	for (i=CON_HISTORY-1; i>0; i--)
	{
		ttyEditLines[i] = ttyEditLines[i-1];
	}
	ttyEditLines[0] = *field;
	if (hist_count<CON_HISTORY)
	{
		hist_count++;
	}
	hist_current = -1; 
}

field_t *Hist_Prev( void )
{
	int hist_prev;
	assert(hist_count <= CON_HISTORY);
	assert(hist_count >= 0);
	assert(hist_current >= -1);
	assert(hist_current <= hist_count);
	hist_prev = hist_current + 1;
	if (hist_prev >= hist_count)
	{
		return NULL;
	}
	hist_current++;
	return &(ttyEditLines[hist_current]);
}

field_t *Hist_Next( void )
{
	assert(hist_count <= CON_HISTORY);
	assert(hist_count >= 0);
	assert(hist_current >= -1);
	assert(hist_current <= hist_count);
	if (hist_current >= 0)
	{
		hist_current--;
	}
	if (hist_current == -1)
	{
		return NULL;
	}
	return &(ttyEditLines[hist_current]);
}


void CON_SigCont(int signum)
{
	CON_Init();
}

void CON_Init( void )
{
	struct termios tc;

	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	signal(SIGCONT, CON_SigCont);

	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK );

	if (!stdinIsATTY)
	{
		Com_Printf("tty console mode disabled\n");
		ttycon_on = qfalse;
		stdin_active = qtrue;
		return;
	}

	Field_Clear(&TTY_con);
	tcgetattr (STDIN_FILENO, &TTY_tc);
	TTY_erase = TTY_tc.c_cc[VERASE];
	TTY_eof = TTY_tc.c_cc[VEOF];
	tc = TTY_tc;

	tc.c_lflag &= ~(ECHO | ICANON);

	tc.c_iflag &= ~(ISTRIP | INPCK);
	tc.c_cc[VMIN] = 1;
	tc.c_cc[VTIME] = 0;
	tcsetattr (STDIN_FILENO, TCSADRAIN, &tc);
	ttycon_on = qtrue;
	ttycon_hide = 1; 
	CON_Show();
}

char *CON_Input( void )
{
	static char text[MAX_EDIT_LINE];
	int avail;
	char key;
	field_t *history;
	size_t Q_UNUSED_VAR size;

	if(ttycon_on)
	{
		avail = read(STDIN_FILENO, &key, 1);
		if (avail != -1)
		{
			if ((key == TTY_erase) || (key == 127) || (key == 8))
			{
				if (TTY_con.cursor > 0)
				{
					TTY_con.cursor--;
					TTY_con.buffer[TTY_con.cursor] = '\0';
					CON_Back();
				}
				return NULL;
			}
			if ((key) && (key) < ' ')
			{
				if (key == '\n')
				{
#ifndef DEDICATED
					if (TTY_con.buffer[0] == '/' || TTY_con.buffer[0] == '\\') {
						Q_strncpyz(text, TTY_con.buffer + 1, sizeof(text));
					} else if (TTY_con.cursor) {
						Q_strncpyz(text, TTY_con.buffer, sizeof(text));
					} else {
						text[0] = '\0';
					}

					Hist_Add(&TTY_con);
					CON_Hide();
					Com_Printf("%s%s\n", TTY_CONSOLE_PROMPT, TTY_con.buffer);
					Field_Clear(&TTY_con);
					CON_Show();
#else
					Hist_Add(&TTY_con);
					Q_strncpyz(text, TTY_con.buffer, sizeof(text));
					Field_Clear(&TTY_con);
					key = '\n';
					size = write(STDOUT_FILENO, &key, 1);
					size = write(STDOUT_FILENO, TTY_CONSOLE_PROMPT, strlen(TTY_CONSOLE_PROMPT));
#endif
					return text;
				}
				if (key == '\t')
				{
					CON_Hide();
					Field_AutoComplete( &TTY_con );
					CON_Show();
					return NULL;
				}
				avail = read(STDIN_FILENO, &key, 1);
				if (avail != -1)
				{
					if (key == '[' || key == 'O')
					{
						avail = read(STDIN_FILENO, &key, 1);
						if (avail != -1)
						{
							switch (key)
							{
								case 'A':
									history = Hist_Prev();
									if (history)
									{
										CON_Hide();
										TTY_con = *history;
										CON_Show();
									}
									tcflush(STDIN_FILENO, TCIFLUSH);
									return NULL;
									break;
								case 'B':
									history = Hist_Next();
									CON_Hide();
									if (history)
									{
										TTY_con = *history;
									} else
									{
										Field_Clear(&TTY_con);
									}
									CON_Show();
									tcflush(STDIN_FILENO, TCIFLUSH);
									return NULL;
									break;
								case 'C':
									return NULL;
								case 'D':
									return NULL;
							}
						}
					}
				}
				Com_DPrintf("droping ISCTL sequence: %d, TTY_erase: %d\n", key, TTY_erase);
				tcflush(STDIN_FILENO, TCIFLUSH);
				return NULL;
			}
			if (TTY_con.cursor >= sizeof(text) - 1)
				return NULL;
			TTY_con.buffer[TTY_con.cursor] = key;
			TTY_con.cursor++; 
			size = write(STDOUT_FILENO, &key, 1);
		}

		return NULL;
	}
	else if (stdin_active)
	{
		int     len;
		fd_set  fdset;
		struct timeval timeout;

		FD_ZERO(&fdset);
		FD_SET(STDIN_FILENO, &fdset); 
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if(select (STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(STDIN_FILENO, &fdset))
			return NULL;

		len = read(STDIN_FILENO, text, sizeof(text));
		if (len == 0)
		{ 
			stdin_active = qfalse;
			return NULL;
		}

		if (len < 1)
			return NULL;
		text[len-1] = 0;    

		return text;
	}
	return NULL;
}

void CON_Print( const char *msg )
{
	if (!msg[0])
		return;

	CON_Hide( );

	fputs( msg, stderr );

	if (!ttycon_on) {
		return;
	}

	if (msg[strlen(msg) - 1] == '\n') {
		CON_Show();

		while (ttycon_show_overdue > 0) {
			CON_Show();
			ttycon_show_overdue--;
		}
	}
	else
	{
		ttycon_show_overdue++;
	}
}
