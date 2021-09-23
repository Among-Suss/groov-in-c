FROM ubuntu:20.04

ENV DEBIAN_FRONTEND noninteractive
ENV DEBCONF_NONINTERACTIVE_SEEN true

RUN apt-get -y update && apt-get -y upgrade && apt-get -y install make gcc libssl-dev libsodium-dev libopus-dev libogg-dev ffmpeg python3-pip python3-requests && pip install youtube-dl

USER root

COPY . /app
WORKDIR /app/src/discord-in-c/

ARG TOKEN=${TOKEN:-""}
ENV TOKEN=${TOKEN}
ARG BOT_PREFIX=${BOT_PREFIX:-""}
ENV BOT_PREFIX=${BOT_PREFIX}
ARG BOT_NAME=${BOT_NAME:-""}
ENV BOT_NAME=${BOT_NAME}

RUN make -B test2

CMD ["./test2"]
