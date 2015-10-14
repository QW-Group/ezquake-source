docker run \
	-v $SOURCEDIR:/source \
	-v $OUTPUTDIR:/nightly \
	-e "FORCEBUILD=TRUE" \
	localghost/ezquake_nightly
