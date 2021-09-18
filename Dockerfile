FROM archlinux

RUN pacman -Syu --noconfirm
RUN pacman -S --noconfirm gcc make ffmpeg youtube-dl libsodium opus

USER root

COPY . /app
WORKDIR /app/src/discord-in-c/

ARG TOKEN=${TOKEN:-""}
ENV TOKEN=${TOKEN}
ARG BOT_PREFIX=${BOT_PREFIX:-""}
ENV BOT_PREFIX=${BOT_PREFIX}

RUN make test2

CMD ["./test2"]