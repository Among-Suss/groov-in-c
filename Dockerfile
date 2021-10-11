FROM debian:bullseye-slim

ENV DEBIAN_FRONTEND noninteractive
ENV DEBCONF_NONINTERACTIVE_SEEN true

RUN apt-get -y update && apt-get -y upgrade && apt-get -y install make gcc libssl-dev libsodium-dev libopus-dev libogg-dev libcurl4-openssl-dev ffmpeg python3-pip && pip install youtube-dl
RUN youtube-dl --rm-cache-dir

USER root

COPY . /app
WORKDIR /app/src/discord-in-c/

RUN make -B test2

CMD ["./test2"]
