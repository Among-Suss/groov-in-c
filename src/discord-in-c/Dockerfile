FROM archlinux

RUN pacman -Syy --noconfirm
RUN pacman -S --noconfirm make ffmpeg youtube-dl libsodium opus

COPY . . 

RUN make test2

CMD ["./test2", "ODgyNjY3NzU3NDkyMTQyMTcw.YS-uaA.JbXLB0BAV4e_sMPog3oYaPcpuTQ"]