#!/bin/bash

stdin=stdin
stdout=stdout
stderr=stderr

stdout_test=$stdout-test
stderr_test=$stderr-test

validate_output() {
	if [ ! -f $1 ] || [ ! -f $2 ]; then
		return
	fi

	diff --ignore-space-change <(cat $1 | tr -s '\n' | tr '\n' ' ') <(cat $2 | tr -s '\n' | tr '\n' ' ') >/dev/null

	if [ $? -ne 0 ]; then
		echo -n "  !$1"
		num_errs=$(($num_errs + 1))
	else
		num_ok=$(($num_ok + 1))
	fi
}

test_valgrind() {
	valgrind_result=$(valgrind $prog $stdin 2>&1 >/dev/null | tail -n1 | cut -d ' ' -f4,10)

	if [ "$valgrind_result" != "0 0" ]; then
		num_errs=$(($num_errs + 1))
		echo -n "  !valgrind"
	else
		num_ok=$(($num_ok + 1))
	fi
}

suite=$1

num_ok=0
num_errs=0
prog=$(pwd)/../build/$suite

if [ -z "$suite" ]; then
	echo "$0: usage: $0 SUITE"
	exit 1
fi

cd $suite

num_tests=$(ls | wc -l)
i=0

for test in *; do
	num_errs_orig=$num_errs

	if [ ! -d "$test" ]; then
		continue
	fi

	cd $test
	i=$(($i + 1))
	printf "\r\033[K%02i/%02i %s" $i $num_tests $test

	#printf "%s:\n" $test

	$prog $stdin 2>$stderr_test | grep -v '^$' > $stdout_test
	if [ $? -ne 0 ]; then
		echo -n "  !runtime"
		num_errs=$(($num_errs + 1))
	else
		validate_output $stdout $stdout_test
		validate_output $stderr $stderr_test
	fi
	test_valgrind

	if [ $num_errs_orig -lt $num_errs ]; then
		echo
	fi

	cd ..
done

cd ..

echo -e "\r\033[K\n$suite: $num_ok OK, $num_errs FAILED"
