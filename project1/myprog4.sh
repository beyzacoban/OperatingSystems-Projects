#!/bin/bash
# This script moves files based on their size (bigger or smaller than 1 MB)

dir="$1"
# Check for 1 argument
if [ $# -ne 1 ]; then
    echo "Incorrect number of arguments!"
    echo "Usage: $0 <directory_name>"
    exit 1
fi
# Check if directory exists
if [ -z "$dir" ]; then
  echo "Please give a directory name."
  exit 1
fi

mkdir -p "$dir/large_files"
mkdir -p "$dir/small_files"

large_count=0
small_count=0

for file in "$dir"/*; do
  if [ -f "$file" ]; then
    size=$(stat -c%s "$file")

    # 1 MB = 1024 * 1024 bytes
    if [ "$size" -gt $((1024 * 1024)) ]; then
      mv "$file" "$dir/large_files/"
      large_count=$((large_count + 1))
    else
      mv "$file" "$dir/small_files/"
      small_count=$((small_count + 1))
    fi
  fi
done

echo "$large_count files moved to large_files"
echo "$small_count files moved to small_files"

