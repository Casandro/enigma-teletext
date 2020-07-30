#!/bin/bash


#This script should be called regularly, for example ever hour or every few minutes


T42_DIR=/daten_fokker/t42
VUO_DIR=/daten_fokker/vuo

tmpdir=/tmp

#This function takes a transport stream and appends the teletext packets in it to another file
process_ts_to_t42_append ()
{
	IN="$1"
	OUT="$2"

	TMP=`mktemp $tmpdir/XXXXXX.ts`

	cp "$IN" $TMP


	ffprobe $TMP 2> /tmp/temp.ffprobe || continue
	sid=`grep "0x" /tmp/temp.ffprobe | grep "Stream" | grep -v "Video" | grep -v "Audio" | grep -v "dvb_subtitle" | grep "dvb_teletext" | cut -d "[" -f 2 | cut -d "]" -f 1`
	if [ -z "$sid" ]
	then
		touc "$OUT"
	else
		src/ts_to_es $sid < "$TMP" | src/es_to_packets >> "$OUT"
	fi
	rm "$TMP"
}


#This function creates the filename for a teletext and calls process_ts_to_t42_append
convert_recording_of_station ()
{
	TS_FILE="$1"
	TEXTNAME="$2"
	dt=`echo "TS_FILE" | cut -d " " -f 1`
	process_ts_to_t42_append "$TS_FILE" $T42_DIR/$dt-"$TEXTNAME".t42
}


#This function looks for recordings with a certain name. If they are older than 5 minutes, they will be converted
convert_station_to_text ()
{
	STATION="$1"
	TEXTNAME="$2"
	dt=`date -d "5 minutes ago" +%s`
	for x in $VUO_DIR/*" - $STATION - "*
	do
		if [ -f "$x" ]
		then
			changed=`stat  -c %Y "$x"`
			if [ "$changed" -lt "$dt" ]
			then
				convert_recording_of_station "$x" "$TEXTNAME"
			fi
		fi
	done
}

convert_station_to_text "Das Erste" "ard-text"
convert_station_to_text "ZDF" "zdf-text"
