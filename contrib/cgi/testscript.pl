#!/usr/bin/perl -w
use strict;

# A simple CGI-script for testing purposes.
# Written by Paul Rombouts <p.a.rombouts@home.nl>
# Copyright 2002  Paul A. Rombouts
# Modified by Andrew M. Bishop to output HTML.
# May be used under the terms of the GNU Public License.

my @CGIvarnames=('SERVER_SOFTWARE',
		 'SERVER_NAME',
		 'GATEWAY_INTERFACE',
		 'SERVER_PROTOCOL',
		 'SERVER_PORT',
		 'REQUEST_METHOD',
		 'REQUEST_URI',
		 'PATH_INFO',
		 'PATH_TRANSLATED',
		 'SCRIPT_NAME',
		 'QUERY_STRING',
		 'REMOTE_HOST',
		 'REMOTE_ADDR',
		 'AUTH_TYPE',
		 'REMOTE_USER',
		 'REMOTE_IDENT',
		 'CONTENT_TYPE',
		 'CONTENT_LENGTH');

print "Content-Type: text/html\n\n";

print << "EOF"
<html>
<head>
<title>WWWOFFLE Test CGI script</title>
</head>
<body>
<pre>
EOF
;

if(@ARGV) {
    print "my args are: @ARGV\n\n\n";
}
else {
    print "I have no arguments.\n\n\n";
}

print "my CGI environment variables are:\n\n";
foreach my $key (@CGIvarnames) {
    print($key,defined($ENV{$key})?"=$ENV{$key}":" is undefined","\n");
}
print "\n";
foreach my $key (keys %ENV) {
    if($key =~ /^HTTP_/) {
	print "$key=$ENV{$key}\n";
     }
}

if(defined(my $length=$ENV{CONTENT_LENGTH})) {
    print "\n\n";
    my $content;
    my $res=sysread(STDIN,$content,$length);
    if(defined($res)) {
	print("my",($res<$length)?" (incomplete) ":" ","content is:\n\n",$content);
    }
    else {
	print "An error occurred while tring to read my content: $!\n";
    }
}

print << "EOF"
</pre>

<p>
<form action="$ENV{PATH_INFO}" method=get>
<textarea cols="80" rows="4" name="text">
Some text to be sent to the CGI
</textarea>
<p>
<input type="submit" value="Send data using GET method">
</form>

<p>
<form action="$ENV{PATH_INFO}" method=post>
<textarea cols="80" rows="4" name="text">
Some text to be sent to the CGI
</textarea>
<p>
<input type="submit" value="Send data using POST method">
</form>

</body>
</html>
EOF
;

exit 0;
