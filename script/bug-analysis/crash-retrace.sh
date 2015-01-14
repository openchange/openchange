#!/bin/bash

OPTIND=1
while getopts "vsgf" OPTION ; do # set $o to the next passed option
  case $OPTION in
    v)
       verbose=-v
       ;;
    g)
       out=--gdb
       ;;
    s)
       out=--stdout
       ;;
    f)
       out=--stacktrace-file
       ;;
    ?) echo "Invalid option $OPTION"
       exit 1;
       ;;

    :) echo "Option $OPTION requires an argument"
       exit 1;
       ;;
  esac
done

shift $(($OPTIND - 1))

crashes=$1
if [ -z "$crashes" ]; then
    echo "Usage: $0 [-svgf] crash_dir_or_file"
    exit 1
fi

if [ -z "$out" ]; then
    out=--stdout
fi

APPORT_CRASHDB_CONF=crashdb.conf ./oc-crash-digger -c apport-config -C apport-sandbox-cache -S "$crashes" --oc-cd-conf=crash-digger.conf $verbose $out
