#!/usr/bin/perl
# Debian task selector, mark II.
# Copyright 2004 by Joey Hess <joeyh@debian.org>.
# Licensed under the GPL, version 2 or higher.
use Locale::gettext;
use Getopt::Long;
use warnings;
use strict;
textdomain('tasksel');

my $debconf_helper="/usr/lib/tasksel/tasksel-debconf";
my $testdir="/usr/lib/tasksel/tests";
my $descdir="/usr/share/tasksel";

sub warning {
	print STDERR "taskel: @_\n";
}

sub error {
	print STDERR "taskel: @_\n";
	exit 1;
}

# A list of all available task desc files.
sub list_task_descs {
	return glob "$descdir/*.desc";
}

# Returns a list of hashes; hash values are arrays for multi-line fields.
sub read_task_desc {
	my $desc=shift;
	my @ret;
	open (DESC, "<$desc") || die "read $desc\: $!";
	local $/="\n\n";
	while (<DESC>) {
		my %data;
		my @lines=split("\n");
		while (@lines) {
			my $line=shift(@lines);
			if ($line=~/^([^ ]+): (.*)/) {
				my ($key, $value)=($1, $2);
				$key=lc($key);
				if (@lines && $lines[0] =~ /^\s+/) {
					# multi-line field
					my @values;
					if (length $value) {
						push @values, $value;
					}
					while (@lines && $lines[0] =~ /^\s+(.*)/) {
						push @values, $1;
						shift @lines;
					}
					$data{$key}=[@values];
				}
				else {
					$data{$key}=$value;
				}
			}
			else {
				print STDERR "parse error on line $.\n";
			}
		}
		if (%data) {
			$data{relevance}=5 unless exists $data{relevance};
			$data{shortdesc}=dgettext("debian-tasks", $data{description}->[0]);
			push @ret, \%data;
		}
	}
	close DESC;
	return @ret;
}

# Returns a list of all available packages.
sub list_avail {
	my @list;
	# Might be better to use the perl apt bindings, but they are not
	# currently in base.
	open (AVAIL, "apt-cache dumpavail|");
	while (<AVAIL>) {
		chomp;
		if (/^Package: (.*)/) {
			push @list, $1;
		}
	}
	close AVAIL;
	return @list;
}

# Given a task hash, checks if its key packages are available.
my %avail_pkgs;
sub task_avail {
	local $_;
	my $task=shift;
	if (! ref $task->{key}) {
		return 1;
	}
	else {
		if (! %avail_pkgs) {
			foreach my $pkg (list_avail()) {
				$avail_pkgs{$pkg} = 1;
			}
		}

		foreach my $pkg (@{$task->{key}}) {
			if (! $avail_pkgs{$pkg}) {
				return 0;
			}
		}
		return 1;
	}
}

# Given a task hash, runs any test program specified in its data, and sets
# the _display and _install fields to 1 or 0 depending on its result.
sub task_test {
	my $task=shift;
	$task->{_display} = 1;
	$task->{_install} = 0;
	foreach my $test (grep /^test-.*/, keys %$task) {
		$test=~s/^test-//;
		if (-x "$testdir/$test") {
			my $ret=system("$testdir/$test", $task->{task}, split " ", $task->{"test-$test"}) >> 8;
			if ($ret == 0) {
				$task->{_display} = 0;
				$task->{_install} = 1;
			}
			elsif ($ret == 1) {
				$task->{_display} = 0;
				$task->{_install} = 0;
			}
			elsif ($ret == 2) {
				$task->{_display} = 1;
				$task->{_install} = 1;
			}
			elsif ($ret == 3) {
				$task->{_display} = 1;
				$task->{_install} = 0;
			}
		}
	}
	return $task;
}

# Converts a list of tasks into a debconf list of their short descriptions.
sub task_to_debconf {
	join ", ", map {
		my $desc=$_->{shortdesc};
		if ($desc=~/, /) {
			warning("task ".$_->{task}." contains a comma in its short description: \"$desc\"");
		}
		$desc;
	} @_;
}

# Given a first parameter that is a debconf list of short descriptions of
# tasks, and then a list of task hashes, returns a list of hashes for all
# the tasks in the debconf list.
sub debconf_to_task {
	my $list=shift;
	my %desc_to_task = map { $_->{shortdesc} => $_ } @_;
	return grep { defined } map { $desc_to_task{$_} } split ", ", $list;
}

# Orders a list of tasks for display.
sub order_for_display {
	sort {
		$b->{relevance} <=> $a->{relevance}
		              || 0 ||
		  $a->{section} cmp $b->{section}
		              || 0 ||
	        $a->{shortdesc} cmp $b->{shortdesc}
	} @_;
}

# Process command line options.
sub getopts {
	my %ret;
	Getopt::Long::Configure ("bundling");
	if (! GetOptions(\%ret, "test|t", "required|r", "important|i", 
		   "standard|s", "no-ui|n", "new-install")) {
		usage();
		exit(1);
	}
	return %ret;
}

sub usage {
	print STDERR gettext(q{Usage:
tasksel install <task>
tasksel [options]; where options is any combination of:
	-t -- test mode; don't really do anything
	-r -- install all required-priority packages
	-i -- install all important-priority packages
	-s -- install all standard-priority packages
	-n -- don't show UI; use with -r or -i usually
	-a -- show all tasks, even those with no packages in them
});
}

my @aptitude_install;
my %options=getopts();
if (@ARGV) {
	if ($ARGV[0] eq "install") {
		shift;
		push @aptitude_install, map { "~t$_" } @ARGV;
	}
	else {
		usage();
		exit 1;
	}
}

my @tasks=map { task_test($_) } grep { task_avail($_) } 
          map { read_task_desc($_) } list_task_descs();

if ($options{"new-install"}) {
	push @aptitude_install, map { "~t".$_->{task} } grep { $_->{_display} == 0 && $_->{_install} == 1 } @tasks;
}
if ($options{"required"}) {
	push @aptitude_install, "~prequired";
}
if ($options{"important"}) {
	push @aptitude_install, "~pimportant";
}
if ($options{"standard"}) {
	push @aptitude_install, "~pstandard";
}

my @list = order_for_display(grep { $_->{_display} == 1 } @tasks);
if (@list && ! $options{"no-ui"}) {
	my $question="tasksel/tasks";
	if ($options{"new-install"}) {
		$question="tasksel/first";
	}
	my @default = grep { $_->{_display} == 1 && $_->{_install} == 1 } @tasks;
	my $tmpfile=`tempfile`;
	chomp $tmpfile;
	system($debconf_helper, $tmpfile, task_to_debconf(@list),
		task_to_debconf(@default), $question);
	open(IN, "<$tmpfile");
	my $ret=<IN>;
	chomp $ret;
	close IN;
	unlink $tmpfile;
	push @aptitude_install, map { "~t".$_->{task} } debconf_to_task($ret, @tasks);
	if (! $options{test} && $ret=~/manual package selection/) {
		# Doing better than this calls for a way to queue stuff for
		# install in aptitude and then enter interactive mode.
		if (@aptitude_install) {
			warning gettext("ignoring other selected packages in favour of manual package selection");
		}
		my $ret=system("aptitude") >> 8;
		if ($ret != 0) {
			error gettext("aptitude failed");
		}
	}
}

if (@aptitude_install) {
	if ($options{test}) {
		print "aptitude -y install ".join(" ", @aptitude_install)."\n";
	}
	else {
		my $ret=system("aptitude", "-y", "install", @aptitude_install) >> 8;
		if ($ret != 0) {
			error gettext("aptitude failed");
		}
	}
}
