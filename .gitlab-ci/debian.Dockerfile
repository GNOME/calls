FROM debian:trixie-slim

RUN export DEBIAN_FRONTEND=noninteractive \
   && apt-get -y update \
   && apt-get -y install --no-install-recommends wget ca-certificates gnupg eatmydata \
   && eatmydata apt-get -y update \
   && cd /home/user/app \
   && eatmydata apt-get --no-install-recommends -y build-dep . \
   && eatmydata apt-get --no-install-recommends -y install build-essential git wget gcovr \
   && eatmydata apt-get --no-install-recommends -y install intltool \
   && eatmydata apt-get --no-install-recommends -y install appstream desktop-file-utils \
   && eatmydata apt-get clean
