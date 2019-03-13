#!/bin/bash

set -e

# Some env must be defined
[[ -z "${CEGA_ENDPOINT}" ]] && echo 'Environment CEGA_ENDPOINT is empty' 1>&2 && exit 1
[[ ! -z "${CEGA_USERNAME}" && ! -z "${CEGA_PASSWORD}" ]] && CEGA_ENDPOINT_CREDS="${CEGA_USERNAME}:${CEGA_PASSWORD}"
[[ -z "${CEGA_ENDPOINT_CREDS}" ]] && echo 'Environment CEGA_ENDPOINT_CREDS is empty' 1>&2 && exit 1
# Check if set
[[ -z "${CEGA_ENDPOINT_JSON_PREFIX+x}" ]] && echo 'Environment CEGA_ENDPOINT_JSON_PREFIX must be set' 1>&2 && exit 1

# Broker connection settings
[[ -z "${CEGA_MQ_SSL}" ]] && echo 'Environment CEGA_MQ_SSL is empty' 1>&2 && exit 1
[[ -z "${CEGA_MQ_HOST}" ]] && echo 'Environment CEGA_MQ_HOST is empty' 1>&2 && exit 1
[[ -z "${CEGA_MQ_PORT}" ]] && echo 'Environment CEGA_MQ_PORT is empty' 1>&2 && exit 1
[[ -z "${CEGA_MQ_VHOST}" ]] && echo 'Environment CEGA_MQ_VHOST is empty' 1>&2 && exit 1
[[ -z "${CEGA_MQ_USER}" ]] && echo 'Environment CEGA_MQ_USER is empty' 1>&2 && exit 1
[[ -z "${CEGA_MQ_PASSWORD}" ]] && echo 'Environment CEGA_MQ_PASSWORD is empty' 1>&2 && exit 1

EGA_GID=$(getent group lega | awk -F: '{ print $3 }')

cat > /etc/ega/auth.conf <<EOF
##################
# Central EGA
##################

cega_endpoint_username = ${CEGA_ENDPOINT%/}/%s?idType=username
cega_endpoint_uid = ${CEGA_ENDPOINT%/}/%u?idType=uid
cega_creds = ${CEGA_ENDPOINT_CREDS}
cega_json_prefix = ${CEGA_ENDPOINT_JSON_PREFIX}

##################
# NSS & PAM
##################
#prompt = Knock Knock:
#ega_shell = /bin/bash
#ega_uid_shift = 10000

ega_gid = ${EGA_GID}
chroot_sessions = yes
db_path = /run/ega.db
ega_dir = /ega/inbox
ega_dir_attrs = 2750 # rwxr-s---
#ega_dir_umask = 027 # world-denied
EOF

cat > /etc/ega/mq.conf <<EOF
##################
# Broker
##################

# of the form amqp(s)://user:password@host:port/vhost
# connection = ${CEGA_MQ_CONNECTION}

# or per values
enable_ssl = ${CEGA_MQ_SSL}
host = ${CEGA_MQ_HOST}
port = ${CEGA_MQ_PORT}
vhost = ${CEGA_MQ_VHOST}
username = ${CEGA_MQ_USER}
password = ${CEGA_MQ_PASSWORD}


connection_attempts = 10
retry_delay = 10
# in seconds

heartbeat = 0

# Where to send the notifications
exchange = lega
routing_key = files.inbox
EOF

# Changing permissions
echo "Changing permissions for /ega/inbox"
chgrp lega /ega/inbox
chmod 750 /ega/inbox
chmod g+s /ega/inbox # setgid bit

echo 'Welcome to Local EGA Demo instance' > /etc/ega/banner

echo "Starting the SFTP server"
exec /opt/openssh/sbin/ega-sshd -D -e -f /etc/ega/sshd_config -Z /etc/ega/mq.conf
