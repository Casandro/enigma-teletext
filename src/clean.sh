#!/bin/bash

for x in *.c
do
	rm `basename $x .c`
done
