FROM alpine:latest as builder

ARG KEA_DHCP_VERSION=1.4.0-P3
ARG LOG4_CPLUS_VERSION=1.2.1

RUN apk add --no-cache --virtual .build-deps \
        alpine-sdk \
        bash \
        boost-dev \
        bzip2-dev \
        file \
        libressl-dev \
        postgresql-dev \
        zlib-dev \
        autoconf \
        automake \
        libtool

RUN curl -sL https://sourceforge.net/projects/log4cplus/files/log4cplus-stable/${LOG4_CPLUS_VERSION}/log4cplus-${LOG4_CPLUS_VERSION}.tar.gz | tar -zx -C /tmp && \
    cd /tmp/log4cplus-${LOG4_CPLUS_VERSION} && \
    ./configure && \
    make -s -j$(nproc) && \
    make install
    
RUN curl -sL https://github.com/salanki/kea-subscriberid-reservation/archive/${KEA_DHCP_VERSION}.tar.gz | tar -zx -C /tmp && \
    cd /tmp/kea-subscriberid-reservation-${KEA_DHCP_VERSION} && \
    autoreconf -i && \ 
    ./configure \
        --enable-shell \
         --with-pgsql  && \
    make -s -j$(nproc) && \
    make install-strip

RUN curl -sL https://github.com/salanki/kea-hook-runscript/archive/1.1.0-salanki3.tar.gz | tar -zx -C /tmp && \
    cd /tmp/kea-hook-runscript-1.1.0-salanki3 && \
    make KEA_INCLUDE=/usr/local/include/kea KEA_LIB=/usr/local/lib && \
    mkdir -p "/usr/local/lib/hooks" && \
    install -Dm755 "kea-hook-runscript.so" "/usr/local/lib/hooks/kea-hook-runscript.so" && \
    apk del --purge .build-deps && \
    rm -rf /tmp/*

FROM alpine:latest
LABEL maintainer "mhiro2 <hirotsu.masaaki@gmail.com>"

RUN apk --no-cache add \
        bash \
        boost \
        bzip2 \
        libressl \
        postgresql-client \ 
        zlib \
        mosquitto-clients \
        curl \
        coreutils

COPY --from=builder /usr/local /usr/local/

ENTRYPOINT ["/usr/local/sbin/kea-dhcp4"]
CMD ["-c", "/usr/local/etc/kea/kea-dhcp4.conf"]
