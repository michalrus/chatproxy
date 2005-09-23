#! /usr/bin/perl

#
#    onetczat.so -- chatproxy module for czat.onet.pl
#    Copyright (C) 2003-2005  Caesar Michael Rus <yodar@router.kom.pl>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#

#
#	onetczat.pl -- perl script to get uokey via http authentication
#

use IO::Handle;
use strict;
use warnings;

my $argc = $#ARGV + 1;
my $nick;
my $pass;

unless ( $argc == 0 || ($argc == 1 && $ARGV[0] eq "p")) {
	die "Usage:\n\t{some_fname.pl} [p]\n\tp -- wait for password from <STDIN>\n";
}

$nick = <STDIN>;
if ($argc) {
	$pass = <STDIN>;
}
else {
	$pass = "";
}

sub printfile {
  my ($file, $data) = @_;
  open FOO, ">>", $file or warn "open $file: $!", return;
  print FOO $data;
  close FOO;
}

sub mktemp {
  my $tmpl = shift;
  my $name = `mktemp $tmpl`;
  if ($? != 0) {
    print $name;
    exit 1;
  }
  chomp $name;
  return $name;
}

chomp $nick;
chomp $pass;

my $tmp;
my $exps;

#
# expire times for non-expiring cookies
#
$exps = `date +%s`;
chomp $exps;
$exps = $exps + (60 * 60 * 24 * 365 * 2);

# unique tmp cookies filename
#
my $cookies_fn = mktemp("/tmp/cookies_XXXXXXXXXX");

#
# unique tmp post-data filename
#
my $post_fn = mktemp("/tmp/post_XXXXXXXXXXX");

#
# unique tmp to-parse filename
#
my $tp_fn = mktemp("/tmp/toparse_XXXXXXXXX");


#
# http logging-in
#
$tmp = `wget -q --save-cookies ${cookies_fn} -O /dev/null "http://kropka.onet.pl/_s/kropka/1?DV=czat%2Flogowanie"`;
printfile($post_fn, "nick=${nick}&pass=${pass}&RE=http%3A%2F%2Fczat.onet.pl%2Fclient%2Findex.html&wracaj=&stary_czat=1&java14=0&java15=0&CH=&P=5015");

$tmp = `wget -S --post-file ${post_fn} --load-cookies ${cookies_fn} --save-cookies ${cookies_fn} -O /dev/null "http://czat.onet.pl/logowanie.html" 2>&1 | grep "Set-Cookie: onet_sid="`;

# processing non-expiring cookie "onet_sid" not saved by wget
chomp $tmp;
$tmp =~ /onet_sid=([^;]+)/;
printfile($cookies_fn, ".onet.pl\tTRUE\t/\tFALSE\t${exps}\tonet_sid\t${1}\n");
# eop

$tmp = `wget -S --post-file ${post_fn} --load-cookies ${cookies_fn} --save-cookies ${cookies_fn} -O ${tp_fn} "http://czat.onet.pl/logowanie.html" 2>&1 | grep "Set-Cookie: onet_uid="`;

# processing non-expiring cookie "onet_uid" not saved by wget
chomp $tmp;
$tmp =~ /onet_uid=([^;]+)/;
printfile($cookies_fn, ".onet.pl\tTRUE\t/\tFALSE\t${exps}\tonet_uid\t${1}\n");
#eop

#
# extracting encoded uokey param
#

sub unescape {
	my($todecode) = @_;
	$todecode =~ tr/+/ /;
	$todecode =~ s/%([0-9a-fA-F]{2})/pack("c",hex($1))/ge;
	return $todecode;
}

my $uokey = "";
my $iter = 0;
my $line = "";

for ($iter = 0; $iter < 32; $iter++) {
        $uokey = "${uokey}%00";
}

`cp ${tp_fn} tp.html`;

open FP, "< ${tp_fn}";

# przestarza³e (;
#
#while ($line = <FP>) {
#	# example:
#        # dk42( unescape( 'some key' ) )
#        if ($line =~ m#dk42[^']*'([^']*)'#) {
#                $uokey = $1;
#                $uokey = unescape($uokey);
#                last;
#        }
#}

while ($line = <FP>) {
	# example:
        # + write( unescape( '1e68%3E%3Cbomk8g%3B%3A%3D9kc9b%3El4m%3B71o' ) ) +

        if ($line =~ m#\+ write\( unescape\( '([^']+)#) {
                $uokey = $1;
                $uokey = unescape($uokey);
                last;
        }
}

close FP;

print "${uokey}\n";
flush STDOUT;

#
# cleaning
#
$tmp = `rm ${cookies_fn} ${post_fn} ${tp_fn}`;
