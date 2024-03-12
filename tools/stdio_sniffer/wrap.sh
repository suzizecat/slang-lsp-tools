#!/bin/bash
# Usage is :
# wrap.sh OUTFILE WRAPPED [ARG1 [ARG2 [...]]]
# Where OUTFILE is the output log file.
# And WRAPPED the software to spy on. 

target_file=$1
shift

echo "Starting new spylog from `date`" > $target_file
echo "Command line is $*" >> $target_file
echo "#########################################################################" >> $target_file
echo "">> $target_file

tee -a "$target_file" | $* | tee -a "$target_file"

echo "#########################################################################" >> $target_file
echo "$1 Finished." >> $target_file
echo "#########################################################################" >> $target_file
