#!/bin/bash
cp ./template.cc ../../scratch/template.cc
FILENAME="./result/task3.txt"
ASSIGNMENTDIREC="assignment1/task3/"

VERBOSE="false"
cd ../../
rm $FILENAME
for RAYLEIGH in true #Only fading is used
do
	for CARA in false true #false true
	do
		echo -e "\nRAYLEIGH: $RAYLEIGH, CARA: $CARA" >> $FILENAME
		for nodeNum in {0..9} #0..9

		do
		 for seed in {1..5} #1..5
		 do
			 echo "Number of Stations: $[nodeNum*5+1], Seed:$seed"
			 ./waf --run "template --verbose=$VERBOSE --seed=$seed --file=$FILENAME --nodeNum=$[nodeNum*5+1] --cara=$CARA --rayleigh=$RAYLEIGH"

		 done
		done
	done
done
echo -e "\n" >> $FILENAME
cp $FILENAME "$ASSIGNMENTDIREC./result.txt"
cd $ASSIGNMENTDIREC
echo ALLDONE
