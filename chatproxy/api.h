/*
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
 *	api.h -- API for chatproxy-plugin
 */

#if (!defined HAVE_API_H)
#	define HAVE_API_H

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

extern struct s_irc_msg irc_parse (char *msg);
extern void pseudo_notice (int fd, char *who, char *fmt, ...);
extern char *ctcp_version (char *ln, char *mod_ver);

extern char *chatproxy_ver;
extern int do_daemonize;

int  mod_set_conf (void **mod_conf, int argc, char **argv);

int  mod_set_session_conf   (void *mod_conf, struct s_auth *auth, int fd);
void mod_unset_session_conf (void *mod_conf);

int  mod_load   (void *mod_conf);
void mod_unload (void *mod_conf);

#endif
