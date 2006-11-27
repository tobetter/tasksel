#!/usr/bin/perl
#
# listpackages directory|file
#
# This program spits out a list of all the packages listed in the tasks.
#
# If you go to ftp-master, this command is then useful:
#
# for package in $(listpackages); do
#   madison -s testing -a "i386 all" $package >/dev/null || echo "No $package!"
# done
#
# Or to see just key packages:
#
# listpackages tasks key

my $what=shift or die "no directory or file specified\n";
my @toshow=qw{packages-list key};
@toshow=@ARGV if @ARGV;

if (-d $what) {
	use File::Find;
	find(\&processfile, $what);
}
else {
	processfile($what);
}

sub processfile {
	my $file=shift;
	if (! defined $file) {
		$file=$_; # File::Find craziness.
		$file eq 'po' && -d $file && ($File::Find::prune = 1);
		return if $File::Find::dir=~/\.svn/;
		return unless $file =~ /^[-+_.a-z0-9]+$/ and -f $file;
	}
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

	my @list;
	push @list, split(' ', $fields{$_}) foreach @toshow;
	print join("\n", @list)."\n" if @list;
}
