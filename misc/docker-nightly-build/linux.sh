docker run --rm \
	-v $SOURCEDIR:/source \
	-v $OUTPUTDIR:/nightly \
	-e "TARGET_PLATFORM=LINUX" \
	localghost/ezquake_nightly
