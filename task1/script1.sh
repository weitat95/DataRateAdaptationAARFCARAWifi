#!/bin/bash
cp ./template.cc ../../scratch/template.cc
FILENAME="./result/task1.txt"
ASSIGNMENTDIREC="assignment1/task1/"

VERBOSE="false"
#RAYLEIGH="false"
#CARA="false"
cd ../../
rm $FILENAME
for RAYLEIGH in false true
do
	for CARA in false true
	do
		echo -e "\nRAYLEIGH: $RAYLEIGH, CARA: $CARA" >> $FILENAME
		for dis in {1..20} #1..20

		do
		 for seed in 1337 #1..5
		 do
			 echo "Distance:$[dis*5].0, Seed:$seed"
			 ./waf --run "template --verbose=$VERBOSE --seed=$seed --file=$FILENAME --distance=$[dis*5].0 --cara=$CARA --rayleigh=$RAYLEIGH"
			 #echo "$[dis*5].0"
		 done
		done
	done
done
echo -e "\n" >> $FILENAME
cp $FILENAME "$ASSIGNMENTDIREC./result.txt"
cd $ASSIGNMENTDIREC
echo ALLDONE
