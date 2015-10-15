This method of building ezQuake requires *Docker*. You can read more about it and download it from [here](https://www.docker.com/ "Docker homepage")

Initialization
==============
First, you need to create the docker image. You can do this by running
```shell
docker build -t localghost/ezquake_nightly .
```
or using the [`build_image.sh`](build_image.sh)-script.
This will create a docker image based on Arch Linux that contains the libraries and tools required to compile linux and windows versions of ezQuake.

Now that we have the image ready we can use it to start containers to build ezQuake.


Windows
=======
Nightly
-------
If you run the build from something like crontab, i.e you only want new builds when there are new commits available, then you just need to run the container and it will check for new commits and exit if none are available.
```shell
docker run \
	-v /outputdir:/nightly \
	-v /sourcedir:/source \
	localghost/ezquake_nightly
```
Here we specify what directory the resulting 7zip-archive will be placed into (in this case "/outputdir") and what directory the ezquake-source repository will be cloned into (in this case "/sourcedir")

This can be done using the [`nightly.sh`](nightly.sh)-script. Set the environment variables *OUTPUTDIR* to where you want the files to go, and *SOURCEDIR* to where you want the ezquake source to be kept before calling the script.

One-off builds
--------------
If you want to force a build no matter if there are new commits available or not, then you can set the FORCEBUILD environment-variable like this:
```shell
docker run \
	-v /outputdir:/nightly \
	-v /sourcedir:/source \
	-e "FORCEBUILD=TRUE" \
	localghost/ezquake_nightly
```

This can be done using the [`forcebuild.sh`](forcebuild.sh)-script. Set *OUTPUTDIR* and *SOURCEDIR* as above.

Linux
=====
You can use the same image to build a linux-version of ezQuake. To do this run:
```shell
docker run \
	-v /outputdir:/nightly \
	-v /sourcedir:/source \
	-e "TARGET_PLATFORM=LINUX" \
	localghost/ezquake_nightly
```

This can be done using the [`linux.sh`](linux.sh)-script. *FORCEBUILD* works here too and you need to set *OUTPUTDIR* and *SOURCEDIR* as well.

Please note that the build environment this uses is *Arch Linux*, if you are compiling on another distribution of linux for use on that distribution you are probably better off installing the dependencies yourself and compiling ezQuake without using this method.
