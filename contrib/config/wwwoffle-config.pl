#!/usr/bin/perl
# Copyright (c) Christian Zagrodnick 1999
# Distributed unter the terms of GPL.
# 
# 2nd Version
# 
# With wwwoffle-config you can have different configurations for wwwoffle
# in one single file. 
#
# There are two ways of configuring now: `#//+' and `#//-'
# With #//+ the leading `#' in the NEXT line will be removed.
# With #//- the next line will be preceeded with a `#'
#
#
# You can call wwwoffle-config from your /etc/ip-up
# with the current interface as I did (wwwoffle-config $INTERFACE):
#
# 
#    #//+ippp0
#    # default=de-relay.boerde.de:8080
#
#    #//+ippp0
#    # *://de-relay.boerde.de/=none
#
#    #//+ippp2
#    # default=noe
#
# Or the other way (line will be removed if ippp2 is the interface): 
#
#   #//-ippp2
#   default=de-relay.boerde.de:8080
# 
# 
#
#   
# 

# The `main' config-file - where to read from
$INFILE="/etc/wwwoffle/wwwoffle.conf.main";

# The `real' config-file, which wwwoffle reads
$OUTFILE="/etc/wwwoffle/wwwoffle.conf";

# Howto call `wwwoffle -config' - if you don't want it to be called 
# prefix the line with a `#'
$CALL="/usr/local/bin/wwwoffle -config";


open(INPUT, $INFILE) || die "Cannot open inputfile ($INFILE)";
open(OUTPUT, ">$OUTFILE") || die "Cannot open outputfile ($OUTFILE)";

if (defined $ARGV[0]) {
  $provider=$ARGV[0];
} else {
  $provider="-"
}



while (<INPUT>) {
  if (defined($strip)) {
    s/^\#//;
    undef($strip);
  }
  print OUTPUT;
  if (/^\#\/\/-$provider$/) {
    print OUTPUT "#";
  }

  if (/^\#\/\/\+$provider$/) {
    $strip=1;
  }
}


if (defined $CALL) {
  print qx{ $CALL };
}
