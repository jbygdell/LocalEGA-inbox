FROM centos:7.5.1804
LABEL maintainer "EGA System Developers"

EXPOSE 9000
VOLUME /ega/inbox

RUN mkdir -p /var/src && \
    mkdir -p /etc/ega

# Before the EGA PAM lib is loaded
ARG LEGA_GID=1000
RUN groupadd -r -g ${LEGA_GID} lega && \
    useradd -r -g lega lega

#################################################
##
## Upgrade CentOS 7, and install Python 3.6
##
#################################################

RUN yum -y install https://centos7.iuscommunity.org/ius-release.rpm && \
    yum -y install epel-release && \
    yum -y update && \
    yum -y install git gcc make bzip2 \
                   zlib-devel bzip2-devel unzip nss-tools \
		   pam libcurl openssl libuuid readline \
		   pam-devel libcurl-devel openssl-devel libuuid-devel readline-devel

# Adding the DEV packages?
# RUN yum install -y nc nmap tcpdump lsof strace bash-completion bash-completion-extras

#################################################
##
## Install SQLite 3.26
##
#################################################

ARG SQLITE_VERSION=3260000

WORKDIR /var/src
RUN curl -OJ https://sqlite.org/2018/sqlite-autoconf-${SQLITE_VERSION}.tar.gz && \
    tar xzf sqlite-autoconf-${SQLITE_VERSION}.tar.gz && \
    cd sqlite-autoconf-${SQLITE_VERSION} && \
    ./configure && \
    make && make install && \
    echo '/usr/local/lib' >> /etc/ld.so.conf.d/ega.conf && \
    cd && rm -rf sqlite-autoconf-${SQLITE_VERSION}{,.tar.gz}

#################################################
##
## Install jsonc lib
##
#################################################

ARG JSONC_VERSION=json-c-0.13.1-20180305

WORKDIR /var/src
RUN yum install -y automake autoconf libtool && \
    git clone https://github.com/json-c/json-c.git /var/src/json-c && \
    cd /var/src/json-c && \
    git checkout ${JSONC_VERSION}  && \
    sh autogen.sh && \
    ./configure && \
    make && \
    make install
# No need to change dir back

#################################################
##
## Install OpenSSH (and patch it)
##
#################################################

ARG OPENSSH_DIR=/opt/openssh
ARG SSHD_UID=74
ARG SSHD_GID=74

RUN getent group ssh_keys >/dev/null || groupadd -r ssh_keys || :
RUN getent group sshd || groupadd -g ${SSHD_GID} -r sshd

RUN mkdir -p /var/empty/sshd && \
    chmod 700 /var/empty/sshd && \
    useradd -c "Privilege-separated SSH" \
            -u ${SSHD_UID} \
	    -g sshd \
	    -s /sbin/nologin \
	    -r \
	    -d /var/empty/sshd sshd
# /var/empty/sshd must be owned by root and not group or world-writable.

COPY src /var/src/ega
COPY conf/sshd_config /etc/ega/sshd_config

WORKDIR /var/src/ega/openssh
RUN make install

# rsa, dsa and ed25519 keys are created in the entrypoint

#################################################
##
## Install EGA PAM
##
#################################################

WORKDIR /var/src
RUN git clone https://github.com/EGA-archive/EGA-auth.git /root/ega-auth && \
    mkdir -p /usr/local/lib/ega && \
    cd /root/ega-auth/src && \
    make install clean && \
    echo '/usr/local/lib/ega' >> /etc/ld.so.conf.d/ega.conf && \
    cp /etc/nsswitch.conf /etc/nsswitch.conf.bak && \
    sed -i -e 's/^passwd:\(.*\)files/passwd:\1files ega/' /etc/nsswitch.conf && \
    cd && rm -rf /root/ega-auth

RUN ldconfig -v

COPY conf/pam.ega /etc/pam.d/ega-sshd

#################################################
##
## Clean up
##
#################################################
RUN yum clean all && \
    yum erase -y zlib-devel bzip2-devel unzip openssl-devel pam-devel libcurl-devel readline-devel && \
    rm -rf /var/cache/yum
RUN rm -rf /var/src

#################################################
WORKDIR /

COPY conf/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod 755 /usr/local/bin/entrypoint.sh
ENTRYPOINT ["entrypoint.sh"]
