#!/bin/sh

#rm out_w
#rm out_o

#for count in $(seq 1000 1000 100000); do
#	for t in w o; do
#		for i in $(seq 1 1000); do
#			echo $count $t $i
#			./test $i $count $t 2>&1 | grep RESULT | awk '{print $2 " " $7+$10}' >> out_$t
#		done
#	done
#done

for count in $(seq 1000 1000 100000); do
	for t in onore; do
		for i in $(seq 1 1000); do
			echo $count $t $i
			./test $i $count $t 2>&1 | grep RESULT | awk '{print $2 " " $7}' >> out_$t
		done
	done
done

