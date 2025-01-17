# Benchmark suite

This directory contains several CNF instances
in the DIMACS format suitable for benchmarking.

## Origin

The files were downloaded from the following sources:
- `ijcai07`: [Ashish Sabharwal homepage](http://www.cs.cornell.edu/~sabhar/software/benchmarks/IJCAI07-suite.tgz)
- `nips2011`: [Stefano Ermon homepage](http://cs.stanford.edu/~ermon/code/nips2011_benchmark.zip)
- `pmc`: [Preprocessing for Propositional Model Counting](http://www.cril.univ-artois.fr/PMC/instancesCompilation.tar.gz)

## Modifications

- Duplicate files were removed. We preferred
  the “original” sources (`ijcai07` and
  `nips2011`) to the derivative (`pmc`) ones.
- In the `nips2011` folder, all 3 subfolders
  (`soft`, `hard`, `soft+hard`) were merged.
  Since many files were listed both in `hard`
  and `soft`, the distinction did not make sense.
- Extension `.dimacs` was renamed to `.cnf`.
  This affects `pmc/circuit/iscas/iscas93`
  and `pmc/circuit/iscas/iscas99` subfolders.

## TXT files

Apart from `.cnf` files, there are several
`.txt` files of the same filename. These
were generated by sharpSAT, commit
`4b43dfd9ee97479cec2f8b9aa1efcc857158d874`,
which is considered a _reference implementation_
for the purpose of testing.

The files were generated with a 15 second
timeout on a Lenovo X220i notebook with
Intel(R) Core(TM) i3-2310M processor.
Consequently, if your modern machine
can solve all such files within 1 minute,
it's likely that no regression is present.

The `.txt` files were generated using the BASH script:
```bash
find test/benchmark/ -name '*.cnf' | while read F; do
  timeout 15 sharpSAT $F > $(dirname $F)/$(basename $F .cnf).txt
done
```
