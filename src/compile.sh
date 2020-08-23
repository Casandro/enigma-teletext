#!/bin/bash

for x in *.c
do
	gcc -Wall  -o `basename $x .c` $x || exit
done
