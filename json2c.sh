#!/bin/sh
set -eu
case "$#" in
	1)
		exec 0< "$1"
		;;
	2)
		exec 0< "$1"
		exec 1> "$2"
		;;
	*)
		echo "Usage: `basename -- "$0"` INPUT [OUTPUT]" >&2
		exit 64
		;;
esac
od -t x1 -v | awk -v name="`basename -- "$1"`" '
	BEGIN {
		if (name ~ /^[0-9]/) {
			name = "__" name
		}
		gsub(/[^0-9A-Za-z]/, "_", name)
		printf "unsigned char %s[] = {\n", name
	}
	{
		for (i = 2; i <= NF; i++) {
			printf "\t0x%s,\n", $i
			len++
		}
	}
	END {
		printf "};\nunsigned int %s_len = %u;\n", name, len
	}
'
