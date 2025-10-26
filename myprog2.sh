#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Incorrect number of arguments!"
    echo "Usage: $0 <directory_name>"
    exit 1
fi

dir=$1

if [ ! -d "$dir" ];
then
  echo "No such directory!"
  exit 1
fi

date=$(date +%F)

mkdir "$dir/backup_$date"

count=0

for file in "$dir"/*.txt "$dir"/*.sh
do
  if [ -f "$file" ];
 then
    cp "$file" "$dir/backup_$date"
    count=$((count+1))
  fi
done

echo "$count files copied into backup_$date"
