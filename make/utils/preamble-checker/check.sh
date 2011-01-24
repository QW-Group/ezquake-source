#!/bin/bash

DIR="../../.."

for cfile in "$DIR"/*.?
do
	LINES=`cat $cfile | grep "is free software; you can redistribute it" | wc -l`
	if [ $LINES -ne 1 ]
	then
		echo $cfile : $LINES
	fi
done

# TODO check for things like 	$Id: hud_editor.h,v 1.15 2007-09-17 11:12:29 cokeman1982 Exp $
