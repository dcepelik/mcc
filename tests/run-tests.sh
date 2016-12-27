#!/bin/bash

prog=../../src/mcc

stdin=stdin
stdout=stdout
stderr=stderr

stdout_test=$stdout-test
stderr_test=$stderr-test

num_ok=0
num_errs=0

validate_output() {
	if [ ! -f $1 ] || [ ! -f $2 ]; then
		return
	fi

	diff <(cat $1 | tr -s '\n' | tr '\n' ' ') <(cat $2 | tr -s '\n' | tr '\n' ' ') >/dev/null

	if [ $? -ne 0 ]; then
		echo "    $1 differs"
		num_errs=$(($num_errs + 1))
	else
		num_ok=$(($num_ok + 1))
	fi
}

test_valgrind() {
	valgrind_result=$(valgrind $prog $stdin 2>&1 >/dev/null | tail -n1 | cut -d ' ' -f4,10)

	if [ "$valgrind_result" != "0 0" ]; then
		num_errs=$(($num_errs + 1))
		echo "    valgrind errors"
	else
		num_ok=$(($num_ok + 1))
	fi
}

for test in *; do
	if [ ! -d "$test" ]; then
		continue
	fi

	cd $test

	printf "%-20s\n" $test

	$prog $stdin 2>$stderr_test | grep -v '^$' > $stdout_test
	if [ $? -ne 0 ]; then
		echo "    runtime error"
		num_errs=$(($num_errs + 1))
	else
		validate_output $stdout $stdout_test
		validate_output $stderr $stderr_test
	fi
	test_valgrind

	#rm -f -- $stdout_test
	#rm -f -- $stderr_test

	cd ..
done

echo
echo "$num_ok OK, $num_errs FAILED"
