FROM archlinux

# WORKAROUND for glibc 2.33 and old Docker
# See https://github.com/actions/virtual-environments/issues/2658
# Thanks to https://github.com/lxqt/lxqt-panel/pull/1562
RUN patched_glibc=glibc-linux4-2.33-4-x86_64.pkg.tar.zst && \
    curl -LO "https://repo.archlinuxcn.org/x86_64/$patched_glibc" && \
    bsdtar -C / -xvf "$patched_glibc"

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