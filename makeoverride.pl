#!/usr/bin/perl
#
# makeoverride directory
#
# This program spits out an override file for the tasks.

my $dir=shift or die "no directory specified\n";

my %p;
use File::Find;
find(\&processfile, $dir);

print "$_\tTask\t", (join ", ", sort keys %{$p{$_}}), "\n"
	for sort keys %p;

sub processfile {
	my $file=$_; # File::Find craziness.
	$file eq 'po' && -d $file && ($File::Find::prune = 1);
	return if $File::Find::dir=~/\.(svn|git)/;
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
	
	foreach my $field (qw(key packages-list)) {
		foreach my $package(split(' ', $fields{$field})) {
			$p{$package}->{$fields{task}} = 1;
		}
	}
}
