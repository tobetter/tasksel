#!/usr/bin/perl
#
# taskpackages directory
#
# This program spits out a list of all the packages listed in the tasks.
#
# If you go to auric, this command is then useful:
#
# for package in $(cat ~joeyh/tasklist); do
#   madison -s testing -a "i386 all" $package >/dev/null || echo "No $package!"
# done

my $dir=shift or die "no directory specified\n";

use File::Find;
find(\&processfile, $dir);

sub processfile {
	my $file=$_; # File::Find craziness.
	return unless $file =~ /^[-+_.a-z0-9]+$/ and -f $file;
	open (IN, $file) or die "$file: $!";
	my %fields;
	my $field="";
	while (<IN>) {
		chomp;
		next if /^\s*#/;
		s/#.*//;

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

	print join("\n", split(' ', $fields{packages}))."\n";
}
