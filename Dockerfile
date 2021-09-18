FROM archlinux

RUN pacman -Syy --noconfirm
RUN pacman -S --noconfirm make ffmpeg youtube-dl libsodium opus
RUN pacman -S --noconfirm gcc

USER root

COPY . /app
WORKDIR /app/src/discord-in-c/

ARG TOKEN=${TOKEN:-""}
ENV TOKEN=${TOKEN}
ARG BOT_PREFIX=${BOT_PREFIX:-""}
ENV BOT_PREFIX=${BOT_PREFIX}

RUN make test2

CMD ["./test2"]