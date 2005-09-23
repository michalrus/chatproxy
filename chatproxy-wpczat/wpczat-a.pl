#! /usr/bin/perl

use IO::Handle;
use strict;
use warnings;

my $wp_nick = "";
my $img_path = "";

$wp_nick = <STDIN>;
$img_path = <STDIN>;

chomp $wp_nick;
chomp $img_path;

my $tmp = "";
my $cookie = "";
my $cookies = "";

my $pdata = "";
my $plen = 0;

my $wpsticket = "";
my $wpdticket = "";
my $reksticket = "";
my $rekticket = "";
my $statid = "";
my $_wpid = "";
my $_wpaw = "";
my $_wpsesid = "";

my $location = "";
my $obrazek = "";
my $obrazek_v = "";

$tmp = `echo -ne 'GET /chat.html?i=1 HTTP/1.1\r\nHost: czat.wp.pl\r\nUser-Agent: Mozilla\r\n\r\n' | nc czat.wp.pl 80 | grep -E '^Set-Cookie:'`;
while ($tmp =~ s/Set-Cookie:\s+([^\s;]+)//i) {
	$cookie = ${1};

	if	($cookie =~ /wpsticket=(.*)/)	{ $wpsticket = $1; }
	elsif	($cookie =~ /wpdticket=(.*)/)	{ $wpdticket = $1; }
	elsif	($cookie =~ /reksticket=(.*)/)	{ $reksticket = $1; }
	elsif	($cookie =~ /rekticket=(.*)/)	{ $rekticket = $1; }
	elsif	($cookie =~ /statid=(.*)/)	{ $statid = $1; }
	elsif	($cookie =~ /_wpaw=(.*)/)	{ $_wpaw = $1; }
	elsif	($cookie =~ /_wpid=(.*)/)	{ $_wpid = $1; }
	elsif	($cookie =~ /_wpsesid=(.*)/)	{ $_wpsesid = $1; }
}
$cookies = "statid=${statid}; statid=${statid}";
$location = "/chat.html?noframeset=1&i=1";

$tmp = `echo -ne 'GET ${location} HTTP/1.1\r\nHost: czat.wp.pl\r\nUser-Agent: Mozilla\r\nCookie: ${cookies}\r\n\r\n' | nc czat.wp.pl 80 | grep -E '^(Set-Cookie|Location):'`;
while ($tmp =~ s/Set-Cookie:\s+([^\s;]+)//i) {
	$cookie = ${1};

	if	($cookie =~ /wpsticket=(.*)/)	{ $wpsticket = $1; }
	elsif	($cookie =~ /wpdticket=(.*)/)	{ $wpdticket = $1; }
	elsif	($cookie =~ /reksticket=(.*)/)	{ $reksticket = $1; }
	elsif	($cookie =~ /rekticket=(.*)/)	{ $rekticket = $1; }
#	elsif	($cookie =~ /statid=(.*)/)	{ $statid = $1; }
	elsif	($cookie =~ /_wpaw=(.*)/)	{ $_wpaw = $1; }
	elsif	($cookie =~ /_wpid=(.*)/)	{ $_wpid = $1; }
	elsif	($cookie =~ /_wpsesid=(.*)/)	{ $_wpsesid = $1; }
}
$cookies = "wpsticket=${wpsticket}; wpdticket=${wpdticket}; reksticket=${reksticket}; rekticket=${rekticket}; statid=${statid}; statid=${statid}";
$tmp =~ s/Location:\s+([^\s]+)//i;
$location = $1;

$tmp = `echo -ne 'GET ${location} HTTP/1.1\r\nHost: czat.wp.pl\r\nUser-Agent: Mozilla\r\nCookie: ${cookies}\r\n\r\n' | nc czat.wp.pl 80`;
while ($tmp =~ s/Set-Cookie:\s+([^\s;]+)//i) {
	$cookie = ${1};

	if	($cookie =~ /wpsticket=(.*)/)	{ $wpsticket = $1; }
	elsif	($cookie =~ /wpdticket=(.*)/)	{ $wpdticket = $1; }
	elsif	($cookie =~ /reksticket=(.*)/)	{ $reksticket = $1; }
	elsif	($cookie =~ /rekticket=(.*)/)	{ $rekticket = $1; }
#	elsif	($cookie =~ /statid=(.*)/)	{ $statid = $1; }
	elsif	($cookie =~ /_wpaw=(.*)/)	{ $_wpaw = $1; }
	elsif	($cookie =~ /_wpid=(.*)/)	{ $_wpid = $1; }
	elsif	($cookie =~ /_wpsesid=(.*)/)	{ $_wpsesid = $1; }
}
$cookies = "wpdticket=${wpdticket}; rekticket=${rekticket}; statid=${statid}";

if ($tmp =~ s/<img src="(http:\/\/si\.wp\.pl\/obrazek\?[^"]+)"//) {
	$obrazek = $1;
	$tmp = `wget -q '--header=Cookie: ${cookies}' -O '${img_path}' '${obrazek}'`;
	print "ocr\n";
	flush STDOUT;
}
else {
	print "nie-ok\n";
	exit;
}

$obrazek_v = <STDIN>;
chomp $obrazek_v;

$cookies = "wpsticket=${wpsticket}; wpdticket=${wpdticket}; reksticket=${reksticket}; rekticket=${rekticket}; statid=${statid}; statid=${statid}";
$location = "/chat.html?noframeset=1&i=1&auth=nie&nick=${wp_nick}&simg=${obrazek_v}&x=30&y=13";
$tmp = `echo -ne 'GET ${location} HTTP/1.1\r\nHost: czat.wp.pl\r\nUser-Agent: Mozilla\r\nCookie: ${cookies}\r\n\r\n' | nc czat.wp.pl 80 > ../../public_html/x.html`;
$tmp = `cat ../../public_html/x.html`;

if ($tmp =~ /<param\s+name="?magic"?\s+value="?([^">]+)/) {
	print "${1}\n";
}
else {
	print "p00f\n";
}
