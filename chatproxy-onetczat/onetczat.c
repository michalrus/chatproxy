/*
 *    onetczat.so -- chatproxy module for czat.onet.pl
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
 *	onetczat.c -- module src file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#include <ytils.h>
#include "api.h"

#define PRIV_CH '+'
#define HTTP_AUTH_TMOUT 60 * 1000 * 1000

char *onetczat_ver = "20051203";

char *sortkey[] = { "--key=1", "--key=3" };
char *sortway[] = { 0, "--numeric-sort" };
char sortcmd[] = "sort";
char *sortargv[] = { sortcmd, "--ignore-case", "--field-separator=:", 0, 0, NULL };

struct s_mod_conf {
	int cfd;
	int sfd;

	struct s_auth *auth;

	char *onet_uid;
	char *onet_pass;
	char *pl_path;

	char *r_servname;
	char *r_nick;

	int clist_wfd;
	int clist_rfd;

	size_t npriv;
	char **r_priv;
	char **c_priv;

	size_t njoined;
	char **joined;
};

struct s_trans_args {
	struct s_mod_conf *mconf;
	char *done;
};

/*
 * tkey()
 * (c) Przemyslaw Frasunek <przemyslaw@frasunek.com>, www.frasunek.com
 */

void tkey (char *key, char *nkey)
{
	static int f1[] = {
		40, 6, 26, 28, 35, 51, 24, 2, 31, 4, 
		8, 9, 53, 14, 21, 60, 57, 32, 10, 21, 
		13, 49, 22, 45, 23, 48, 34, 49, 1, 8, 
		24, 20, 16, 18, 60, 18, 57, 48, 5, 34, 
		26, 7, 5, 15, 59, 6, 7, 57, 15, 48, 
		43, 20, 46, 16, 35, 50, 49, 46, 57, 13, 
		47, 15, 59, 40, 4, 20, 24, 60, 49, 20, 
		0, 55, 14, 19, 49, 1, 27, 17
	};

	static int f2[] = {
		48, 41, 39, 56, 24, 47, 7, 30, 5, 17, 
		29, 28, 18, 18, 8, 34, 44, 6, 8, 56, 
		50, 7, 55, 53, 39, 36, 42, 36, 16, 26, 
		56, 29, 39, 37, 5, 26, 10, 2, 35, 49, 
		45, 2, 26, 12, 26, 15, 20, 1, 23, 43, 
		9, 23, 39, 9, 38, 6, 31, 58, 50, 40, 
		33, 10, 54, 26, 23, 28, 57, 28, 40, 38, 
		43, 30, 24, 36, 20, 13, 49, 11
	};

	static int f3[] = {
		18, 52, 19, 10, 29, 28, 14, 6, 1, 2, 
		46, 38, 57, 15, 43, 18, 16, 44, 54, 60, 
		30, 28, 39, 50, 24, 29, 54, 25, 44, 9, 
		7, 17, 48, 13, 54, 32, 16, 17, 12, 32, 
		59, 53, 46, 38, 10, 21, 20, 51, 25, 54, 
		10, 6, 56, 58, 16, 43, 19, 59, 24, 20, 
		45, 26, 47, 28, 10, 34, 11, 8, 20, 52, 
		44, 20, 9, 9, 20, 43, 32, 20
	};

	static int p1[] = {
		1, 5, 10, 11, 13, 9, 14, 0, 12, 15, 
		8, 2, 3, 4, 7, 6
	};

	static int p2[] = {
		6, 2, 8, 1, 5, 14, 0, 10, 9, 11, 
		13, 15, 4, 7, 12, 3
	};

	char ai[16], ai1[16], c;
	int i, j, k, l, i1, j1, k1, l1;

	for (i = 0; i < 16; i++) {
		c = key[i];
		ai[i] = (c > '9' ? c > 'Z' ? (c - 97) + 36 : (c - 65) + 10 : c - 48);
	}

	for (j = 0; j < 16; j++)
		ai[j] = f1[ai[j] + j];

	memcpy(ai1, ai, sizeof(ai1));

	for (k = 0; k < 16; k++)
		ai[k] = (ai[k] + ai1[p1[k]]) % 62;

	for (l = 0; l < 16; l++)
		ai[l] = f2[ai[l] + l];

	memcpy(ai1, ai, sizeof(ai1));

	for (i1 = 0; i1 < 16; i1++)
		ai[i1] = (ai[i1] + ai1[p2[i1]]) % 62;

	for (j1 = 0; j1 < 16; j1++)
		ai[j1] = f3[ai[j1] + j1];

	for (k1 = 0; k1 < 16; k1++) {
		l1 = ai[k1];
		ai[k1] = l1 >= 10 ? l1 >= 36 ? (97 + l1) - 36 : (65 + l1) - 10 : 48 + l1;
		nkey[k1] = ai[k1];
	}

	nkey[16] = 0;
}

char *dk42 (char *Ill) {
	int IIl, lIl[32], l1l[32];

	for(IIl = 0; IIl < 32; IIl++)
		lIl[IIl] = (int )Ill[IIl];

	l1l[0] = lIl[1] ^ 4;
	l1l[1] = lIl[5] ^ 14;
	l1l[2] = lIl[24] ^ 14;
	l1l[3] = lIl[12] ^ 13;
	l1l[4] = lIl[2] ^ 10;
	l1l[5] = lIl[25] ^ 14;
	l1l[6] = lIl[16] ^ 11;
	l1l[7] = lIl[29] ^ 1;
	l1l[8] = lIl[13] ^ 8;
	l1l[9] = lIl[3] ^ 13;
	l1l[10] = lIl[14] ^ 10;
	l1l[11] = lIl[19] ^ 5;
	l1l[12] = lIl[17] ^ 6;
	l1l[13] = lIl[28] ^ 7;
	l1l[14] = lIl[23] ^ 9;
	l1l[15] = lIl[6] ^ 8;
	l1l[16] = lIl[18] ^ 9;
	l1l[17] = lIl[7] ^ 12;
	l1l[18] = lIl[4] ^ 5;
	l1l[19] = lIl[21] ^ 8;
	l1l[20] = lIl[9] ^ 13;
	l1l[21] = lIl[30] ^ 2;
	l1l[22] = lIl[8] ^ 4;
	l1l[23] = lIl[15] ^ 0;
	l1l[24] = lIl[22] ^ 10;
	l1l[25] = lIl[27] ^ 4;
	l1l[26] = lIl[20] ^ 5;
	l1l[27] = lIl[11] ^ 12;
	l1l[28] = lIl[26] ^ 5;
	l1l[29] = lIl[0] ^ 2;
	l1l[30] = lIl[31] ^ 2;
	l1l[31] = lIl[10] ^ 10;

	for (IIl = 0; IIl < 32; IIl++) {

		/*
		 * ;>>
		 *
		 * if (l1l[IIl] > 58)
		 *	l1l[IIl] = 97 + lIl[IIl] % 6;
		 * else
		 *	l1l[IIl] = 48 + lIl[IIl] % 10;
		 */
 
		Ill[IIl] = (char )l1l[IIl];
	}

	return Ill;
}

/*
 * new dk42()
 */

char *onet_write (char *l11llIIl1l)
{
	/* int zavQGC = 0xc7d; */
	int zOiaaR = 0xb8d;
	int zSTDyf = 0x1523;
	int zDRDRe = 0x1017;
	int zbSopm = 0xd18;
	int zV_von = 0x14da;
	int zLDskC = 0xa40;
	int zlChYm = 0x1a45;
	int zrEztU = 0x12e9;
	int zNcqiA = 0x243b;
	int zHgwWU = 0x167d;
	int zxcMmv = 0xc57;
	int zBgVAw = 0x11e8;
	int zGcrFJ = 0x11cc;
	int zulksx = 0x1544;
	int zQEuVb = 0x1168;
	int zRsDQX = 0x1adf;
	int zpE_MO = 0x1ebd;
	int zoUvJa = 0x17b;
/*	int zbPk_O = 0x1aec;
	int zydlsp = 0xef9;
	int zQDJAf = 0x179d;
	int zjElLb = 0x36c;
	int z_eQhf = 0xd17;
	int zspHlW = 0x7f8;
	int zyhzKd = 0x15af;
	int zPKJRX = 0x21dc;
	int zRuTaf = 0x3ae;
	int zNhcnH = 0x158a;
	int z_xrIV = 0x2025;
	int zKJjvF = 0xb9a;
	int zXHLro = 0x1302;
	int zejOpS = 0x1303;
	int zMOnLJ = 0xcde;
	int zZTfeZ = 0xad6;
	int zBOkEA = 0x2155;
	int z_KMkD = 0xab2;
	int zcyJen = 0x15bf;
	int zHWEwO = 0x1520;
	int zTAAkq = 0x26ca;	*/

	char lIIII11IlI[33]; // 0x1d2 + 4839 - 0x1499 + \0
	char lllI11llII[33];
	char l1I1III11I[33];
	size_t lI1lI111l1;

	for(lI1lI111l1 = (0xaad + 7209 - 0x26d6); lI1lI111l1 < (0x1d2 + 4839 - 0x1499); lI1lI111l1++)
		lIIII11IlI[lI1lI111l1]=l11llIIl1l[lI1lI111l1];

	lllI11llII[0] =		lIIII11IlI[(0x54+2880-zOiaaR)]^(0x371+6602-0x1d31);
	lllI11llII[1] =		lIIII11IlI[(0x516+4119-zSTDyf)]^(0x1810+2065-0x2013);
	lllI11llII[2] =		lIIII11IlI[(0xcad+901-zDRDRe)]^(0x16b9+2385-0x2002);
	lllI11llII[3] =		lIIII11IlI[(0x9eb+4459-0x1b46)]^(0x49f+5366-0x1995);
	lllI11llII[4] =		lIIII11IlI[(0x74d+6749-0x2191)]^(0x7ac+6851-0x2267);
	lllI11llII[5] =		lIIII11IlI[(0x8f1+1068-zbSopm)]^(0x1055+2914-0x1bad);
	lllI11llII[6] =		lIIII11IlI[(0x1c+1367-0x55e)]^(0x50+227-0x131);
	lllI11llII[7] =		lIIII11IlI[(0xb44+2465-zV_von)]^(0x217+9367-0x26ad);
	lllI11llII[8] =		lIIII11IlI[(0x38f+3727-0x1215)]^(0xc28+69-0xc63);
	lllI11llII[9] =		lIIII11IlI[(0x2ff+8112-0x22a9)]^(0x4e5+1120-0x945);
	lllI11llII[10] =	lIIII11IlI[(0x94d+5350-0x1e2f)]^(0x6b7+911-zLDskC);
	lllI11llII[11] =	lIIII11IlI[(0x972+5390-0x1e62)]^(0x1636+1043-zlChYm);
	lllI11llII[12] =	lIIII11IlI[(0x1edc+61-0x1f18)]^(0x6a0+2829-0x11ac);
	lllI11llII[13] =	lIIII11IlI[(0x2d4+2249-0xb83)]^(0x1e02+181-0x1eaa);
	lllI11llII[14] =	lIIII11IlI[(0xcd5+1579-zrEztU)]^(0x3dc+613-0x640);
	lllI11llII[15] =	lIIII11IlI[(0x1da+2098-0xa0c)]^(0x8f2+6735-0x233b);
	lllI11llII[16] =	lIIII11IlI[(0xb6f+5423-0x209b)]^(0x629+2228-0xed1);
	lllI11llII[17] =	lIIII11IlI[(0x10e9+568-0x1309)]^(0x18be +2947-zNcqiA);
	lllI11llII[18] =	lIIII11IlI[(0x704+3975-zHgwWU)]^(0xe87+1207-0x1336);
	lllI11llII[19] =	lIIII11IlI[(0x207+2659-zxcMmv)]^(0x1b51+2672-0x25b8);
	lllI11llII[20] =	lIIII11IlI[(0x463+128-0x4c7)]^(0x95+6381-0x1974);
	lllI11llII[21] =	lIIII11IlI[(0x5c9+3111-zBgVAw)]^(0x596+673-0x82b);
	lllI11llII[22] =	lIIII11IlI[(0x176d+2181-0x1ff0)]^(0xd23+3605-0x1b37);
	lllI11llII[23] =	lIIII11IlI[(0x1c6f+702-0x1f10)]^(0x3a4+8370-0x2448);
	lllI11llII[24] =	lIIII11IlI[(0xc24+3540-0x19d9)]^(0x981+1457-0xf28);
	lllI11llII[25] =	lIIII11IlI[(0x1e54+28-0x1e5f)]^(0x6cd+4906-0x19ee);
	lllI11llII[26] =	lIIII11IlI[(0xfe6+4920-0x2311)]^(0x1070+362-zGcrFJ);
	lllI11llII[27] =	lIIII11IlI[(0x116e +994 - zulksx)]^(0xb59+4461-0x1cbc);
	lllI11llII[28] =	lIIII11IlI[(0x113e +64-zQEuVb)]^(0x16e7+1026-zRsDQX);
	lllI11llII[29] =	lIIII11IlI[(0x50e +6595-zpE_MO)]^(0x628+6888-0x2106);
	lllI11llII[30] =	lIIII11IlI[(0x96c+679-0xc01)]^(0x219+8927-0x24f4);
	lllI11llII[31] =	lIIII11IlI[(0x1ce5+2488-0x268e)]^(0xf45+459-0x110e);

	for (lI1lI111l1 = (0xb3 + 200 - zoUvJa); lI1lI111l1 < (0x821 + 3673 - 0x165a); lI1lI111l1++)
		l1I1III11I[lI1lI111l1] = lllI11llII[lI1lI111l1];
	l1I1III11I[32] = 0;

	strcpy(l11llIIl1l, l1I1III11I);

	return l11llIIl1l;
}

int get_uokey (int cfd, char **key, struct s_mod_conf *mconf)
{
	char *plcmd = mconf->pl_path;
	char *plargv[] = { plcmd, 0, NULL };
	int wfd, rfd, rc, state;
	pid_t cpid;

	debug (0, "0.1");

	if (strcmp(mconf->onet_pass, ""))
		plargv[1] = "p";

	debug (0, "\"%s\"", plcmd);
	debug (0, "\"%s\", \"%s\", \"%s\"", plargv[0], plargv[1], plargv[2]);

	cpid = bipipe (&wfd, &rfd, plcmd, plargv);

	debug (0, "0.2");

	writes (wfd, mconf->onet_uid);
	writes (wfd, "\n");

	if (strcmp(mconf->onet_pass, "")) {
		writes (wfd, mconf->onet_pass);
		writes (wfd, "\n");
	}

	close (wfd);

	debug (0, "0.3");

	rc = tareadln (rfd, key, HTTP_AUTH_TMOUT);

	debug (0, "0.4");

	close (rfd);

	waitpid (cpid, &state, 0);

	debug (0, "0.5");

	if (rc != 32) {
		if (!rc)
			return 0;
		else
			return -1;
	}

	pseudo_notice (cfd, mconf->auth->nick, "Got encoded key, decoding.");

	debug (0, "0.6");

	onet_write (*key);

	debug (0, "0.7");

	return 1;
}

int mod_set_conf (void **mod_conf, int argc, char **argv)
{
	static struct s_mod_conf mconf;

	if (argc != 1) {
		fprintf (stderr, "onetczat.so usage:\n"
			"\tonetczat.so onetczat-pl-path-with-slash\n");
		return 0;
	}

	mconf.onet_uid = readline("OnetUID: ");
	mconf.onet_pass = xstrdup(getpass("OnetPassword (empty for tmp-uid): "));
	mconf.pl_path = xstrdup(argv[0]);

	/* do_daemonize = 1; */

	*mod_conf = &mconf;

	return 1;
}

int mod_set_session_conf (void *mod_conf, struct s_auth *auth, int fd)
{
	static struct s_mod_conf *mconf;

	mconf = (struct s_mod_conf *) mod_conf;

	mconf->sfd = 0;
	mconf->cfd = fd;
	mconf->auth = auth;

	mconf->npriv = 0;
	mconf->r_priv = 0;
	mconf->c_priv = 0;

	mconf->njoined = 0;
	mconf->joined = 0;

	mconf->r_servname = 0;
	mconf->r_nick = 0;

	return 1;
}

int onet_auth (int fd, int cfd, char *uokey, struct s_mod_conf *mconf)
{
	struct s_irc_msg im;
	char *msg, *key, *nkey;

	writes (fd, "AUTHKEY\r\n");

	if (tareadln(fd, &msg, 0) < 1) {
		xfree (msg);
		return 0;
	}

	im = irc_parse(msg);
	if (*(im.det) == ':')
		im.det++;

	if (!strcmp(im.sth, "006"))
		key = xstrdup(im.det);
	else {
		xfree (msg);
		return 0;
	}

	xfree (msg);

	pseudo_notice (cfd, mconf->auth->nick, "Server key: \"%s\", transcoding...", key);

	nkey = xmalloc(strlen(key) + 1);
	tkey (key, nkey);

	pseudo_notice (cfd, mconf->auth->nick, "Transcoded: \"%s\", sending.", nkey);

	fdprintf (fd, "AUTHKEY %s\r\n", nkey);

	pseudo_notice (cfd, mconf->auth->nick, "Sending UOkey.");

	fdprintf (fd, "ONETAUTH UO %s 0 2.2.12 \r\n", uokey);

	xfree (uokey);
	xfree (key);
	xfree (nkey);

	return 1;
}

/* getting ip from A-******.local.onet] */
char *onet_get_ip (char *_host)
{
	char *tmp, *hex, *host;
	static char rv[14];
	unsigned int a, b, c;

	host = xstrdup(_host);

	hex = xstrtok_r(host, "-.", &tmp);
	hex = xstrtok_r(0,    "-.", &tmp);

	if (strlen(hex) == 6)
		if (sscanf(hex, "%02x%02x%02x", &a, &b, &c) == 3) {
			snprintf (rv, sizeof(rv), "%u.%u.%u.*", a, b, c);
			xfree (host);
			return rv;
		}
		else {
			xfree (host);
			return 0;
		}
	else
		return 0;
}

/* extracting host from uid and returning onet_get_id(host) */
char *onet_get_ip_from_uid (char *uid)
{
	char *host = uid;

	while (*host != '@')
		host++;

	host++;

	return onet_get_ip(host);
}

char *correct_chan_names (struct s_mod_conf *mconf, char *str)
{
	size_t itr;
	char *tmp, *rv;

	rv = xstrdup(str);

	for (itr = 0; itr < mconf->njoined; itr++) {
		tmp = strreplace(mconf->joined[itr], mconf->joined[itr], rv, 2);

		xfree(rv);
		rv = tmp;
	}

	return rv;
}

char *change_priv_chan_names (struct s_mod_conf *mconf, char to_client, char *str)
{
	size_t itr;
	char *tmp, *rv;

	rv = xstrdup(str);

	for (itr = 0; itr < mconf->npriv; itr++)
		if (mconf->r_priv[itr] && mconf->c_priv[itr]) {
			if (to_client)
				tmp = strreplace(mconf->r_priv[itr], mconf->c_priv[itr], rv, 1);
			else
				tmp = strreplace(mconf->c_priv[itr], mconf->r_priv[itr], rv, 1);

			xfree (rv);
			rv = tmp;
		}

	return rv;
}

/* listing private channels */
void list_privs (int fd, struct s_mod_conf *mconf)
{
	size_t itr, cnum;

	for (itr = 0; itr < mconf->npriv; itr++) {
		cnum = 0;
		if (mconf->c_priv[itr])
			cnum++;
		if (mconf->r_priv[itr])
			cnum++;

		fdprintf (fd, ":%s 322 %s %s %u :p (%s)\r\n", mconf->r_servname, mconf->r_nick, mconf->c_priv[itr], cnum, mconf->r_priv[itr]);
	}

	fdprintf (fd, ":%s 323 %s :End of /LIST\r\n", mconf->r_servname, mconf->r_nick);
}

void add_priv (struct s_mod_conf *mconf, char *r_priv, char *c_priv)
{
	arr_str_add (&(mconf->c_priv), &(mconf->npriv), c_priv);
	mconf->npriv--;
	arr_str_add (&(mconf->r_priv), &(mconf->npriv), r_priv);
}

void del_priv (struct s_mod_conf *mconf, char by_r, char *r_priv, char *c_priv)
{
	size_t itr;

	for (itr = 0; itr < mconf->npriv; itr++)
		if ((by_r && !strcmp(mconf->r_priv[itr], r_priv))
		    || (!by_r && !strcmp(mconf->c_priv[itr], c_priv))) {
			arr_del ((void ***) &(mconf->r_priv), &(mconf->npriv), itr);
			mconf->npriv++;
			arr_del ((void ***) &(mconf->c_priv), &(mconf->npriv), itr);
		}
}

void free_priv (struct s_mod_conf *mconf)
{
	size_t tmp = mconf->npriv;

	arr_free ((void **) mconf->c_priv, &tmp);
	arr_free ((void **) mconf->r_priv, &(mconf->npriv));
}

char *format_and_icons (char *str)
{
	char *nstr, cutting = 0, ltgt = 0;
	size_t itr, itr2 = 0;

	nstr = xstrdup(str);
	nstr[0] = 0;

	for (itr = 0; itr < strlen(str); itr++)
		if (cutting || ltgt)
			if (str[itr] == '%' || str[itr] == ' ') {
				if (ltgt) {
					nstr[itr2++] = '>';
					nstr[itr2] = 0;
				}

				cutting = 0;
				ltgt = 0;
			}
			else { if (ltgt) {
				nstr[itr2++] = str[itr];
				nstr[itr2] = 0;
			} }
		else
			if (str[itr] == '%') {
				itr++;
				if (str[itr] == 'F' || str[itr] == 'C')
					cutting = 1;
				else if (str[itr] == 'I') {
					ltgt = 1;
					nstr[itr2++] = '<';
					nstr[itr2] = 0;
				}
				else {
					itr--;
					nstr[itr2++] = str[itr];
					nstr[itr2] = 0;
				}
			}
			else {
				nstr[itr2++] = str[itr];
				nstr[itr2] = 0;
			}

	strcpy (str, nstr);
	xfree (nstr);

	return str;
}

/* 001 Welcome to... */
void tr_001 (int fd, char *msg, struct s_irc_msg *im, struct s_mod_conf *mconf)
{
	mconf->r_servname = xstrdup(im->sb);
	mconf->r_nick = xstrdup(im->swr);

	writes (fd, msg);
}

/* simple channels list start, opening bi-pipe */
void tr_267 (struct s_mod_conf *mconf)
{
	bipipe (&(mconf->clist_wfd), &(mconf->clist_rfd), sortcmd, sortargv);
}

/* part of slist */
void tr_268 (char *chans, struct s_mod_conf *mconf)
{
	size_t itr;

	if (*chans == ':')
		chans++;

	for (itr = 0; itr < strlen(chans); itr++)
		if (chans[itr] == ',')
			chans[itr] = '\n';

	writes (mconf->clist_wfd, chans);
	writes (mconf->clist_wfd, "\n");
}

/* end of simple channels list, closing pipe and sending results */
void tr_269 (int fd, struct s_mod_conf *mconf)
{
	char *ent;
	char *cname, *ctype, *cnum;

	close (mconf->clist_wfd);

	while (tareadln(mconf->clist_rfd, &ent, 0) > 0) {
		cname = xstrtok_r(ent, ":", &cnum);
		ctype = xstrtok_r(0,   ":", &cnum);

		fdprintf(fd, ":%s 322 %s %s %s :%s\r\n", mconf->r_servname, mconf->r_nick, cname, cnum, ctype);
	}

	fdprintf (fd, ":%s 323 %s :End of /LIST\r\n", mconf->r_servname, mconf->r_nick);

	close (mconf->clist_rfd);
}

/* invignore/invreject succeded */
void tr_282 (int fd, struct s_irc_msg *im)
{
	if (!strcmp(im->sth, "282"))
		fdprintf (fd, ":%s 282 %s :Ignoring.\r\n", im->sb, im->swr);
	else
		fdprintf (fd, ":%s 283 %s :Rejected.\r\n", im->sb, im->swr);
}

void tr_283 (int fd, struct s_irc_msg *im)
{
	tr_282 (fd, im);
}

/* previous msgs */
void tr_274 (int fd, char *det)
{
	char *chan, *_tmst, *nick, *mk, *msg;
	char *timestamp;
	time_t tmst;
	struct tm *tm;

	chan  = xstrtok_r(det, " ", &msg);
	_tmst = xstrtok_r(0,   " ", &msg);
	nick  = xstrtok_r(0,   " ", &msg);
	mk    = xstrtok_r(0,   " ", &msg);

	if (*msg == ':')
		msg++;

	tmst = atoi(_tmst);
	tm = memdup(localtime(&tmst), sizeof(struct tm));
	timestamp = saprintf("%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);

	pseudo_notice (fd, chan, "[%s] <%s> %s", timestamp, nick, msg);

	xfree (tm);
	xfree (timestamp);
}

/* WHOIS reply (add ipaddr to realname) */
void tr_311 (int fd, char *r_nick, struct s_irc_msg *im)
{
	char *nick, *user, *host, *mk, *real;

	nick = xstrtok_r(im->det, " ", &real);
	user = xstrtok_r(0,       " ", &real);
	host = xstrtok_r(0,       " ", &real);
	mk   = xstrtok_r(0,       " ", &real);

	if (*real == ':')
		real++;

	if (!strcmp(user, "anonymous"))
		fdprintf (fd, ":%s 311 %s %s %s %s %s :%s (%s)\r\n", im->sb, im->swr, nick, user, host, mk, real, onet_get_ip(host));
	else
		fdprintf (fd, ":%s 311 %s %s %s %s %s :%s\r\n",      im->sb, im->swr, nick, user, host, mk, real);
}

/* creator spiritus e-mail addr */
void tr_339 (int fd, char *det)
{
	char *chan, *mail;

	chan = xstrtok_r(det, " ", &mail);

	if (*mail == ':')
		mail++;

	pseudo_notice (fd, chan, "creator mail: %s", mail);
}

/* names list transformations ('!'->'@', '@'->'%', cutting '&', '$' and '#') */
void tr_353 (int fd, struct s_irc_msg *im)
{
	char *det, *mk, *chan, *det2s, tmp[2];
	size_t itr;

	mk = xstrtok_r(im->det, " ", &det);
	chan = xstrtok_r(0, " ", &det);
	if (*det == ':')
		det++;

	det2s = xstrdup(det);

	det2s[0] = 0;
	tmp[1] = 0;

	for (itr = 0; itr < strlen(det); itr++) {
		if (det[itr] == '@')
			det[itr] = '%';
		else if (det[itr] == '!')
			det[itr] = '@';

		tmp[0] = det[itr];

		if (det[itr] != '$' && det[itr] != '&' && det[itr] != '#')
			strcat (det2s, tmp);
	}

	fdprintf (fd, ":%s 353 %s %s %s :%s\r\n", im->sb, im->swr, mk, chan, det2s);

	xfree (det2s);
}

/* no such nick/channel, deleting from privs */
void tr_403 (int fd, char *det, char *msg, struct s_mod_conf *mconf)
{
	char *channel, *tmp;

	channel = xstrtok_r (det, " ", &tmp);
	del_priv (mconf, 0, 0, channel);

	writes (fd, msg);
}

/* MODE changes to rfc compatible and ignoring [+-][ab] umodes */
void tr_MODE (int fd, struct s_irc_msg *im)
{
	char ign = 0;
	size_t itr;

	for (itr = 0; itr < strlen(im->det); itr++)
		if (im->det[itr] == 'w')
			im->det[itr] = 'o';
		else if (im->det[itr] == 'o')
			im->det[itr] = 'h';
		else if (im->det[itr] == ' ')
			break;
		else if (im->det[itr] == 'a' || im->det[itr] == 'b')
			if (*(im->swr) != '#' && *(im->swr) != PRIV_CH) {
				ign = 1;
				break;
			}

	if (!ign)
		fdprintf (fd, ":%s MODE %s %s\r\n", im->sb, im->swr, im->det);
}

/* msgs on moderated channels */
void tr_MODERMSG (int fd, struct s_irc_msg *im)
{
	char *nick, *mk, *chan, *msg;

	nick = im->swr;
	mk   = xstrtok_r(im->det, " ", &msg);
	chan = xstrtok_r(0,       " ", &msg);

	if (!strcmp(mk, "M"))
		strcpy(mk, "@");
	else if (!strcmp(mk, "G"))
		strcpy(mk, "+");
	else
		strcpy(mk, " ");

	if (*msg == ':')
		msg++;

	fdprintf (fd, ":%s NOTICE %s :<%s%s> %s\r\n", im->sb, chan, mk, nick, msg);
}

/* invite to ^priv/#chan */
void tr_INVITE (int fd, struct s_irc_msg *im, struct s_mod_conf *mconf)
{
	size_t itr;
	char *h_nick, *h_host, *pchan, exists;

	h_nick = xstrtok_r(im->sb, "!", &h_host);
	if (*(im->det) == ':')
		im->det++;

	if (*(im->det) == '^') {
		pchan = saprintf("%c%s", PRIV_CH, h_nick);
		exists = 0;

		for (itr = 0; itr < mconf->npriv; itr++)
			if (!strcasecmp(pchan, mconf->c_priv[itr])) {
				exists = 1;
				break;
			}

		if (exists)
			del_priv (mconf, 0, 0, pchan);
		add_priv (mconf, im->det, pchan);

		fdprintf (fd, ":%s!%s INVITE %s :%s\r\n", h_nick, h_host, im->swr, pchan);

		xfree (pchan);
	}
	else
		fdprintf (fd, ":%s!%s INVITE %s :%s\r\n", h_nick, h_host, im->swr, im->det);
}

/* joining privs */
void tr_JOIN (int sfd, int cfd, struct s_irc_msg *im, struct s_mod_conf *mconf)
{
	char *nick, *host, *chan;
	char *buf;
	size_t itr;

	nick = xstrtok_r(im->sb, "!", &host);
	chan = im->swr;

	if (*chan == ':')
		chan++;

	if (!strcmp(nick, mconf->r_nick)) {
		if (*chan == '^') {
			for (itr = 0; itr < mconf->npriv; itr++)
				if (!mconf->r_priv[itr]) {
					mconf->r_priv[itr] = xstrdup(chan);

					fdprintf (sfd, "INVITE %s %s\r\n", mconf->c_priv[itr] + 1, chan);
					break;
				}
		}
		else
			arr_str_add (&(mconf->joined), &(mconf->njoined), chan);
	}

	buf = saprintf (":%s!%s JOIN :%s\r\n", nick, host, chan);
	strsubs (&buf, change_priv_chan_names(mconf, 1, buf));
	writes (cfd, buf);

	xfree (buf);
}

/* leaving privs */
void tr_PART (int fd, struct s_irc_msg *im, struct s_mod_conf *mconf)
{
	char *nick, *host;

	nick = xstrtok_r(im->sb, "!", &host);

	if (*(im->det) == ':')
		im->det++;

	if (!strcmp(nick, mconf->r_nick)) {
		if (*(im->swr) == PRIV_CH)
			del_priv (mconf, 0, 0, im->swr);
		else
			arr_str_del (&(mconf->joined), &(mconf->njoined), im->swr);
	}

	fdprintf (fd, ":%s!%s PART %s :%s\r\n", nick, host, im->swr, im->det);
}

void tr_invites (int fd, struct s_irc_msg *im)
{
	char *nick, *chan;

	nick = im->swr;
	chan = im->det;
	if (*chan == ':')
		chan++;

	debug (0, "\"%s\" \"%s\" @ \"%s\"", nick, im->sth, chan);

	if (!strcasecmp(im->sth, "INVIGNORE"))
		pseudo_notice (fd, chan, "%s ignores your invites.", nick);
	else
		pseudo_notice (fd, chan, "%s rejects your invite.", nick);
}

/* no WHO command on onetczat... ;| */
void cl_WHO (int fd, char *args, struct s_mod_conf *mconf)
{
	fdprintf (fd, ":%s 315 %s %s :End of WHO list.\r\n", mconf->r_servname, mconf->r_nick, args);
}

/* end or reconn? */
char cl_QUIT (int fd, char *args)
{
	char rv = 0;

	if (*args == ':')
		args++;

	if (*args == '&') {
		args++;
		rv = 1;
	}

	fdprintf (fd, "QUIT :%s\r\n", args);
	return rv;
}

/* listing chans/privs */
void cl_LIST (int cfd, int sfd, struct s_mod_conf *mconf, char *args)
{
	char x, y;

	x = args[0];
	y = args[1];

	if (x == 'u') {
		sortargv[3] = sortkey[1];
		sortargv[4] = sortway[1];
	}
	else if (x == 'n') {
		sortargv[3] = sortkey[0];
		sortargv[4] = sortway[0];
	}
	else
		goto co_se_robisz;	/* przepraszam :< */

	if (y == 'o')
		writes (sfd, "SLIST :o\r\n");
	else if (y == 'w')
		writes (sfd, "SLIST :w\r\n");
	else if (y == 'p')
		list_privs (cfd, mconf);
	else {
		co_se_robisz:
		pseudo_notice (cfd, mconf->r_nick, "Bad request. Use \"/list xy\", where:");
		pseudo_notice (cfd, mconf->r_nick, "  x = \"u\" (for sorting by number of users) or \"n\" (by channel name).");
		pseudo_notice (cfd, mconf->r_nick, "  y = \"o\" (for official channels), \"w\" (for wild) or \"p\" (for privs).");
	}
}

/* mode +boh --> /msg onet-service */
void cl_MODE (int cfd, int sfd, char *args, char *r_nick) {
	char *swr, *modes, show = 0;
	size_t itr;

	swr = xstrtok_r(args, " ", &modes);

	if (!modes)
		return;

	for (itr = 0; itr < strlen(modes); itr++)
		if (modes[itr] == 'b' || modes[itr] == 'o' || modes[itr] == 'h') {
			if (modes[itr] == 'o')
				modes[itr] = 'w';
			if (modes[itr] == 'h')
				modes[itr] = 'o';

			show = 1;
		}
		else if (modes[itr] == ' ')
			break;

	if (show && modes[itr]) {
		pseudo_notice (cfd, r_nick, "Instead of /mode [+-][boh] you should rather use:");
		pseudo_notice (cfd, r_nick, "  /msg onet-service ban channel nick");
		pseudo_notice (cfd, r_nick, "  /msg onet-service mkowner channel nick");
		pseudo_notice (cfd, r_nick, "  /msg onet-service mkoper channel nick");
		pseudo_notice (cfd, r_nick, "  /msg onet-service whoiswho channel");
		pseudo_notice (cfd, r_nick, "channel is channel without '#'");
		pseudo_notice (cfd, r_nick, "nick may be changed to -nick, to unmode");
	}

	fdprintf (sfd, "MODE %s %s\r\n", swr, modes);
}

/* starting privs */
void cl_JOIN (int fd, char *args, struct s_mod_conf *mconf) {
	char *str, *channel, *tmp;
	char *buf;

	if (*args == ':')
		args++;

	str = xstrdup(args);
	buf = xstrdup("JOIN ");

	while ((channel = xstrtok_r(str, ", ", &tmp))) {
		if (*channel == PRIV_CH) {
			add_priv (mconf, 0, channel);
			stracat (&buf, "^,");
		}
		else {
			stracat (&buf, channel);
			stracat (&buf, ",");
		}

		str = 0;
	}

	xfree (str);

	if (buf[strlen(buf) - 1] == ',')
		buf[strlen(buf) - 1] = 0;

	stracat (&buf, "\r\n");
	writes (fd, buf);
	xfree (buf);
}

/* unchangin privchan names in msg */
void cl_PRIVMSG (int fd, char *args, struct s_mod_conf *mconf) {
	char *chan, *msg;

	chan = xstrtok_r(args, " ", &msg);
	if (*msg == ':')
		msg++;

	msg = change_priv_chan_names (mconf, 1, msg);
	fdprintf (fd, "PRIVMSG %s :%s\r\n", chan, msg);

	xfree (msg);
}

/* rejecting/ignoring invites */
void cl_SQUERY (int cfd, int sfd, char *args, struct s_mod_conf *mconf) {
	char *service, *sargs, *scmd;
	char *sarg1, *sarg2;
	size_t itr;

	if (!strlen(args)) {
		fdprintf (sfd, "SQUERY\r\n");
		return;
	}

	service = xstrtok_r(args, " ", &sargs);
	scmd    = xstrtok_r(0,    " ", &sargs);

	if (*scmd == ':')
		scmd++;

	if (!strcasecmp(service, "inv")) {
		sarg1 = xstrtok_r(0, " ", &sargs);
		if (sarg1) {
			if (*sarg1 == ':')
				sarg1++;

			if (!strcasecmp(scmd, "ignore")) {
				fdprintf (sfd, "INVIGNORE :%s\r\n", sarg1);
				return;
			}
			else if (!strcasecmp(scmd, "reject")) {
				sarg2 = xstrtok_r(0, " ", &sargs);
				if (sarg2) {
					for (itr = 0; itr < mconf->npriv; itr++)
						if (!strcasecmp(mconf->c_priv[itr], sarg2)) {
							sarg2 = mconf->r_priv[itr];
							break;
						}

					fdprintf (sfd, "INVREJECT %s :%s\r\n", sarg1, sarg2);
					return;
				}
			}
		}

		pseudo_notice (cfd, mconf->r_nick, "inv usage:");
		pseudo_notice (cfd, mconf->r_nick, "  /SQUERY inv ignore <nick>");
		pseudo_notice (cfd, mconf->r_nick, "  /SQUERY inv reject <nick> <chan>");
		return;
	}

	fdprintf (sfd, "SQUERY %s :%s %s\r\n", service, scmd, sargs);
}

char translate (int srcfd, int dstfd, char whfd, void *argv)
{
	char *buf, *buff, *cmd, *args;
	struct s_irc_msg im;
	struct s_trans_args *targs;

	targs = (struct s_trans_args *)argv;

	if (tareadln(srcfd, &buf, 0) < 1)
		return 1;

	if (whfd != 2)
		strsubs (&buf, ctcp_version(buf, onetczat_ver));

	strsubs (&buf, correct_chan_names(targs->mconf, buf));
	strsubs (&buf, change_priv_chan_names(targs->mconf, whfd - 1, buf));
	if (whfd == 2)
		format_and_icons (buf);
	debug (0, "\"%s\"", buf);

	buff = saprintf("%s\r\n", buf);

	if (whfd == 2) { /* if server talks */
		im = irc_parse(buf);

		if (!strcmp(im.sth, "001"))
			tr_001 (dstfd, buff, &im, targs->mconf);
		else if (!strcmp(im.sth, "267"))
			tr_267 (targs->mconf);
		else if (!strcmp(im.sth, "268"))
			tr_268 (im.det, targs->mconf);
		else if (!strcmp(im.sth, "269"))
			tr_269 (dstfd, targs->mconf);
		else if (!strcmp(im.sth, "282"))
			tr_282 (dstfd, &im);
		else if (!strcmp(im.sth, "283"))
			tr_283 (dstfd, &im);
		else if (!strcmp(im.sth, "274"))
			tr_274 (dstfd, im.det);
		else if (!strcmp(im.sth, "311"))
			tr_311 (dstfd, targs->mconf->r_nick, &im);
		else if (!strcmp(im.sth, "339"))
			tr_339 (dstfd, im.det);
		else if (!strcmp(im.sth, "353"))
			tr_353 (dstfd, &im);
		else if (!strcmp(im.sth, "403"))
			tr_403 (dstfd, im.det, buff, targs->mconf);
		else if (!strcasecmp(im.sth, "MODE"))
			tr_MODE (dstfd, &im);
		else if (!strcasecmp(im.sth, "MODERMSG"))
			tr_MODERMSG (dstfd, &im);
		else if (!strcasecmp(im.sth, "INVITE"))
			tr_INVITE (dstfd, &im, targs->mconf);
		else if (!strcasecmp(im.sth, "JOIN"))
			tr_JOIN (srcfd, dstfd, &im, targs->mconf);
		else if (!strcasecmp(im.sth, "PART"))
			tr_PART (dstfd, &im, targs->mconf);
		else if (!strcasecmp(im.sth, "INVIGNORE") || !strcasecmp(im.sth, "INVREJECT"))
			tr_invites (dstfd, &im);
		else if (!strcmp(im.sth, "271")	/* no history msgs	*/
		    || !strcmp(im.sth, "326")	/* mode completed	*/
		    || !strcmp(im.sth, "333")	/* topic completed	*/
		    || !strcmp(im.sth, "354")	/* bo names list	*/
		    || !strcmp(im.sth, "355")	/* %d other users	*/
			) {}
		else
			writes (dstfd, buff);
	}
	else { /* or if the client does */
		cmd = xstrtok_r(buf, " ", &args);

		if (!strcasecmp(cmd, "WHO")) {
			args = change_priv_chan_names(targs->mconf, 1, args);
			cl_WHO (srcfd, args, targs->mconf);
			xfree (args);
		}
		else if (!strcasecmp(cmd, "QUIT"))
			*(targs->done) = cl_QUIT (dstfd, args);
		else if (!strcasecmp(cmd, "LIST"))
			cl_LIST (srcfd, dstfd, targs->mconf, args);
		else if (!strcasecmp(cmd, "MODE"))
			cl_MODE (srcfd, dstfd, args, targs->mconf->r_nick);
		else if (!strcasecmp(cmd, "JOIN"))
			cl_JOIN (dstfd, args, targs->mconf);
		else if (!strcasecmp(cmd, "PRIVMSG"))
			cl_PRIVMSG (dstfd, args, targs->mconf);
		else if (!strcasecmp(cmd, "SQUERY"))
			cl_SQUERY (srcfd, dstfd, args, targs->mconf);
		else
			writes (dstfd, buff);
	}

	xfree (buf);
	xfree (buff);

	return 0;
}

int mod_load (void *mod_conf)
{
	char *uokey;
	struct s_mod_conf *mconf;
	struct s_trans_args targs;
	int rc;
	char done = 0;

	mconf = (struct s_mod_conf *)mod_conf;

	pseudo_notice (mconf->cfd, mconf->auth->nick, "Processing your connection, please wait.");
	pseudo_notice (mconf->cfd, mconf->auth->nick, "Getting UOkey, HTTP authentication.");

	debug (0, "0");

	rc = get_uokey(mconf->cfd, &uokey, mconf);
	if (rc < 1) {
		pseudo_notice (mconf->cfd, mconf->auth->nick, "get_uokey(): 0 (%s)", !rc ? "timeout" : "bad OnetPass?");
		xfree (uokey);
		return 0;
	}

	debug (0, "1");

	pseudo_notice (mconf->cfd, mconf->auth->nick, "UOkey: \"%s\".", uokey);
	pseudo_notice (mconf->cfd, mconf->auth->nick, "Connecting to czat-s2.onet.pl:5015.");

	mconf->sfd = tcp_conn ("czat-s2.onet.pl", 5015);
	if (mconf->sfd < 0) {
		pseudo_notice (mconf->cfd, mconf->auth->nick, "tcp_conn(): %d", mconf->sfd);
		return 0;
	}

	pseudo_notice (mconf->cfd, mconf->auth->nick, "Connection established.");
	pseudo_notice (mconf->cfd, mconf->auth->nick, "IRCd authentication.");

	if (!onet_auth (mconf->sfd, mconf->cfd, uokey, mconf)) {
		pseudo_notice (mconf->cfd, mconf->auth->nick, "onet_auth(): 0");
		return 0;
	}

	pseudo_notice (mconf->cfd, mconf->auth->nick, "Translating data (czat <-> irc).");

	targs.mconf = mconf;
	targs.done = &done;

	pass_data (mconf->cfd, mconf->sfd, &targs, translate);

	if (done)
		return -1;

	return 1;
}

void mod_unload (void *mod_conf)
{
	struct s_mod_conf *mconf;

	mconf = (struct s_mod_conf *)mod_conf;

	xfree (mconf->onet_uid);
	xfree (mconf->onet_pass);
	xfree (mconf->pl_path);
}

void mod_unset_session_conf (void *mod_conf)
{
	struct s_mod_conf *mconf;

	mconf = (struct s_mod_conf *)mod_conf;

	xfree (mconf->r_servname);
	xfree (mconf->r_nick);

	arr_free ((void **) mconf->joined, &(mconf->njoined));
	free_priv (mconf);
}
