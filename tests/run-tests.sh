#!/usr/bin/env sh

mcc=../../../src/mcc
out_test=out-test

cd cpp

for test in *; do
	cd $test
	printf "%-20s" $test

	if [ ! -f out ]; then
		echo "Missing output file to compare against"
		rm -f $out_test
		cd ..
		continue
	fi

	$mcc 'in' > $out_test 2>/dev/null

	if [ $? != 0 ]; then
		echo "Errors during compilation"
	else
		diff out $out_test #>/dev/null

		if [ $? != 0 ]; then
			echo "Output differs"
		else
			echo "OK"
		fi
	fi

	rm -f $out_test
	cd ..
done
