#!/bin/bash

T42DIR=/daten_server/teletext/in
T42DONE=$T42DIR/done

T42SPLIT=/daten_server/teletext/split


mkdir -p $T42DONE

mkdir -p $T42SPLIT



for x in $T42DIR/*.t42
do
	if [ -f "$x" ]
	then
		bn=`basename "$x" .t42`
		station=`echo $bn | cut -d "." -f 1`
		dt=`echo $bn | cut -d "." -f 2 | cut -d "_" -f 1`
		echo $bn $station $dt

		mkdir -p $T42SPLIT/$station
		src/split_t42_to_pages_ram $T42SPLIT/$station $dt < $x && mv $x $T42DONE
	fi

done


for x in $T42SPLIT/*
do
	src/delete_double $x
done
