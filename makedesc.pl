#!/usr/bin/perl
#
# makedesc directory file
#
# Scan the directory for files, and use the files to generate a task
# description file. The format of the task description file is described in
# tata.c. The format of the input files is:
#
# Task: desktop
# Section: user
# Description: Provide a basic GUI system
#  This task provides functionality for a basic desktop; whether Gnome
#   based, KDE based or customised. With this task, your system will boot
#   into a graphical login screen, at which point you can choose which of
#   these desktops you wish to use on a per-user basis. You can further
#   customise your desktop once installed.
# Packages:
#  kdebase
#  gdm
#  ...
#
# Hash-comments are allowed in the files, but must be on their own lines.

my $dir=shift or die "no directory specified\n";
my $file=shift or die "no file specified\n";

open (OUT, ">$file") or die ">$file: $!";

use File::Find;
find(\&processfile, $dir);

sub processfile {
	return unless /^[-_.a-z0-9]+$/ and -f $_;
	open (IN, $_) or die "$_: $!";
	my %fields;
	my $field="";
	while (<IN>) {
		chomp;
		next if /^\s*#/;

		if (/^\s/) {
			$fields{$field}.="\n$_";
		}
		else {
			($field, my $value)=split(/:\s*/, $_, 2);
			$field=lc($field);
			$fields{$field}=$value;
		}
	}
	close IN;

	print OUT map { ucfirst($_).": ".$fields{$_}."\n" }
		qw{task section description};
	print OUT "\n";
}

close OUT;
