#!/usr/bin/perl
#
# doincludes directory
#
# Expands #include directives in files in the directory. This is used
# to let task package pull in the contents of metapackages, keeping the
# contents up-to-date, w/o actually pulling in the metapackages themselves,
# since some metapackages are rather prone to breakage near release time.

my $dir=shift or die "no directory specified\n";

my %depends;
{
	local $/="\n\n";
	if (! open (AVAIL, "/var/lib/dpkg/available")) {
		warn "cannot real available file, so disabling lint check\n";
		$dolint=0;
	}
	while (<AVAIL>) {
		my ($package)=/Package:\s*(.*?)\n/;
		my ($depends)=/Depends:\s*(.*?)\n/;
		$depends{$package}=$depends;
	}
	close AVAIL;
}

use File::Find;
find(\&processfile, $dir);

sub processfile {
	my $file=$_; # File::Find craziness.
	return unless -f $file;
	return unless $file =~ /^([-+_.a-z0-9]+)$/;
	my @lines;
	open (IN, $file) or die "$file: $!";
	while (<IN>) {
		if (/#\s*endinclude/) {
			if (! $skipping) {
				die "$file: #endinclude without #include";
			}
			$skipping=0;
		}
		
		push @lines, $_ unless $skipping;

		if (/^#\s*include\s+(\w+)/) {
			my $pkg=$1;
			if ($skipping) {
				die "$file: nested includes near $_\n";
			}
			if (! exists $depends{$pkg}) {
				die "$file: #include $1 failed; no such package";
			}
			push @lines, "#Automatically added by doincludes.pl; do not edit.\n";
			# Split deps and remove alternates and versioned
			# deps. Include the metapackage on the list.
			push @lines, map { s/[|(].*//; "  $_\n" }
			             split(/,\s+/, $depends{$pkg}), $pkg;
			$skipping=1;
		}
	}
	close IN;
	if ($skipping) {
		die "$file: #include without #endinclude";
	}
	
	open (OUT, ">$file") or die "$file: $!";
	print OUT @lines;
	close OUT;
}
