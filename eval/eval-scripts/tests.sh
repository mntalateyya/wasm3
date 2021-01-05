#!/usr/bin/sh
#
# Evaluate performance compared to original wasm3
#

export device="lap-lin";
export iterations=1;

echo "Switching to master Branch"
sleep 3

# git checkout master
cd build
# cmake .. -DCMAKE_BUILD_TYPE=Release
# make
cd ../eval

export grepexp="User time|Maximum resident|Iterations/Sec";

for i in $( seq 1 $iterations )
do
	echo "Iteration $i for master"
	{
		/usr/bin/time -v ../build/wasm3 ../../wasm-interp-test/fib.wasm 43;
		sleep 5;
		/usr/bin/time -v ../build/wasm3 ../../wasm-interp-test/matmul.wasm 1000;
		sleep 5;
		/usr/bin/time -v ../build/wasm3 ../../wasm-interp-test/isort.wasm 40000;
		sleep 5;
		/usr/bin/time -v ../build/wasm3 ../test/benchmark/coremark/coremark-wasi.wasm
	} 2>&1 | 
	grep -E $grepexp > data/master_"$device"_$i;
	sleep 10
done
cd ..

echo "Switching to Research Branch"
sleep 3

git checkout research 
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
cd ../eval

export grepexp="start:|init done:|User time|Maximum resident|reaction time:|serialize-time:|deserial-time:|Iterations/Sec";

export SLEEP=100000000;
export statefile=dummy;
for i in $( seq 1 $iterations )
do
	echo "Iteration $i for research-full"
	{
		/usr/bin/time -v ../build/wasm3 --state $statefile ../../wasm-interp-test/fib.wasm 43;
		sleep 5;
		/usr/bin/time -v ../build/wasm3 --state $statefile ../../wasm-interp-test/matmul.wasm 1000;
		sleep 5;
		/usr/bin/time -v ../build/wasm3 --state $statefile ../../wasm-interp-test/isort.wasm 40000;
		sleep 5;
		/usr/bin/time -v ../build/wasm3 --state $statefile ../test/benchmark/coremark/coremark-wasi.wasm
	} 2>&1 | 
	grep -E $grepexp > data/research_"$device"_full_$i
	sleep 10
done

export SLEEP=20000;
export statefile="$device"_none;
for i in $( seq 1 $iterations )
do
	echo "Iteration $i for research-none"
	{
		/usr/bin/time -v ../build/wasm3 --state "$statefile"_fib_$i ../../wasm-interp-test/fib.wasm 43;
		sleep 5;
		/usr/bin/time -v ../build/wasm3 --state "$statefile"_mat_$i ../../wasm-interp-test/matmul.wasm 1000;
		sleep 5;
		/usr/bin/time -v ../build/wasm3 --state "$statefile"_isort_$i ../../wasm-interp-test/isort.wasm 40000;
	} 2>&1 | 
	grep -E $grepexp > data/research_"$device"_none_$i
	sleep 10
done

export SLEEP=6000000;
export statefile="$device"_half;
for i in $( seq 1 $iterations )
do
	echo "Iteration $i for research-half"
	{
		/usr/bin/time -v ../build/wasm3 --state "$statefile"_fib_$i ../../wasm-interp-test/fib.wasm 43;
		sleep 5;
		/usr/bin/time -v ../build/wasm3 --state "$statefile"_mat_$i ../../wasm-interp-test/matmul.wasm 1000;
		sleep 5;
		/usr/bin/time -v ../build/wasm3 --state "$statefile"_isort_$i ../../wasm-interp-test/isort.wasm 40000;
	} 2>&1 | 
	grep -E $grepexp > data/research_"$device"_half_$i
	sleep 10
done