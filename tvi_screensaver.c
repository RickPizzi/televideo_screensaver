/* -------------------------------------------------------------------------- */
/* tvi_screensaver -  screen saver with lock for Televideo terminals          */
/* -------------------------------------------------------------------------- */

/* Based on Sclocka - https://gitlab.com/mezantrop/sclocka */
/* Adapted for Televideo terminals by Rick Pizzi pizzi@leopardus.com */

/*
* Copyright (c) 2019-2023, Mikhail Zakharov <zmey20000@yahoo.com>
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
*    list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* -------------------------------------------------------------------------- */
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>

#if (WITH_PAM)
    #include <security/pam_appl.h>
#endif

#if defined (__FreeBSD__)
    #include <libutil.h>
#endif

#if defined(__APPLE__) || defined(__NetBSD__) || defined(__OpenBSD__)
    #include <util.h>
#endif

#if defined (__linux__)
    #include <pty.h>
    #include <utmp.h>
#endif

/* -------------------------------------------------------------------------- */
#define __PROGRAM       "tvi_screensaver - screen saver/lock for terminals"

#define NBF_STDOUT()    setvbuf(stdout, NULL, _IONBF, 0)
#define LBF_STDOUT()    setvbuf(stdout, NULL, _IOLBF, 0)
#define HIDE_CURSOR() fputs("\033.", stdout)           /* Hide cursor */
#define SHOW_CURSOR() fputs("\033.", stdout)           /* Show cursor */

#define CLL()         fputs("\033T", stdout)             /* Clear line to ^*/
#define CLS()         fputs("\033*", stdout)       	/* Clear screen */
#define CLR()         fputs("\033[0m", stdout)             /* Reset graphic */
#define ASCR()        fputs("\033[?1049h", stdout)         /* Alt scr buffer */
#define NSCR()        fputs("\033[?1049l", stdout)         /* normal scr buffer */

/* Get cursor position */
#define GET_POS(y, x) {\
    fputs("\033[6n", stdout);\
    scanf("\033[%d;%dR", &y, &x);\
}

#define PWD_PROMPT      "\rpassword: "
#define IVAL            5               /* Default start interval in secs */
#define BUFSZ           1024            /* Various buffers */

#define RSCR_NONE       'n'
#define RSCR_FMFD       'f'
#define RSCR_BUFF       'b'
#define RSCR_CAPS       'c'
#define RSCR_DEFT       RSCR_FMFD

#if (WITH_PAM)
    #define PAM_SERV        "login"
#endif

void quit(int ecode);
void wait4child(pid_t pid);
#if (WITH_PAM)
    int read_password(char ch, int lock, char *user,
        char *host, char *pam_service);
    int pam_auth(char *user, char *service);
#endif
void usage(int ecode);
void print_buffer(char scrbuf[]);

int master, slave;      /* PTY master/slave parts */
struct termios tt;      /* Original terminal capabilities */
#if (WITH_PAM)
    char passwd[PAM_MAX_RESP_SIZE];
    int pwd_ofs = 0;
#endif
int lock = 2;           /* 0: password unlocked; 1: locked; 2: locked 1st run */
int cursor_visible = 1;

/* -------------------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    static struct termios rtt, stt;     /* Current&temp terminal capabilities */
    struct winsize win;
    fd_set rfd;
    int n, cc;
    pid_t pid;
    char *sh = NULL;                    /* Shell to load */
    char buf[64 * BUFSZ] = {0};         /* Terminal IO buffer */
    char scrbuf[2000] = {0};   /* The screen buffer */
    int scrbufp = 0;                    /* Offset in the screen buffer */
    time_t tvec, start;
    struct timeval tv;
    int in_show = 0;                    /* terminal blanked when 1 */
    int reblank = 0;

    int flg;                            /* Command-line options flag */
    int cflg = 1;                       /* Clear screen on the first run */
    int cls = 1;                        /* Perform screen cleaning or not */
    long ival = IVAL;                   /* Start interval in secs */
    char bflg = RSCR_BUFF;              /* Default restore screen method */

    #if (WITH_PAM)
        char *user;                     /* username */
        char host[MAXHOSTNAMELEN];      /* hostname */
        int pflg = 1;                   /* Enable password check */
        char *pam_service = PAM_SERV;   /* PAM service to use */
    #else
        int pflg = 0;                   /* Disable password check */
    #endif
    unsigned char ff = 0x0C;            /* Form Feed */

    #if (WITH_PAM)
        while ((flg = getopt(argc, argv, "b:ci:pP:h")) != -1)
    #else
        while ((flg = getopt(argc, argv, "b:ci:h")) != -1)
    #endif
        switch(flg) {
            case 'b':
                /* Restore the screen after the saver:
                (n)one, (b)uffer or default terminal (c)apabilities */
                if (*optarg != RSCR_NONE &&
                    *optarg != RSCR_FMFD &&
                    *optarg != RSCR_BUFF &&
                    *optarg != RSCR_CAPS) usage(1);
                bflg = *optarg;
                break;
            case 'c':
                cls = cflg = 0;     /* Do not clean the screen on the 1-st run */
                break;
            case 'i':
                if ((ival = strtol(optarg, (char **)NULL, 10)) < 1) {
                    fprintf(stderr, "FATAL: wrong [-i interval] value");
                    usage(1);
                }
                break;
        #if (WITH_PAM)
            case 'p':
                pflg = 0;           /* Disable password check */
                break;
            case 'P':
                pam_service = optarg;
                break;
        #endif
            case 'h':
            default:
                usage(0);
        }

    if (isatty(STDIN_FILENO)) {
        if (tcgetattr(STDIN_FILENO, &tt) == -1) {
            fprintf(stderr, "FATAL@tcgetattr() %s\n", strerror(errno));
            exit(1);
        }

        if (ioctl(STDIN_FILENO, TIOCGWINSZ, &win) == -1) {
            fprintf(stderr, "FATAL@ioctl() %s\n", strerror(errno));
            exit(1);
        }

        if (openpty(&master, &slave, NULL, &tt, &win) == -1) {
            fprintf(stderr, "FATAL@openpty() %s\n", strerror(errno));
            exit(1);
        }
    } else {
        fprintf(stderr, "FATAL: Can run on terminals only\n");
        exit(1);
    }

    /* Make terminal raw and disable echo */
    rtt = tt;
    cfmakeraw(&rtt);
    rtt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &rtt);

    /* -- Error in fork() --------------------------------------------------- */
    if ((pid = fork()) < 0) {
        fprintf(stderr, "FATAL@fork() %s\n", strerror(errno));
        quit(1);
    }

    /* -- Child spawns a shell ---------------------------------------------- */
    if (pid == 0) {
        if (!(sh = getenv("SHELL"))) sh = _PATH_BSHELL;
        close(master);
        login_tty(slave);
        execl(sh, sh, "-l", (char *)NULL);
        exit(0);
    }

    /* -- Parent continues here --------------------------------------------- */
    #if (WITH_PAM)
        user = getlogin();
        gethostname(host, sizeof(host));
    #endif

    NBF_STDOUT(); 
    start = time(0);                        /* Set initial time */
    srand(start);                           /* randomize seed for screensaver */

    for (;;) {
        tv.tv_sec = 0;
        tv.tv_usec = 64 * 1000;          /* Microseconds */

        FD_SET(master, &rfd);
        FD_SET(STDIN_FILENO, &rfd);

        n = select(master + 1, &rfd, 0, 0, &tv);
        if (n < 0 && errno != EINTR) break;
        if (n > 0) {
            start = time(0);                /* Update start time on IO */

            /* -- INPUT ----------------------------------------------------- */
            if (FD_ISSET(STDIN_FILENO, &rfd)) {
                cc = read(STDIN_FILENO, buf, lock ? 1 : sizeof(buf));

                /* Child is dead? Finish operations */
                if (cc < 0) break;

                /* Write EOF */
                if (cc == 0)
                    if (tcgetattr(master, &stt) == 0 &&
                        (stt.c_lflag & ICANON) != 0)
                            write(master, &stt.c_cc[VEOF], 1);

                /* Write data to terminal */
                if (cc > 0) {
                    if (!in_show || !lock)
			 write(master, buf, cc);
                    else {
                        /* Ask for password */
#if (WITH_PAM)
			if (pflg) {
			    if (buf[0] == ' ' && !reblank) {
		    		if (lock && in_show) {
					print_buffer(scrbuf);
					if (!cursor_visible) {
						SHOW_CURSOR();
						cursor_visible=1;
					}
					reblank=1;
		    		}
			    }
			    else
                            	lock = read_password(buf[0],
                                	lock, user, host, pam_service);
			}
			else
				lock = 0;
#endif
                        if (cflg) cls = 1;

                        if (!pflg || !lock) {
                            /* Allow user to proceed */
			    if (!cursor_visible) {
				SHOW_CURSOR();
				cursor_visible=1;
			    }
                            if (bflg == RSCR_FMFD)
                                write(master, &ff, 1);  /* Send Form Feed */
                            else if (bflg == RSCR_CAPS)
                                NSCR();
                            else if (bflg == RSCR_BUFF) {
                                /* from the screen buffer, write out contents
                                of the current screen + 2*current screen */

				if (!reblank)
					print_buffer(scrbuf);
			    }
			    if (!lock) {
                            	in_show = 0;

                            	/* Drop current input buffer when leaving a show */
                            	memset(buf, 0, sizeof(buf));
                            	cc = 0;
                            	lock = 1;       /* lock again */
			    }
                        }
                    }
                }
            }

            /* -- OUTPUT ---------------------------------------------------- */
            if (FD_ISSET(master, &rfd)) {
                if ((cc = read(master, buf, sizeof(buf))) > 0) {
		    if (lock && in_show && !reblank) {
			print_buffer(scrbuf);
			if (!cursor_visible) {
				SHOW_CURSOR();
				cursor_visible=1;
			}
			reblank=1;
		    }
                    write(STDOUT_FILENO, buf, cc);
                    if (cc && bflg == RSCR_BUFF) {
                        scrbufp = sizeof(scrbuf) - cc;
                        memmove(scrbuf, scrbuf + cc, scrbufp);
                        memmove(scrbuf + scrbufp, buf, cc);
                    }
                } else break;
            }
        }

        /* -- Start show if the time has come ------------------------------- */
        tvec = time(0);
        if ((tvec - start) > ival * 60) {
            /* capture the screen, clear it (if needed) and hide cursor */
            if (bflg != RSCR_CAPS)
                if (cls) {
                    CLR(); CLS(); cls = 0;
                }

            if (!in_show || reblank) {
		reblank=0;
                if (bflg == RSCR_CAPS) {
                    ASCR();
                    CLR(); CLS();
                } else
                    cls = cflg ? 1 : 0;
            }

	    if (cursor_visible) {
            	HIDE_CURSOR();
		cursor_visible=0;
	    }

            in_show = 1;
            lock = 2;               /* Reset lock to first try */

        }
    }

    wait4child(pid);
    quit(0);
}

/* -------------------------------------------------------------------------- */
#if (WITH_PAM)
int read_password(char ch, int lock, char *user, char *host, char *pam_service) {
    if (lock == 2) {
        /* Clear screen, draw prompt on the first I/O */
	if (!cursor_visible) {
		SHOW_CURSOR();
		cursor_visible=1;
	}
	printf(PWD_PROMPT, user, host);
        return 1;
    }

    /* Not locked --> exit */
    if (!lock) return 0;

    /* Get password char-by-char; handle Enter and backspace keys */
    switch(ch) {
        case '\r':
        case '\n':                  /* Check password */
            fputs("\n\r", stdout);
            if (!(pam_auth(user, pam_service))) {
                memset(passwd, 0, sizeof(passwd));
                pwd_ofs = 0;
                CLL();
	    	fputs("terminal unlocked\n\r", stdout);
                return 0;
            }
	    if (pwd_ofs)
	    	fputs("password incorrect\n\r", stdout);
	    printf(PWD_PROMPT, user, host);
            memset(passwd, 0, sizeof(passwd));
            pwd_ofs = 0;
            break;
        case '\b':                  /* Backspace */
        case '\x7F':                /* Delete key as backspace  */
            if (pwd_ofs > 0) {
                pwd_ofs  = pwd_ofs  > 0 ? pwd_ofs - 1 : 0;
            	passwd[pwd_ofs] = '\0';
                putchar(ch);
            }
            break;
        default:                    /* Save char; print '*' */
            passwd[pwd_ofs++] = ch;
            putchar('*');
            break;
    }

    return 1;
}

/* -------------------------------------------------------------------------- */
int pam_conv(int n, const struct pam_message **msg, struct pam_response **resp,
    void *data) {

    struct pam_response *pr;
    int i;


    if (n <= 0 || n > PAM_MAX_NUM_MSG) return PAM_CONV_ERR;
    if ((pr = calloc(n, sizeof(*pr))) == NULL) return PAM_BUF_ERR;

    for (i = 0; i < n; i++) {
        pr[i].resp = NULL;
        pr[i].resp_retcode = 0;
        switch (msg[i]->msg_style) {
            case PAM_PROMPT_ECHO_OFF:
            case PAM_PROMPT_ECHO_ON:
                pr[i].resp = strdup(passwd);
                break;
            case PAM_ERROR_MSG:             /* Do we need this? */
            case PAM_TEXT_INFO:
                fprintf(stderr, "\n\r%s\n", msg[i]->msg);
                break;
            default:
                /* Clear possible passwords in responces; then free memory */
                for (i = 0; i < n; i++)
                    if (pr[i].resp) {
                        memset(pr[i].resp, 0, strlen(pr[i].resp));
                        free(pr[i].resp);
                    }
                free(pr);
                *resp = NULL;
                return PAM_CONV_ERR;
        }
    }
    *resp = pr;
    return PAM_SUCCESS;
}

/* -------------------------------------------------------------------------- */
int pam_auth(char *user, char *service) {
    static pam_handle_t *pamh;
    static struct pam_conv pamc;
    int rval;


    pamc.conv = &pam_conv;
    /* Pretend we want login service */
    rval = pam_start(service, user, &pamc, &pamh);
    if (rval == PAM_SUCCESS) rval = pam_authenticate(pamh, 0);
    if (pam_end(pamh, rval) != PAM_SUCCESS) pamh = NULL;

    return rval == PAM_SUCCESS ? 0 : 1;
}
#endif
/* -------------------------------------------------------------------------- */
void quit(int ecode) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tt);            /* Restore TTY */
    close(master);
    LBF_STDOUT();
    exit(ecode);
}

/* -------------------------------------------------------------------------- */
void wait4child(pid_t pid) {
    int e, status;

    if (waitpid(pid, &status, 0) == pid) {
        if (WIFEXITED(status)) e = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) e = WTERMSIG(status);
        else e = 1;                     /* Can't happen */
        quit(e);
    }
}

/* -------------------------------------------------------------------------- */
void usage(int ecode) {

#if (WITH_PAM)
	printf("%s\n\nUsage:\n\
\ttvi_screensaver [-b n|b|c] [-c] [-B] [-i n] [-s n] [-p] [-P service] [-h]\n\n\
[-b %c]\t\tScreen restore: (n)one, (f)ormfeed, (b)uffer, (c)apabilities\n\
[-c]\t\tDo not clear the window\n\
[-i %d]\t\tWait n minutes before launching the screensaver\n\
\n\
[-p]\t\tDisable PAM password check\n\
[-P %s]\tUse custom PAM service\n\
\n\
[-h]\t\tThis message\n\n", __PROGRAM, RSCR_DEFT, IVAL, PAM_SERV);
#else
	printf("%s\n\nUsage:\n\
\tstvi_screensaver [-b n|b|c] [-c] [-B] [-i n] [-s n] [-P service] [-h]\n\n\
[-b %c]\t\tScreen restore: (n)one, (f)ormfeed, (b)uffer, (c)apabilities\n\
[-c]\t\tDo not clear the window\n\
[-i %d]\t\tWait n minutes before launching the screensaver\n\
\n\
[-h]\t\tThis message\n\n", __PROGRAM, RSCR_DEFT, IVAL);
#endif
    exit(ecode);
}

void print_buffer(scrbuf)
char scrbuf[];
{
	int k;
	char *p;
	static struct Sl {
		char *start;
		char *end;
		int nl;
	} sl[2000];
	int nlc=0, lc=0;
	
	for (sl[0].start = scrbuf; sl[0].start < scrbuf + 2000; sl[0].start++)
		if (*sl[0].start != 0x00)
			break;
	for (p = scrbuf; p < scrbuf + 2000; p++) {
		if (*p == 0x0a) {
			sl[nlc].end=p;
			sl[nlc+1].start=p+1;
			nlc++;
		}
	}
	for (k = 0; k < nlc; k++)
		sl[k].nl = 1 + (int)(sl[k].end -sl[k].start) / 80;
	for (k = nlc -1; k >= 0; k--) {
		if (lc + sl[k].nl < 24)
			lc += sl[k].nl;
		else
			break;
	}
	for (k = ++k; k < nlc; k++) {
		//printf("%d: start 0x%lx end 0x%lx len=%d ln=%d\r\n", k, (unsigned long)(sl[k].start), (unsigned long)(sl[k].end), (int)(sl[k].end -sl[k].start), sl[k].nl);
		for (p=sl[k].start; p <= sl[k].end; p++)
			putchar(*p);
	}
}
