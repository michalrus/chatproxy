/*
 *    chatproxy -- webchat tunneling utility
 *    Copyright (C) 2003-2005  Caesar Michael Rus <yodar@router.kom.pl>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 *	main.c -- main src file
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <errno.h>

#include <ytils.h>

extern int errno;
char *chatproxy_ver = "20050920";
int do_daemonize;

struct s_auth {
	char *nick;
	char *user;
	char *host;
	char *serv;
	char *real;
};

struct s_irc_msg {
	char *sb;
	char *sth;
	char *swr;
	char *det;
	char is_sb;
};

struct s_irc_msg irc_parse (char *msg)
{
	struct s_irc_msg rv;

	rv.sb  = xstrtok_r(msg, " ", &(rv.det));
	rv.sth = xstrtok_r(0,   " ", &(rv.det));
	rv.swr = xstrtok_r(0,   " ", &(rv.det));

	rv.is_sb = 0;

	if (*(rv.sb) == ':') {
		rv.is_sb = 1;
		rv.sb++;
	}

	return rv;
}

char *ctcp_version (char *ln, char *mod_ver)
{
	static char *mod_name;
	char *cmd, *nick, *msg, *ctcpt, *ctcpm;

	if (!mod_ver && !ln) {	/* free */
		xfree (mod_name);
		return 0;
	}
	else if (!mod_ver) {	/* set */
		mod_name = strrchr(ln, '/');
		mod_name = xstrdup(mod_name ? mod_name + 1 : ln);
		return 0;
	}

	cmd  = xstrtok_r(ln, " ", &msg);
	if (strcmp(cmd, "NOTICE"))
		return saprintf("%s %s", cmd, msg);

	nick = xstrtok_r(0,  " ", &msg);

	if (*msg == ':')
		msg++;

	if (*msg != 1)
		return saprintf("%s %s :%s", cmd, nick, msg);
	msg++;

	if (msg[strlen(msg) - 1] == 1)
		msg[strlen(msg) - 1] = 0;

	ctcpt = xstrtok_r(msg, " ", &ctcpm);

	if (!strcasecmp(ctcpt, "VERSION"))
		return saprintf("%s %s :\001%s chatproxy/%s %s/%s (%s)\001", cmd, nick, ctcpt, chatproxy_ver, mod_name, mod_ver, ctcpm);
	else
		return saprintf("%s %s :\001%s %s\001", cmd, nick, ctcpt, ctcpm);
}


void *xdlopen (char *path)
{
	void *hdl;

	if (!(hdl = dlopen(path, RTLD_LAZY)))
		debug (2, "%s", dlerror());

	return hdl;
}

void *xdlsym (void *hdl, const char *symbol)
{
	void *rv;
	char *err;

	dlerror ();	/* Clear any existing error */
	rv = dlsym(hdl, symbol);

	if ((err = dlerror()))
		debug (2, "%s", err);

	return rv;
}

struct s_auth authenticate (int fd)
{
	struct s_auth auth;
	char *res, *cmd, *args;
	int rc;

	memset (&auth, 0, sizeof(struct s_auth));

	while ((rc = tareadln(fd, &res, 0)) > 0) {
		cmd = xstrtok_r (res, " ", &args);

		if (!strcasecmp(cmd, "USER")) {
			auth.user = xstrdup(xstrtok_r(0, " ", &args));
			auth.host = xstrdup(xstrtok_r(0, " ", &args));
			auth.serv = xstrdup(xstrtok_r(0, " ", &args));

			if (args) {
				if (*args == ':')
					args++;

				auth.real = xstrdup(args);
			}

			if (auth.nick)
				break;
		}
		else if (!strcasecmp(cmd, "NICK")) {
			auth.nick = xstrdup(xstrtok_r(0, " ", &args));

			if (auth.user)
				break;
		}

		xfree (res);
	}

	xfree (res);

	if (rc < 1)
		debug (2, "tareadln(): %d", rc);

	return auth;
}

void auth_free (struct s_auth *auth)
{
	xfree (auth->nick);
	xfree (auth->user);
	xfree (auth->host);
	xfree (auth->serv);
	xfree (auth->real);
}

void pseudo_notice (int fd, char *who, char *fmt, ...)
{
	va_list args;
	char *msg, *smsg;

	va_start (args, fmt);
	msg = vsaprintf (fmt, args);
	va_end (args);

	smsg = saprintf (":chatproxy NOTICE %s :%s\r\n", who, msg);
	write (fd, smsg, strlen(smsg));

	xfree (msg);
	xfree (smsg);
}

int main (int argc, char **argv)
{
	int fd;
	unsigned short port;
	struct s_auth auth;
	void *mod_conf, *hdl;
	char done = 0;
	char *dtmp;

	int  (*mod_set_conf)           (void **, int, char **);
	int  (*mod_set_session_conf)   (void *, struct s_auth *, int);
	void (*mod_unset_session_conf) (void *);
	int  (*mod_load)               (void *);
	void (*mod_unload)             (void *);

	if (argc < 3) {
		fprintf (stderr, "Usage:\n\t%s lport plugin_path [plugin_argv]\n", argv[0]);
		exit (1);
	}

	printf ("Daemonize? [y/n]: ");
	fflush (stdout);
	tareadln (fileno(stdin), &dtmp, 0);
	if (!strcmp(dtmp, "y"))
		do_daemonize = 1;
	else
		do_daemonize = 0;
	xfree (dtmp);

	ctcp_version (argv[2], 0);

	hdl = xdlopen (argv[2]);

	mod_set_conf           = xdlsym(hdl, "mod_set_conf");
	mod_set_session_conf   = xdlsym(hdl, "mod_set_session_conf");
	mod_unset_session_conf = xdlsym(hdl, "mod_unset_session_conf");
	mod_load               = xdlsym(hdl, "mod_load");
	mod_unload             = xdlsym(hdl, "mod_unload");

	if (!(*mod_set_conf) (&mod_conf, argc - 3, argv + 3))
		debug (2, "mod_set_conf(): 0");

	if (do_daemonize) {
		debug (0, "Switching to daemon mode...");
		daemonize (0);
	}

	port = atoi(argv[1]);

	while (!done) {
		debug (0, "listening...");
		fd = tcp_accept_one(port, 5);
		debug (0, "got client");

		if (fd < 0)
			debug (2, "tcp_accept_one(): %d", fd);

		auth = authenticate (fd);

		if (!(*mod_set_session_conf) (mod_conf, &auth, fd))
			debug (2, "mod_set_session_conf(): 0");

		switch ((*mod_load) (mod_conf)) {
			case 0:
				debug (1, "mod_start(): 0");
				break;
			case -1:
				done = 1;
				break;
		}

		(*mod_unset_session_conf) (mod_conf);

		debug (0, "back to chatproxy (;");

		auth_free (&auth);
		debug (0, "auth freed");
		close (fd);
		debug (0, "fd closed...");
	}

	(*mod_unload) (mod_conf);

	dlclose(hdl);

	ctcp_version (0, 0);

	return 0;
}
