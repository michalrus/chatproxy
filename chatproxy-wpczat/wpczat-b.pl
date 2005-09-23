#! /usr/bin/perl

use strict;
use warnings;

my $wp_user = "";
my $wp_pass = "";
my $wp_nick = "";

$wp_user = <STDIN>;
$wp_pass = <STDIN>;
$wp_nick = <STDIN>;

chomp $wp_user;
chomp $wp_pass;
chomp $wp_nick;

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

$tmp = `echo -ne 'GET /index2.html HTTP/1.1\r\nHost: profil.wp.pl\r\nUser-Agent: Mozilla\r\n\r\n' | nc profil.wp.pl 80 | grep -E '^(Location|Set-Cookie):'`;
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
$cookies = "wpsticket=${wpsticket}; wpdticket=${wpdticket}; reksticket=${reksticket}; rekticket=${rekticket}; statid=${statid}; statid=${statid}";

$tmp =~ s/Location:\s+([^\s]+)//i;
$location = $1;

$tmp = `echo -ne 'GET ${location} HTTP/1.1\r\nHost: profil.wp.pl\r\nUser-Agent: Mozilla\r\nCookie: ${cookies}\r\n\r\n' | nc profil.wp.pl 80 | grep -E '^Set-Cookie:'`;
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
$cookies = "wpsticket=${wpsticket}; wpdticket=${wpdticket}; reksticket=${reksticket}; rekticket=${rekticket}; statid=${statid}; statid=${statid}";

$pdata = "serwis=wp.pl&url=profil.html&tryLogin=1&countTest=1&login_username=${wp_user}&login_password=${wp_pass}&savelogin=2&savessl=2&starapoczta=2&minipoczta=2&zaloguj=Zaloguj";
$plen = length($pdata);

$tmp = `echo -ne 'POST /index2.html HTTP/1.1\r\nHost: profil.wp.pl\r\nUser-Agent: Mozilla\r\nCookie: ${cookies}}\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: ${plen}\r\n\r\n${pdata}' | nc profil.wp.pl 80 | grep -E '^Set-Cookie:'`;
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
$cookies = "wpdticket=${wpdticket}; rekticket=${rekticket}; statid=${statid}; _wpsesid=${_wpsesid}; _wpid=${_wpid}; _wpaw=${_wpaw}";

#$location = "/chat.html?noframeset=1&i=1";
$location = "/chat.html?noframeset=1&auth=tak&i=1&nick=${wp_nick}";
$tmp = `echo -ne 'GET ${location} HTTP/1.1\r\nHost: czat.wp.pl\r\nUser-Agent: Mozilla\r\nCookie: ${cookies}\r\n\r\n' | nc czat.wp.pl 80`;

if ($tmp =~ /<param\s+name="?magic"?\s+value="?([^">]+)/) {
	print "${1}\n";
}
else {
	print "p00f\n";
}
