FROM phusion/baseimage

WORKDIR /home/app

COPY ./laines ./laines
COPY ./easylogging.conf ./easylogging.conf
COPY ./lib/lib ./lib

EXPOSE 50051

CMD ./laines
