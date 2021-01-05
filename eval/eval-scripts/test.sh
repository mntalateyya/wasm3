export SLEEP=100000
for trial in {1..5}
do
	>&2 echo "Trial #$trial"
	for n in {40..100..10}
	do
		>&2 echo "fib $n"
		filename=dump-fib-$trial-$n
		build/wasm3 --state $filename --func fib ../wasm-interp-test/fib.wasm $n
		./scp.exp $filename
		rm -f $filename
		# build/wasm3 --state dump --func fib ../wasm-interp-test/fib.wasm $n
	done

	for size in {300..700..100}
	do
		>&2 echo "matmul $size x $size"
		filename=dump-mat-$trial-$size
		build/wasm3 --state $filename --func mymain ../wasm-interp-test/matmul.wasm $size
		./scp.exp $filename
		rm -f $filename 
		# build/wasm3 --state dump --func mymain ../wasm-interp-test/matmul.wasm $size
	done
done
