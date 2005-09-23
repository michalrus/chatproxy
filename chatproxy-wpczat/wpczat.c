/*
 *    wpczat.so -- chatproxy module for czat.wp.pl
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
 *	wpczat.c -- module src file
 */


#include <ytils.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <sys/wait.h>
#include "api.h"

#define HTTP_AUTH_TMOUT 60 * 1000 * 1000

extern int errno;
char *mod_version = "20051023";

struct s_mod_conf {
	int cfd;
	int sfd;
	struct s_auth *auth;

	char *wp_user;
	char *wp_pass;
	char *wp_nick;

	char *r_nick;
	char *r_serv;

	char *a_pl_path;
	char *b_pl_path;
	char *img_path;

	char *hash_o;
	char *hash_t;
	char *hash_v;
	char *hash_w;
};

struct s_trans_args {
	struct s_mod_conf *mconf;
	char *done;
};

/* makes id from ircd-magic (s1) and applet-magic (s2) */
char *B (char *s1, char *s2)
{
	char *cH, *ba;
	char *abyte0, *abyte1, *abyte2;
	char byte0, byte1, byte2;
	int i1, j1;
	unsigned long long l1;

	char M[] = {
		2, 8, 7, 2, 11, 8, 0, 3, 14, 5,
		7, 3, 9, 4, 2, 11, 4, 15, 7, 7,
		5, 13, 1, 14, 11, 6, 4, 7, 4, 3,
		13, 9
	};

	char cW[] = {
		4, 7, 4, 3, 8, 10, 12, 0, 12, 8,
		2, 2, 10, 9, 3, 11, 2, 6, 8, 11,
		2, 9, 7, 11, 14, 9, 9, 0, 14, 0,
		4, 11
	};

	cH = cW;
	ba = M;

	for (i1 = 0; (i1 & 0x20) == 0; i1++) {
		ba[i1] = (char )((ba[i1] + i1) & 0xf);
		cH[i1] = (char )((cH[i1] - i1) & 0xf);
	}

	abyte0 = xstrdup(s2);
	abyte1 = xstrdup(s1);
	abyte2 = xmalloc(36 + 1);

	for (i1 = 0; i1 < 32; i1++) {
		byte0 = abyte0[i1];
		byte1 = abyte1[i1 & 7];

		byte0 = (char )(byte0 <= 57 ? byte0 - 48 : (byte0 - 97) + 10);
		byte1 = (char )(byte1 <= 57 ? byte1 - 48 : (byte1 - 97) + 10);
		byte0 = (char )(((byte0 ^ ba[i1] ^ byte1) + cH[i1]) & 0xf);
		byte0 = (char )(byte0 <= 9 ? byte0 + 48 : (byte0 + 97) - 10);

		abyte2[i1] = byte0;
	}

	l1 = getmtime();

	abyte0[0] = (char )(int )(l1 & 15L);
	l1 >>= 4;
	abyte0[1] = (char )(int )(l1 & 15L);
	l1 >>= 4;
	abyte0[2] = (char )(int )(l1 & 15L);
	abyte0[3] = (char )((-abyte0[0] - abyte0[1] - abyte0[2]) & 0xf);

	for (j1 = 0; j1 < 4; j1++) {
		byte2 = abyte0[j1];
		byte2 = (char )(byte2 <= 9 ? byte2 + 48 : (byte2 + 97) - 10);
		abyte2[j1 + 32] = byte2;
	}

	abyte2[36] = 0;

	xfree (abyte0);
	xfree (abyte1);

	return (char *)abyte2;
}

void fill_otvw (struct s_mod_conf *mconf)
{
	static char o[256];
	static char t[256];
	static char v[256];
	static char w[256];

	/*
	 * '@' <-> '<'
	 * '#' <-> '>'
	 * '&' <-> '{'
	 * '+' <-> '}'
	 * '!' <-> '\\'
	 *
	 * for safety (;
	 *
	 * here...
	 */

	char ac[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789±æê³ñó¶¿¼¡ÆÊ£ÑÓ¦¯¬[]_-^/:;?.,=)\\<>$%}{*(";
	int l, i1, j1, k1;

	for (l = 0; l < 256; l++) {
		t[l] = '_';
		w[l] = '_';
		o[l] = '_';
		v[l] = '_';
	}

	for(i1 = 0; i1 < strlen(ac); i1++) {
		t[(uchar) ac[i1]] = ac[i1];
		w[(uchar) ac[i1]] = ac[i1];
		o[(uchar) ac[i1]] = ac[i1];
		v[(uchar) ac[i1]] = ac[i1];
	}

	for(j1 = 65; j1 <= 90; j1++)
		/* t[j1] = Character.toLowerCase((char)j1); */
		t[j1] = (char) (j1 + 32);

	t[(uchar) '¡'] = '±';
	t[(uchar) 'Æ'] = 'æ';
	t[(uchar) 'Ê'] = 'ê';
	t[(uchar) '£'] = '³';
	t[(uchar) 'Ñ'] = 'ñ';
	t[(uchar) 'Ó'] = 'ó';
	t[(uchar) '¦'] = '¶';
	t[(uchar) '¯'] = '¿';
	t[(uchar) '¬'] = '¼';
	for(k1 = 97; k1 <= 122; k1++)
		/* w[k1] = Character.toUpperCase((char)k1); */
		w[k1] = (char) (k1 - 32);

	w[(uchar) '±'] = '¡';
	w[(uchar) 'æ'] = 'Æ';
	w[(uchar) 'ê'] = 'Ê';
	w[(uchar) '³'] = '£';
	w[(uchar) 'ñ'] = 'Ñ';
	w[(uchar) 'ó'] = 'Ó';
	w[(uchar) '¶'] = '¦';
	w[(uchar) '¿'] = '¯';
	w[(uchar) '¼'] = '¬';
	o[(uchar) '±'] = 'a';
	o[47] = 'b';
	o[(uchar) 'æ'] = 'c';
	o[58] = 'd';
	o[(uchar) 'ê'] = 'e';
	o[59] = 'f';
	o[(uchar) '³'] = 'l';
	o[63] = 'g';
	o[46] = 'k';
	o[(uchar) 'ñ'] = 'n';
	o[(uchar) 'ó'] = 'o';
	o[44] = 'p';
	o[61] = 'r';
	o[(uchar) '¶'] = 's';
	o[(uchar) '¿'] = 'z';
	o[(uchar) '¼'] = 'x';
	o[41] = '0';
	o[(uchar) '\\'] = '1';	/* here... */
	o[(uchar) '<'] = '2';	/* here... */
	o[(uchar) '>'] = '3';	/* here... */
	o[36] = '4';
	o[37] = '5';
	o[(uchar) '}'] = '6';	/* here... */
	o[(uchar) '{'] = '7';	/* here... */
	o[42] = '8';
	o[40] = '9';
	v[97] = '±';
	v[98] = '/';
	v[99] = 'æ';
	v[100] = ':';
	v[101] = 'ê';
	v[102] = ';';
	v[108] = '³';
	v[103] = '?';
	v[107] = '.';
	v[110] = 'ñ';
	v[111] = 'ó';
	v[112] = ',';
	v[114] = '=';
	v[115] = '¶';
	v[122] = '¿';
	v[120] = '¼';
	v[48] = ')';
	v[49] = '\\';	/* here...	*/
	v[50] = '<';	/* here...	*/
	v[51] = '>';	/* here...	*/
	v[52] = '$';
	v[53] = '%';
	v[54] = '}';	/* here...	*/
	v[55] = '{';	/* and here.	*/
	v[56] = '*';
	v[57] = '(';

	mconf->hash_o = o;
	mconf->hash_t = t;
	mconf->hash_v = v;
	mconf->hash_w = w;
}

char *nick_hash (struct s_mod_conf *mconf, char *s)
{
	char *ac = s;
	int l = strlen(ac);
	int i1 = 1;

	char *o = mconf->hash_o;
	char *t = mconf->hash_t;

	int i2, l1;

	char *ac1, *ac2;
	int j1, k1;
	int j2, k2, l2;
	char c;
	char *stringbuffer;

	size_t itr;

	if (ac[0] == '~') {
		ac = xstrdup(ac);
		ac[0] = 'a';
		l--;
	}
	else {
		ac1 = xmalloc (1 + l  + 1);
		strcpy (ac1 + 1, ac);
		ac1[0] = 'b';
		ac = ac1;
	}

	/*
	 * '@' -> '<'
	 * '#' -> '>'
	 * '&' -> '{'
	 * '+' -> '}'
	 * '!' -> '\\'
	 */

	for (itr = 0; itr < strlen(ac); itr++)
		if (ac[itr] == '@')
			ac[itr] = '<';
		else if (ac[itr] == '#')
			ac[itr] = '>';
		else if (ac[itr] == '&')
			ac[itr] = '{';
		else if (ac[itr] == '+')
			ac[itr] = '}';
		else if (ac[itr] == '!')
			ac[itr] = '\\';

	j1 = l >> 2;
	k1 = l & 3;
	if(k1 > 0)
		j1++;
	ac2 = xmalloc(j1 + j1 + 1  + 1);
	ac2[0] = '|';

	for (i2 = 0; i2 < j1; i2++) {
		j2 = 16;
		k2 = 0;
		l2 = 0;

		for(l1 = 0; l1 < 4 && i1 < strlen(ac); l1++) {
			j2 >>= 1;

			c = t[(uchar) ac[i1]];
			if(c != ac[i1]) {
				ac[i1] = c;
				k2 |= j2;
			}

			c = o[(uchar) ac[i1]];
			if(c != ac[i1]) {
				ac[i1] = c;
				l2 |= j2;
			}

			i1++;
		}

		if(k2 < 10)
			ac2[i2 + 1] = (char)(48 + k2);
		else
			ac2[i2 + 1] = (char)((97 + k2) - 10);
		if(l2 < 10)
			ac2[j1 + i2 + 1] = (char)(48 + l2);
		else
			ac2[j1 + i2 + 1] = (char)((97 + l2) - 10);
	}

	stringbuffer = saprintf("%s%s", ac, ac2);

	xfree (ac);
	xfree (ac2);

	return stringbuffer;
}

char *nick_unhash (struct s_mod_conf *mconf, char *s)
{
	int l = (strchr(s, '|') - s);
	int i1 = 0;
	char c = s[0];
	char *ac;
	int j1 = 0;

	char *v = mconf->hash_v;
	char *w = mconf->hash_w;

	int k1, i2, l1, l2, k2, i3, j2;
	char *ac1, *ac2;

	if (l == (char *) 0 - s)
		return xstrdup("");

	if (c == 'a') {
		ac = psubstr(s, 0, l);
		i1 = 1;
		ac[0] = '~';
		j1 = strlen(ac) - 1;
	}
	else if (c == 'b') {
		ac = psubstr(s, 1, l);
		j1 = strlen(ac);
	}
	else
		return xstrdup("");

	k1 = j1 >> 2;
	i2 = j1 & 3;

	if(i2 > 0)
		k1++;

	ac1 = psubstr(s, l + 1,      l + 1 + k1);
	ac2 = psubstr(s, l + 1 + k1, l + 1 + k1 + k1);

	for (l1 = 0; l1 < strlen(ac1); l1++) {
		k2 = 0;

		if (ac1[l1] >= 'a' && ac1[l1] <= 'f')
			k2 = (ac1[l1] - 97) + 10;
		else
			k2 = ac1[l1] - 48;

		l2 = 0;

		if (ac2[l1] >= 'a' && ac2[l1] <= 'f')
			l2 = (ac2[l1] - 97) + 10;
		else
			l2 = ac2[l1] - 48;

		i3 = 16;

		for(j2 = 0; j2 < 4 && i1 < strlen(ac); j2++) {
			i3 >>= 1;

			if((l2 & i3) == i3)
				ac[i1] = v[(uchar) ac[i1]];
			if((k2 & i3) == i3)
				ac[i1] = w[(uchar) ac[i1]];
			i1++;
		}
	}

	xfree (ac1);
	xfree (ac2);

	return ac;
}

/*
 * conv() (c) Adam Wysocki <gophi ma³pa chmurka.net>, www.gophi.rotfl.pl
 *
 * enum {
 * 	CP_ISO = 0,	// ISO-8859-2
 * 	CP_IBM,		// IBM-852 (MS-DOS)
 * 	CP_WIN		// CP-1250 (MS-Windows)
 * };
 */

char conv (size_t from, size_t to, char ch)
{
	static char ctab[3][19] = {
		{ 0xEA, 0xF3, 0xB1, 0xB6, 0xB3, 0xBF, 0xBC, 0xE6, 0xF1, 0xCA, 0xD3, 0xA1, 0xA6, 0xA3, 0xAF, 0xAC, 0xC6, 0xD1, 0 },
		{ 0xA9, 0xA2, 0xA5, 0x98, 0x88, 0xBE, 0xAB, 0x86, 0xE4, 0xA8, 0xE0, 0xA4, 0x98, 0x9D, 0xBD, 0x8D, 0x8F, 0xE3, 0 },
		{ 0xEA, 0xF3, 0xB9, 0x9C, 0xB3, 0xBF, 0x9F, 0xE6, 0xF1, 0xCA, 0xD3, 0xA5, 0x8C, 0xA3, 0xAF, 0x8F, 0xC6, 0xD1, 0 }
	};
	size_t i = 0;

	while (ctab[from][i])
		if (ch == ctab[from][i])
			return ctab[to][i];
		else
			i++;

	return ch;
}

char *su_352 (struct s_mod_conf *mconf, char *_str)
{
	char *str = xstrdup(_str);
	char *chan, *user, *host, *serv, *hnick, *mk, *real;
	char *rv, *nick;

	chan  = xstrtok_r(str, " ", &real);
	user  = xstrtok_r(0,   " ", &real);
	host  = xstrtok_r(0,   " ", &real);
	serv  = xstrtok_r(0,   " ", &real);
	hnick = xstrtok_r(0,   " ", &real);
	mk    = xstrtok_r(0,   " ", &real);

	nick = nick_unhash(mconf, hnick);

	rv = saprintf("%s %s %s %s %s %s %s", chan, user, host, serv, (!strlen(nick) ? hnick : nick), mk, real);

	xfree (nick);
	xfree (str);

	return rv;
}

char *su_353 (struct s_mod_conf *mconf, char *_str)
{
	char *rv = 0, *mk, *chan, *nicks, nickmk, *hnick, *nick;
	char *str = xstrdup(_str);
	char fn;

	mk   = xstrtok_r(str, " ", &nicks);
	chan = xstrtok_r(0,   " ", &nicks);

	if (*nicks == ':')
		nicks++;

	while ((hnick = xstrtok_r(0, " ", &nicks))) {
		nickmk = 0;

		if (*hnick == '@' || *hnick == '+') {
			nickmk = *hnick;
			hnick++;
		}

		nick = nick_unhash(mconf, hnick);

		if (nickmk && *nick)
			strsubs (&nick, saprintf("%c%s", nickmk, nick));

		fn = 1;

		if (!strlen(nick)) {
			xfree (nick);
			nick = hnick;
			if (nickmk)
				nick--;
			fn = 0;
		}

		if (rv)
			strsubs (&rv, saprintf("%s %s", rv, nick));
		else
			rv = saprintf("%s %s :%s", mk, chan, nick);

		if (fn)
			xfree (nick);
	}

	xfree (str);

	return rv;
}

char *su_whois (struct s_mod_conf *mconf, char *_str)
{
	char *rv, *hwho, *who, *msg;
	char *str = xstrdup(_str);
	char fn;

	hwho = xstrtok_r(str, " ", &msg);
	who = nick_unhash(mconf, hwho);

	fn = 1;

	if (!strlen(who)) {
		xfree (who);
		who = hwho;
		fn = 0;
	}

	rv = saprintf("%s %s", who, msg);

	if (fn)
		xfree (who);
	xfree (str);

	return rv;
}

char *server_unhash (struct s_mod_conf *mconf, char *str)
{
	struct s_irc_msg im;
	char *_str = xstrdup(str);
	char *rv;

	char *hnick, *host, *nick = 0, *tmp = 0;

	im = irc_parse(_str);

	if (!im.is_sb) {
		xfree (_str);
		return xstrdup(str);
	}

	if (strchr(im.sb, '!')) {
		hnick = xstrtok_r(im.sb, "!", &host);
		nick = nick_unhash(mconf, hnick);

		if (strlen(nick))
			im.sb = saprintf("%s!%s", nick, host);
		else
			im.sb = saprintf("%s!%s", hnick, host);

		xfree (nick);
	}
	else {
		nick = nick_unhash(mconf, im.sb);

		if (strlen(nick)) {
			im.sb = xstrdup(nick);
			xfree (nick);
		}
		else {
			xfree (nick);
			nick = 0;
		}
	}

	if (im.swr) {
		tmp = nick_unhash(mconf, im.swr);
		if (strlen(tmp))
			im.swr = tmp;
	}

	if (!strcmp(im.sth, "353"))
		im.det = su_353(mconf, im.det);
	else if (!strcmp(im.sth, "352"))
		im.det = su_352(mconf, im.det);
	else if (!strcmp(im.sth, "401")
	 || !strcmp(im.sth, "311")
	 || !strcmp(im.sth, "319")
	 || !strcmp(im.sth, "312")
	 || !strcmp(im.sth, "317")
	 || !strcmp(im.sth, "318"))
		im.det = su_whois(mconf, im.det);

	if (im.det)
		rv = saprintf(":%s %s %s %s", im.sb, im.sth, im.swr, im.det);
	else if (im.swr)
		rv = saprintf(":%s %s %s", im.sb, im.sth, im.swr);
	else if (im.sth)
		rv = saprintf(":%s %s", im.sb, im.sth);
	else
		rv = saprintf(":%s", im.sb);

	if (nick)			xfree (im.sb);
	if (im.swr)			xfree (tmp);
	if (!strcmp(im.sth, "353")
	 || !strcmp(im.sth, "352"))	xfree (im.det);

	xfree (_str);

	return rv;
}

char *client_hash (struct s_mod_conf *mconf, char *_str)
{
	char *rv;
	char *str, *cmd, *arg1, *arg2, *args;
	char *nnick, *nnick2 = 0;

	rv = _str;
	str = xstrdup(_str);
	cmd = xstrtok_r(str, " ", &args);

	if (!strcasecmp(cmd, "PRIVMSG")
	 || !strcasecmp(cmd, "NOTICE")
	 || !strcasecmp(cmd, "INVITE")) {
		arg1 = xstrtok_r(0, " ", &args);
		if (!arg1)
			goto finisz;

		if (*arg1 != '#' && *arg1 != '&' && *arg1 != '+')
			nnick = nick_hash(mconf, arg1);
		else
			nnick = xstrdup(arg1);

		if (args)
			rv = saprintf("%s %s %s", cmd, nnick, args);
		else
			rv = saprintf("%s %s",    cmd, nnick);

		xfree (nnick);
	}
	else if (!strcasecmp(cmd, "WHOIS")
	 || !strcasecmp(cmd, "WHOWAS")) {
		arg1 = xstrtok_r(0, " ", &args);
		if (!arg1)
			goto finisz;

		arg2 = xstrtok_r(0, " ", &args);

		nnick  = nick_hash(mconf, arg1);
		if (arg2)
			nnick2 = nick_hash(mconf, arg2);

		if (arg2)
			rv = saprintf("%s %s %s", cmd, nnick, nnick2);
		else
			rv = saprintf("%s %s",    cmd, nnick);

		xfree (nnick);
		if (arg2)
			xfree (nnick2);
	}


finisz:
	xfree (str);
	return (rv == _str ? xstrdup(rv) : rv);
}

char *format (char *str)
{
	char *tfnstr, *nstr, *rv, del;
	size_t itr, itr2;

	static char *to_cut[]   = { "<b>", "<i>", "<u>", "</b>", "</i>", "</u>", "</color>", "</size>", "</font>", 0 };
	static char *to_cut_b[] = { "<b=", "<i=", "<u=", "<color=", "<size=", "<font=", 0 };

	if (to_cut[0]) {
		nstr = xstrdup("");

		for (itr = 0; itr < strlen(str); itr++) {
			del = 0;

			for (itr2 = 0; to_cut[itr2]; itr2++)
				if (strlen(str + itr) >= strlen(to_cut[itr2]))
					if (!strncmp(str + itr, to_cut[itr2], strlen(to_cut[itr2]))) {
						str += itr + strlen(to_cut[itr2]);
						itr = -1;
						del = 1;
						break;
					}

			if (del)
				continue;

			nstr = xrealloc(nstr, strlen(nstr) + 2);
			nstr[strlen(nstr) + 1] = 0;
			nstr[strlen(nstr)]     = str[itr];
		}
	}
	else
		nstr = xstrdup(str);

	if (!to_cut_b[0])
		return nstr;

	rv = xstrdup("");
	tfnstr = nstr;
	del = 0;

	for (itr = 0; itr < strlen(nstr); itr++) {
		if (del && nstr[itr] == '>') {
			del = 0;
			continue;
		}

		for (itr2 = 0; to_cut_b[itr2]; itr2++)
			if (strlen(nstr + itr) >= strlen(to_cut_b[itr2]))
				if (!strncmp(nstr + itr, to_cut_b[itr2], strlen(to_cut_b[itr2]))) {
					nstr += itr + strlen(to_cut_b[itr2]);
					itr = -1;
					del = 1;
					break;
				}

		if (!del) {
			rv = xrealloc(rv, strlen(rv) + 2);
			rv[strlen(rv) + 1] = 0;
			rv[strlen(rv)]     = nstr[itr];
		}
	}

	xfree (tfnstr);
	return rv;
}

char *iso2win (char *buf, char r)
{
	size_t itr;

	for (itr = 0; itr < strlen(buf); itr++)
		if (r)
			buf[itr] = conv(2, 0, buf[itr]);
		else
			buf[itr] = conv(0, 2, buf[itr]);

	return buf;
}

int mod_set_conf (void **mod_conf, int argc, char **argv)
{
	static struct s_mod_conf mconf;

	bzero (&mconf, sizeof(mconf));

	if (argc != 2) {
		fprintf (stderr, "wpczat.so usage:\n"
			"\twpczat.so <wpczat-a.pl path with slash> <wpczat-b.pl pws>\n");
		return 0;
	}

	fill_otvw (&mconf);

/*
	char *tmp, *res;

	while (1) {
		tmp = readline("Line: ");
		res = nick_hash(&mconf, tmp);
		debug (0, "\"%s\"", res);
		xfree (res);
		xfree (tmp);
	}
*/

	mconf.a_pl_path = xstrdup(argv[0]);
	mconf.b_pl_path = xstrdup(argv[1]);

	mconf.wp_nick = readline("WPnick [with/out ~]: ");

	if (*(mconf.wp_nick) == '~')
		mconf.img_path = readline("PNG path: ");
	else {
		mconf.wp_user = readline("WPuser: ");
		mconf.wp_pass = xstrdup(getpass("WPpass: "));
	}

	mconf.r_nick = 0;
	mconf.r_serv = 0;

	do_daemonize = 0;

	*mod_conf = &mconf;

	return 1;
}

int mod_set_session_conf (void *mod_conf, struct s_auth *auth, int fd)
{
	struct s_mod_conf *mconf;

	mconf = (struct s_mod_conf *)mod_conf;

	mconf->auth = auth;
	mconf->cfd = fd;

	return 1;
}

int get_amagic (int cfd, char **key, struct s_mod_conf *mconf)
{
	char *plargv[2], *tmp;
	int wfd, rfd, rc, status;
	pid_t cpid;

	if (*(mconf->wp_nick) == '~')
		plargv[0] = mconf->a_pl_path;
	else
		plargv[0] = mconf->b_pl_path;

	plargv[1] = 0;
	cpid = bipipe (&wfd, &rfd, plargv[0], plargv);

	if (*(mconf->wp_nick) == '~') {
		writes (wfd, mconf->wp_nick);  writes (wfd, "\n");
		writes (wfd, mconf->img_path); writes (wfd, "\n");

		debug (0, "0.");
		rc = tareadln (rfd, &tmp, HTTP_AUTH_TMOUT);
		debug (0, "1. %s", tmp);

		if (rc < 1) {
			*key = xstrdup("");
			xfree (tmp);
			return 0;
		}

		if (strcmp(tmp, "ocr")) {
			*key = xstrdup("");
			xfree (tmp);
			return -1;
		}

		xfree (tmp);

		pseudo_notice (cfd, mconf->auth->nick, "Please read \"%s\" and send it to me using \"/QUOTE <key>\"", mconf->img_path);

		rc = tareadln (cfd, &tmp, HTTP_AUTH_TMOUT);
		if (rc < 1) {
			*key = xstrdup("");
			xfree (tmp);
			pseudo_notice (cfd, mconf->auth->nick, "Your time has passed. G'bye.");
			return 0;
		}

		writes (wfd, tmp); writes (wfd, "\n");
		xfree (tmp);
	}
	else {
		writes (wfd, mconf->wp_user); writes (wfd, "\n");
		writes (wfd, mconf->wp_pass); writes (wfd, "\n");
		writes (wfd, mconf->wp_nick); writes (wfd, "\n");
	}

	close (wfd);
	rc = tareadln (rfd, key, HTTP_AUTH_TMOUT);

	close (rfd);
	waitpid (cpid, &status, 0);

	if (rc != 32) {
		if (!rc)
			return 0;
		else
			return -1;
	}

	return 1;
}

int wp_auth (int fd, int cfd, char *amagic, struct s_mod_conf *mconf)
{
	struct s_irc_msg im;
	char *msg, *imagic, *key, *hnick;

	hnick = nick_hash(mconf, mconf->wp_nick);
	fdprintf (fd, "NICK %s\r\n", hnick);
	xfree (hnick);

	while (1) {
		if (tareadln(fd, &msg, 0) < 1) {
			xfree (msg);
			return 0;
		}

		im = irc_parse(msg);
		if (*(im.det) == ':')
			im.det++;

		if (!strcasecmp(im.sth, "MAGIC"))
			imagic = xstrdup(im.det);

		/*
		 * "Nickname is already in use."
		 *
		 * else if (!strcasecmp(im.sth, "433") 
		 *	continue;
		 */

		else {
			pseudo_notice (cfd, mconf->auth->nick, "Unknown server reply:");
			pseudo_notice (cfd, mconf->auth->nick, ":%s %s %s :%s", im.sb, im.sth, im.swr, im.det);
			xfree (msg);
			return 0;
		}

		break;
	}

	pseudo_notice (cfd, mconf->auth->nick, "iMagic: \"%s\", calculating passkey...", imagic);

	key = B(imagic, amagic);
	xfree (imagic);
	xfree (msg);

	pseudo_notice (cfd, mconf->auth->nick, "Passkey: \"%s\", sending...", key);

	/* one can also connect with other ipaddr/realname, just for safety */
	fdprintf (fd, "USER 192.168.1.1 8 %s :Czat-Applet\r\n", key);

	xfree (key);

	return 1;
}

void tr_375 (int fd, struct s_irc_msg *im, struct s_mod_conf *mconf)
{
	if (!mconf->r_nick) {
		mconf->r_nick = xstrdup(im->swr);
		debug (0, "0: \"%s\"", mconf->r_nick);
	}
	if (!mconf->r_serv) {
		mconf->r_serv = xstrdup(im->sb);
		debug (0, "1: \"%s\"", mconf->r_serv);
		fdprintf (fd, ":%s 001 %s :Fake welcome.\r\n", im->sb, im->swr);
	}

	fdprintf (fd, ":%s %s %s %s\r\n", im->sb, im->sth, im->swr, im->det);
}

void tr_WPJOIN (int fd, struct s_irc_msg *im)
{
	fdprintf (fd, ":%s JOIN %s %s\r\n", im->sb, im->swr, im->det);
}

void tr_PRIVMSG (int fd, struct s_irc_msg *im)
{
	size_t nbraces = 0;

	if (*(im->det) == ':')
		im->det++;

	if (!strncmp("\x03\x09{\x02", im->det, 4))
		while (nbraces < 3 && *(im->det) != 0)
			if (*(im->det++) == '}')
				nbraces++;

	fdprintf (fd, ":%s PRIVMSG %s :%s\r\n", im->sb, im->swr, im->det);
}

void cl_JOIN (int fd, char *args)
{
	fdprintf (fd, "WPJOIN %s\r\n", args);
}

char translate (int srcfd, int dstfd, char whfd, void *argv)
{
	char *buf, *buff, *cmd, *args;
	struct s_irc_msg im;
	struct s_trans_args *targs;

	targs = (struct s_trans_args *)argv;

	if (tareadln(srcfd, &buf, 0) < 1)
		return 1;

	if (whfd - 1 == 0) {
		strsubs (&buf, ctcp_version(buf, mod_version));
		strsubs (&buf, client_hash(targs->mconf, buf));
		debug (0, "-> \"%s\"", buf);
	}

	iso2win (buf, whfd - 1);

	if (whfd - 1) {
		strsubs (&buf, server_unhash(targs->mconf, buf));
		strsubs (&buf, format(buf));
		debug (0, "<- \"%s\"", buf);
	}

	buff = saprintf("%s\r\n", buf);

	if (whfd == 2) { /* if server talks */
		im = irc_parse(buf);

		if (!strcmp(im.sth, "375"))
			tr_375 (dstfd, &im, targs->mconf);
		else if (!strcasecmp(im.sth, "WPJOIN"))
			tr_WPJOIN (dstfd, &im);
		else if (!strcasecmp(im.sth, "PRIVMSG"))
			tr_PRIVMSG (dstfd, &im);
		else
			writes (dstfd, buff);
	}
	else { /* or if the client does */
		cmd = xstrtok_r(buf, " ", &args);

		if (!strcasecmp(cmd, "JOIN"))
			cl_JOIN (dstfd, args);
		else
			writes (dstfd, buff);
	}

	xfree (buf);
	xfree (buff);

	return 0;
}

int mod_load (void *mod_conf)
{
	struct s_mod_conf *mconf;
	struct s_trans_args targs;
	char *amagic;
	int rc;
	char done = 0;

	mconf = (struct s_mod_conf *)mod_conf;

	pseudo_notice (mconf->cfd, mconf->auth->nick, "Processing your connection, please wait.");
	pseudo_notice (mconf->cfd, mconf->auth->nick, "Getting aMagic, HTTP authentication.");

	rc = get_amagic (mconf->cfd, &amagic, mconf);
	if (rc < 1) {
		pseudo_notice (mconf->cfd, mconf->auth->nick, "get_amagic(): 0 (%s)", !rc ? "timeout" : "bad WPpass?");
		xfree (amagic);
		return 0;
        }

	pseudo_notice (mconf->cfd, mconf->auth->nick, "aMagic: \"%s\".", amagic);
        pseudo_notice (mconf->cfd, mconf->auth->nick, "Connecting to czati1.wp.pl:5579.");

	mconf->sfd = tcp_conn ("czati1.wp.pl", 5579);
	if (mconf->sfd < 0) {
		pseudo_notice (mconf->cfd, mconf->auth->nick, "tcp_conn(): %d", mconf->sfd);
		return 0;
	}

	pseudo_notice (mconf->cfd, mconf->auth->nick, "Connection established.");
	pseudo_notice (mconf->cfd, mconf->auth->nick, "IRCd authentication.");

	if (!wp_auth (mconf->sfd, mconf->cfd, amagic, mconf)) {
		pseudo_notice (mconf->cfd, mconf->auth->nick, "wp_auth(): 0");
		return 0;
	}

	pseudo_notice (mconf->cfd, mconf->auth->nick, "Translating data (chat <-> irc).");

	targs.mconf = mconf;
	targs.done = &done;

	debug (0, "passing data...");

	pass_data (mconf->cfd, mconf->sfd, &targs, translate);

	debug (0, "end of passing & end of mod_load");

	return 1;
}

void mod_unset_session_conf (void *mod_conf)
{
	struct s_mod_conf *mconf;

	debug (0, "mod_unset_conf()");

	mconf = (struct s_mod_conf *)mod_conf;

	xfree (mconf->r_nick);
	xfree (mconf->r_serv);

	mconf->r_nick = 0;
	mconf->r_serv = 0;
}

void mod_unload (void *mod_conf)
{
	struct s_mod_conf *mconf;

	mconf = (struct s_mod_conf *)mod_conf;

	if (*(mconf->wp_nick) == '~')
		xfree (mconf->img_path);
	else {
		xfree (mconf->wp_user);
		xfree (mconf->wp_pass);
	}

	xfree (mconf->wp_nick);

	xfree (mconf->a_pl_path);
	xfree (mconf->b_pl_path);
}
