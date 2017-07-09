#!/bin/sh

xxd -i $1 $2
sed -i"" -E -e 's/[_]*help/help/g' $2
