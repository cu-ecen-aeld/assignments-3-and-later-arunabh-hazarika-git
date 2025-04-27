#!/bin/sh

if [ $# -lt 2 ] ; then
    echo "Usage: finder.sh <filesdir> <searchterm>"
    exit 1
fi

if [ ! -d $1 ] ; then
    echo "Directory $1 not found"
    exit 1
fi

fileCnt=0
lineCnt=0
for fd in `find $1 -type f` ; do
    fileCnt=$((fileCnt + 1))
    c=`grep "$2" "$fd" | wc -l`
    lineCnt=$((lineCnt + c))
done

echo "The number of files are $fileCnt and the number of matching lines are $lineCnt"
