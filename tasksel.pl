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
my $packagesdir="/usr/lib/tasksel/packages";
my $descdir="/usr/share/tasksel";
my $localdescdir="/usr/local/share/tasksel";
my $statusfile="/var/lib/dpkg/status";

sub warning {
	print STDERR "tasksel: @_\n";
}

sub error {
	print STDERR "tasksel: @_\n";
	exit 1;
}

# A list of all available task desc files.
sub list_task_descs {
	return glob("$descdir/*.desc"), glob("$localdescdir/*.desc");
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
			if ($line=~/^([^ ]+):(?: (.*))?/) {
				my ($key, $value)=($1, $2);
				$key=lc($key);
				if (@lines && $lines[0] =~ /^\s+/) {
					# multi-line field
					my @values;
					if (defined $value && length $value) {
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
				print STDERR "parse error in stanza $. of $desc\n";
			}
		}
		if (%data) {
			$data{relevance}=5 unless exists $data{relevance};
			$data{shortdesc}=$data{description}->[0];
			$data{shortdesctrans}=dgettext("debian-tasks", $data{shortdesc});
			push @ret, \%data;
		}
	}
	close DESC;
	return @ret;
}

# Loads info for all tasks, and returns a set of task structures.
sub all_tasks {
	my %seen;
	grep { $seen{$_->{task}}++; $seen{$_->{task}} < 2 }
	map { read_task_desc($_) } list_task_descs();
}

# Returns a list of all available packages.
sub list_avail {
	my @list;
	# Might be better to use the perl apt bindings, but they are not
	# currently in base.
	open (AVAIL, "apt-cache dumpavail|");
	local $_;
	while (<AVAIL>) {
		chomp;
		if (/^Package: (.*)/) {
			push @list, $1;
		}
	}
	close AVAIL;
	return @list;
}

# Returns a list of all installed packages.
sub list_installed {
	my @list;
	local $/="\n\n";
	open (STATUS, $statusfile);
	local $_;
	while (<STATUS>) {
		if (/^Status: .* installed$/m && /Package: (.*)$/m) {
			push @list, $1;
		}
	}
	close STATUS;
	return @list;
}

my %avail_pkgs;
# Given a package name, checks to see if it's available. Memoised.
sub package_avail {
	my $package=shift;
	
	if (! %avail_pkgs) {
		foreach my $pkg (list_avail()) {
			$avail_pkgs{$pkg} = 1;
		}
	}

	return $avail_pkgs{$package};
}

my %installed_pkgs;
# Given a package name, checks to see if it's installed. Memoised.
sub package_installed {
	my $package=shift;
	
	if (! %installed_pkgs) {
		foreach my $pkg (list_installed()) {
			$installed_pkgs{$pkg} = 1;
		}
	}

	return $installed_pkgs{$package};
}

# Given a task hash, checks if its key packages are available.
sub task_avail {
	local $_;
	my $task=shift;
	if (! ref $task->{key}) {
		return 1;
	}
	else {
		foreach my $pkg (@{$task->{key}}) {
			if (! package_avail($pkg)) {
				return 0;
			}
		}
		return 1;
	}
}

# Given a task hash, checks to see if it is already installed.
# (All of its key packages must be installed.)
sub task_installed {
	local $_;
	my $task=shift;
	if (! ref $task->{key}) {
		return 0; # can't tell with no key packages
	}
	else {
		foreach my $pkg (@{$task->{key}}) {
			if (! package_installed($pkg)) {
				return 0;
			}
		}
		return 1;
	}
}

# Given task hash, returns a list of all available packages in the task.
# If the aptitude_tasks parameter is true, then it does not expand tasks
# that aptitude knows about, and just returns aptitude task syntax for
# those.
sub task_packages {
	my $task=shift;
	my $aptitude_tasks=shift;
	
	my @list;
	if ($task->{packages} eq 'task-fields') {
		# task-fields method one is built-in for speed.
		if ($aptitude_tasks) {
			return '~t^'.$task->{task}.'$';
		}
		else {
			local $/="\n\n";
			open (AVAIL, "apt-cache dumpavail|");
			while (<AVAIL>) {
				if (/^Task: (.*)/m) {
					my @tasks=split(", ", $1);
					if (grep { $_ eq $task->{task} } @tasks) { 
						push @list, $1 if /^Package: (.*)/m;
					}
				}
			}
			close AVAIL;
		}
	}
	else {
		# external method
		@list=grep { package_avail($_) } split("\n", `$packagesdir/$task->{packages} $task->{task}`);
	}
	return @list;
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

# Hides a task and marks it not to be installed if it depends on other
# tasks.
sub hide_dependent_tasks {
	my $task=shift;
	if (exists $task->{depends} && length $task->{depends}) {
		$task->{_display} = 0;
		$task->{_install} = 0;
	}
	return $task;
}

# Converts a list of tasks into a debconf list of their short descriptions.
sub task_to_debconf {
	my $field = shift;
	join ", ", map {
		my $desc=$_->{$field};
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

# Given a set of tasks and a name, returns the one with that name.
sub name_to_task {
	my $name=shift;
	return (grep { $_->{task} eq $name } @_)[0];
}

sub usage {
	print STDERR gettext(q{Usage:
tasksel install <task>
tasksel remove <task>
tasksel [options]; where options is any combination of:
	-t, --test          test mode; don't really do anything
	-r, --required      install all required-priority packages
	-i, --important     install all important-priority packages
	-s, --standard      install all standard-priority packages
	-n, --no-ui         don't show UI; use with -r or -i usually
	    --new-install   automatically install some tasks
	    --list-tasks    list tasks that would be displayed and exit
	    --task-packages list available packages in a task
	    --task-desc     returns the description of a task
});
}

# Process command line options and return them in a hash.
sub getopts {
	my %ret;
	Getopt::Long::Configure ("bundling");
	if (! GetOptions(\%ret, "test|t", "required|r", "important|i", 
		   "standard|s", "no-ui|n", "new-install", "list-tasks",
		   "task-packages=s@", "task-desc=s")) {
		usage();
		exit(1);
	}
	# Special case apt-like syntax.
	if (@ARGV && $ARGV[0] eq "install") {
		shift @ARGV;
		$ret{install} = shift @ARGV;
	}
	if (@ARGV && $ARGV[0] eq "remove") {
		shift @ARGV;
		$ret{remove} = shift @ARGV;
	}
	if (@ARGV) {
		usage();
		exit 1;
	}
	return %ret;
}

sub main {
	my %options=getopts();

	# Options that output stuff and don't need a full processed list of tasks.
	if (exists $options{"task-packages"}) {
		my @tasks=all_tasks();
		foreach my $taskname (@{$options{"task-packages"}}) {
			my $task=name_to_task($taskname, @tasks);
			if ($task) {
				print "$_\n" foreach task_packages($task);
			}
		}
		exit(0);
	}
	elsif ($options{"task-desc"}) {
		my $task=name_to_task($options{"task-desc"}, all_tasks());
		if ($task) {
			my $extdesc=join(" ", @{$task->{description}}[1..$#{$task->{description}}]);
			print dgettext("debian-tasks", $extdesc)."\n";
			exit(0);
		}
		else {
			exit(1);
		}
	}

	# This is relatively expensive, get the full list of available tasks and
	# mark them.
	my @tasks=map { hide_dependent_tasks($_) } map { task_test($_) }
	          grep { task_avail($_) } all_tasks();
	
	if ($options{"list-tasks"}) {
		map { $_->{_installed} = task_installed($_) } @tasks;
		print "".($_->{_installed} ? "i" : "u")." ".$_->{task}."\t".$_->{shortdesc}."\n"
			foreach order_for_display(grep { $_->{_display} } @tasks);
		exit(0);
	}
	
	# Now work out what to tell aptitude to install.
	my @aptitude_install;
	if (! $options{"new-install"}) {
		# Don't install hidden tasks if this is not a new install.
		map { $_->{_install} = 0 } grep { $_->{_display} == 0 } @tasks;
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
	if ($options{"install"}) {
		my $task=name_to_task($options{"install"}, @tasks);
		$task->{_install} = 1 if $task;
	}
	my @aptitude_remove;
	if ($options{"remove"}) {
		my $task=name_to_task($options{"remove"}, @tasks);
		push @aptitude_remove, task_packages($task, 0);
	}
	
	# The interactive bit.
	my $interactive=0;
	my $manual_selection=0;
	my @list = order_for_display(grep { $_->{_display} == 1 } @tasks);
	if (@list && ! $options{"no-ui"} && ! $options{install} && ! $options{remove}) {
		$interactive=1;
		if (! $options{"new-install"}) {
			# Find tasks that are already installed.
			map { $_->{_installed} = task_installed($_) } @list;
			# Don't install new tasks unless manually selected.
			map { $_->{_install} = 0 } @list;
		}
		else {
			# Assume that no tasks are installed, to ensure
			# that complete tasks get installed on new
			# installs.
			map { $_->{_installed} = 0 } @list;
		}
		my $question="tasksel/tasks";
		if ($options{"new-install"}) {
			$question="tasksel/first";
		}
		my @default = grep { $_->{_display} == 1 && ($_->{_install} == 1 || $_->{_installed} == 1) } @tasks;
		my $tmpfile=`tempfile`;
		chomp $tmpfile;
		system($debconf_helper, $tmpfile,
			task_to_debconf("shortdesc", @list),
			task_to_debconf("shortdesctrans", @list),
			task_to_debconf("shortdesc", @default),
			$question);
		open(IN, "<$tmpfile");
		my $ret=<IN>;
		chomp $ret;
		close IN;
		unlink $tmpfile;
		if ($ret=~/manual package selection/) {
			$manual_selection=1;
		}
		
		# Set _install flags based on user selection.
		map { $_->{_install} = 0 } @list;
		foreach my $task (debconf_to_task($ret, @tasks)) {
			if (! $task->{_installed}) {
				$task->{_install} = 1;
			}
			$task->{_selected} = 1;
		}
		foreach my $task (@list) {
			if (! $task->{_selected} && $task->{_installed}) {
				push @aptitude_remove, task_packages($task, 0);
			}
		}
	}

	# Mark dependnent packages for install if their dependencies are met.
	foreach my $task (@tasks) {
		if (! $task->{_install} && exists $task->{depends} && length $task->{depends} ) {
			$task->{_install} = 1;
			foreach my $dep (split(', ', $task->{depends})) {
				if (! grep { $_->{task} eq $dep && $_->{_install} } @tasks) {
					$task->{_install} = 0;
				}
			}
		}
	}

	# Add tasks to install.
	my @to_install=map { task_packages($_, 1) } grep { $_->{_install} } @tasks;
	push @aptitude_install, @to_install;
	
	# Clear screen before running aptitude.
	if ($interactive && (@aptitude_remove || @aptitude_install) && ! $options{test}) {
		system("clear");
	}

	# Remove any packages we were asked to.
	if (@aptitude_remove) {
		if ($options{test}) {
			print "aptitude remove ".join(" ", @aptitude_remove)."\n";
		}
		else {
			my $ret=system("aptitude", "remove", @aptitude_remove) >> 8;
			if ($ret != 0) {
				error gettext("aptitude failed")." ($ret)";
			}
		}
	}
	
	# And finally, act on selected tasks.
	if (@aptitude_install || $manual_selection) {
		# If the user selected no tasks and manual package
		# selection, run aptitude w/o the --visual-preview parameter.
		if (! @to_install && $manual_selection) {
			if ($options{test}) {
				print "aptitude\n";
			}
			else {
				my $ret=system("aptitude") >> 8;
				if ($ret != 0) {
					error gettext("aptitude failed")." ($ret)";
				}
			}
		}
		else {
			# Manaul selection and task installs, as best
			# aptitude can do it currently.
			if ($manual_selection) {
				unshift @aptitude_install, "--visual-preview";
			}
			
			if ($options{test}) {
				print "aptitude --without-recommends -y install ".join(" ", @aptitude_install)."\n";
			}
			else {
				my $ret=system("aptitude", "--without-recommends", "-y", "install", @aptitude_install) >> 8;
				if ($ret != 0) {
					error gettext("aptitude failed");
				}
			}
		}
	}
}

main();
