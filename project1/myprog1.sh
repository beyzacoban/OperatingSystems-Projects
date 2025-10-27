#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Incorrect number of arguments!"
    echo "Usage: $0 <filename>"
    exit 1
fi

if [ ! -f "$1" ];
then
  echo "no file!"
  exit 1
fi

#take filename prints top 3 most frequent words (case insensitive) not important punctuation marks
grep -oE '[[:alnum:]]+' "$1" | tr '[:upper:]' '[:lower:]' | sort | uniq -c | sort -nr | head -3

