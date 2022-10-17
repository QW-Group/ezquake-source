FROM debian:buster

LABEL authors="Anders Lindmark <anders.lindmark@gmail.com>,Andreas Beisler <eb@subdevice.org>"

RUN apt-get update && \
    apt-get install -y git build-essential mingw-w64 libsdl2-2.0-0 libsdl2-dev libjansson-dev libexpat1-dev libcurl4-openssl-dev libpng-dev libjpeg-dev libspeex-dev libspeexdsp-dev libsndfile-dev p7zip-full && \
    apt-get clean

# Add buildscript
ADD mk_nightly.sh /

# Target volume for the nightly archive
VOLUME /source
VOLUME /nightly

# Default target platform
ENV TARGET_PLATFORM WINDOWS

CMD /mk_nightly.sh
