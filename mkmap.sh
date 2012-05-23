#!/bin/bash

PAGESIZE=4096

cat <<EOF | tee kmap.h > umap.h
#ifndef _MAP_H
#define _MAP_H

EOF

cat <<EOF > kmap.c
#define __KERNEL__20113__ 1

#include "headers.h"
#include "kmap.h"

EOF

echo -e "extern void (*const PROC_IMAGE_MAP[])(void);\n" >> kmap.h
echo "void (*const PROC_IMAGE_MAP[])(void) = {" >> kmap.c

i=0
while read -a line; do
	echo "ID=${i}: PROG=${line[0]} IMGBASE=${line[1]}"
	if [ $((line[1] % PAGESIZE)) -ne 0 ]; then
		echo "mkmap.sh: error: mapping for ${line[0]} is not on a page boundary." >&2
		exit 1
	fi
	echo "#define ${line[0]} (void(*)(void))${line[1]}" >> kmap.h
	echo "#define ${line[0]}_ID ${i}" | tee -a kmap.h >> umap.h
	echo >> kmap.h
	echo "	/* ${line[0]} */	(void(*)(void))${line[1]}," >> kmap.c
	i=$((i+1))
done

cat <<EOF | tee -a kmap.h >> umap.h
#define PROC_NUM_ENTRY	${i}
#endif
EOF

echo "};" >> kmap.c
