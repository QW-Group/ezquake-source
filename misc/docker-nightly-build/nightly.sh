cd $(/usr/bin/dirname $0)
OUTPUTDIR=$(cat image_outputdir)
docker run -v $OUTPUTDIR:/nightly localghost/ezquake_nightly

