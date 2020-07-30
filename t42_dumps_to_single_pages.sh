#!/bin/bash

T42DIR=/daten_fokker/t42
ZIPDIR=/daten_fokker/zip


dt=`date -d "Yesterday" +%Y%m%d`
echo $dt

for x in $T42DIR/$dt/*.t42
do
	if [ -f "$x" ]
	then
		bn=`basename "$x" .t42`
		t42_date=`echo "$bn" | cut -d "-" -f 1`
		tmp=`mktemp`
		mkdir "$tmp-dir"
		src/split_t42_pages_ram "$tmp-dir/%s" < "$x"
		zip $ZIPDIR/$bn.zip "$tmp-dir"
		rm -r "$tmp-dir"
		rm "$tmp"
	fi

done
