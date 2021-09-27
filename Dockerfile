FROM debian:bullseye-20210902-slim

ENV DEBIAN_FRONTEND noninteractive
ENV DEBCONF_NONINTERACTIVE_SEEN true

RUN apt-get -y update && apt-get -y upgrade && apt-get -y install make gcc libssl-dev libsodium-dev libopus-dev libogg-dev ffmpeg python3-pip python3-requests && pip install youtube-dl
RUN youtube-dl --rm-cache-dir

USER root

COPY . /app
WORKDIR /app/src/discord-in-c/

RUN make -B sanitize

CMD ["./test2"]
