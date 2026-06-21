ARG BASE=docker.io/ubuntu:22.04
ARG CONFIGURE_ARGS=""
ARG FAKERELEASE=false
ARG PROGNAME="armagetronad"
ARG PROGTITLE="Armagetron Advanced"

########################################

# runtime prerequisites
FROM ${BASE} AS runtime_base
LABEL maintainer="Manuel Moos <z-man@users.sf.net>"

ARG PROGNAME

RUN apt-get -y update && DEBIAN_FRONTEND=noninteractive apt-get install \
bash \
libboost-thread1.74.0 \
libxml2 \
libprotobuf23 \
runit \
-y

# use libprotobuf32 from ubuntu 23.04 onward

WORKDIR /

########################################

# development prerequisites
FROM runtime_base AS builder

# build dependencies
RUN apt-get -y update && DEBIAN_FRONTEND=noninteractive apt-get install \
autoconf \
automake \
libboost-all-dev \
patch \
bash \
bison \
bzip2 \
g++ \
make \
libtool \
libxml2-dev \
protobuf-compiler \
pkg-config \
python3 \
wget \
-y

########################################

# build
FROM builder as build

ARG CONFIGURE_ARGS
ARG FAKERELEASE
ARG PROGNAME
ARG PROGTITLE

ENV SOURCE_DIR /root/${PROGNAME}
ENV BUILD_DIR /root/build

COPY . ${SOURCE_DIR}
WORKDIR ${SOURCE_DIR}

RUN (test -r configure && test -f missing) || (./bootstrap.sh && cat version.m4)

RUN mkdir -p ${BUILD_DIR} && chmod 755 ${BUILD_DIR}
WORKDIR ${BUILD_DIR}
RUN ARMAGETRONAD_FAKERELEASE=${FAKERELEASE} progname="${PROGNAME}" progtitle="${PROGTITLE}" \
${SOURCE_DIR}/configure --prefix=/usr/local --disable-glout --disable-sysinstall --disable-useradd \
    --disable-master --disable-uninstall --disable-desktop \
    ${CONFIGURE_ARGS} && \
make -j `nproc` && \
DESTDIR=/root/destdir make install && \
rm -rf ${SOURCE_DIR} ${BUILD_DIR}

########################################

# finish runtime
FROM runtime_base AS runtime

# pack
FROM runtime AS run_server

ARG PROGNAME

COPY --chown=root --from=build /root/destdir /
COPY batch/docker-entrypoint.sh.in /usr/local/bin/docker-entrypoint.sh

RUN bash /usr/local/share/games/*-dedicated/scripts/sysinstall install /usr/local && \
sed -i /usr/local/bin/docker-entrypoint.sh -e "s/@progname@/${PROGNAME}/g" && \
chmod 755 /usr/local/bin/docker-entrypoint.sh

USER nobody

VOLUME ["/var/${PROGNAME}"]
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
#ENTRYPOINT ["bash"]
EXPOSE 4534/udp
