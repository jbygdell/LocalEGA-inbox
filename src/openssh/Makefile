prefix=/opt/openssh
bindir=${prefix}/bin
sbindir=${prefix}/sbin
PRIVSEP_PATH=/var/empty/sshd

CC=gcc
LD=gcc
CFLAGS=-g -O2 -pipe \
       -Wall -Wpointer-arith -Wuninitialized -Wsign-compare -Wformat-security \
       -Wsizeof-pointer-memaccess -Wno-pointer-sign -Wno-unused-result \
       -mfunction-return=thunk -mindirect-branch=thunk -D_FORTIFY_SOURCE=2 \
       -fno-strict-aliasing -ftrapv -fno-builtin-memset -fstack-protector-strong -fPIE
CPPFLAGS=-I. -I.. \
	 -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_DEFAULT_SOURCE -DHAVE_CONFIG_H \
	 -DSSHDIR=\"${prefix}/etc\" -D_PATH_SSH_PIDDIR=\"/var/run\" -D_PATH_PRIVSEP_CHROOT_DIR=\"$(PRIVSEP_PATH)\"
LIBS=-lcrypto -ldl -lutil -lz  -lcrypt -lresolv
SSHDLIBS= -lpam
AR=ar
RANLIB=ranlib
INSTALL=/usr/bin/install -c
LDFLAGS=-L. -Lopenbsd-compat/ -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -fstack-protector-strong -pie

TARGETS=ega-sshd ssh-keygen

XMSS_OBJS=\
	ssh-xmss.o \
	sshkey-xmss.o \
	xmss_commons.o \
	xmss_fast.o \
	xmss_hash.o \
	xmss_hash_address.o \
	xmss_wots.o

LIBOPENSSH_OBJS=\
	ssh_api.o \
	ssherr.o \
	sshbuf.o \
	sshkey.o \
	sshbuf-getput-basic.o \
	sshbuf-misc.o \
	sshbuf-getput-crypto.o \
	krl.o \
	bitmap.o \
	${XMSS_OBJS}

LIBSSH_OBJS=${LIBOPENSSH_OBJS} \
	authfd.o authfile.o \
	canohost.o channels.o cipher.o cipher-aes.o cipher-aesctr.o \
	cipher-ctr.o cleanup.o \
	compat.o crc32.o fatal.o hostfile.o \
	log.o match.o moduli.o nchan.o packet.o opacket.o \
	readpass.o ttymodes.o xmalloc.o addrmatch.o \
	atomicio.o dispatch.o mac.o uuencode.o misc.o utf8.o \
	monitor_fdpass.o rijndael.o ssh-dss.o ssh-ecdsa.o ssh-rsa.o dh.o \
	msg.o progressmeter.o dns.o entropy.o gss-genr.o umac.o umac128.o \
	ssh-pkcs11.o smult_curve25519_ref.o \
	poly1305.o chacha.o cipher-chachapoly.o \
	ssh-ed25519.o digest-openssl.o digest-libc.o hmac.o \
	sc25519.o ge25519.o fe25519.o ed25519.o verify.o hash.o \
	kex.o kexdh.o kexgex.o kexecdh.o kexc25519.o \
	kexdhc.o kexgexc.o kexecdhc.o kexc25519c.o \
	kexdhs.o kexgexs.o kexecdhs.o kexc25519s.o \
	platform-pledge.o platform-tracing.o platform-misc.o

SSHDOBJS=sshd.o auth-rhosts.o auth-passwd.o \
	audit.o audit-bsm.o audit-linux.o platform.o \
	sshpty.o sshlogin.o servconf.o serverloop.o \
	auth.o auth2.o auth-options.o session.o \
	auth2-chall.o groupaccess.o \
	auth-bsdauth.o auth2-hostbased.o auth2-kbdint.o \
	auth2-none.o auth2-passwd.o auth2-pubkey.o \
	monitor.o monitor_wrap.o auth-krb5.o \
	auth2-gss.o gss-serv.o gss-serv-krb5.o \
	loginrec.o auth-pam.o auth-shadow.o auth-sia.o md5crypt.o \
	sftp-server.o sftp-common.o \
	../mq-config.o ../mq-notify.o \
	sandbox-null.o sandbox-rlimit.o sandbox-systrace.o sandbox-darwin.o \
	sandbox-seccomp-filter.o sandbox-capsicum.o sandbox-pledge.o \
	sandbox-solaris.o uidswap.o


all: /usr/local/lib/librabbitmq.so $(TARGETS)

$(LIBSSH_OBJS): config.h
$(SSHDOBJS): config.h

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

LIBCOMPAT=openbsd-compat/libopenbsd-compat.a
$(LIBCOMPAT):
	$(MAKE) -C openbsd-compat

libssh.a: $(LIBSSH_OBJS)
	$(AR) rv $@ $(LIBSSH_OBJS)
	$(RANLIB) $@

ega-sshd: libssh.a	$(LIBCOMPAT) $(SSHDOBJS) /usr/local/lib/librabbitmq.so
	$(LD) -o $@ $(SSHDOBJS) $(LDFLAGS) -lssh -lopenbsd-compat $(SSHDLIBS) $(LIBS) -L/usr/local/lib -lrabbitmq

ssh-keygen: $(LIBCOMPAT) libssh.a ssh-keygen.o
	$(LD) -o $@ ssh-keygen.o $(LDFLAGS) -lssh -lopenbsd-compat $(LIBS)

clean:
	rm -f *~ *.o *.a $(TARGETS)
	rm -f ../mq-*.o
	$(MAKE) -C openbsd-compat clean

install: $(TARGETS)
	mkdir -p $(bindir)
	mkdir -p $(sbindir)
	mkdir -p -m 0755 $(PRIVSEP_PATH)
	$(INSTALL) -m 0755 ssh-keygen $(bindir)/ssh-keygen
	$(INSTALL) -m 0755 ega-sshd $(sbindir)/ega-sshd

debug1: CFLAGS += -DDEBUG=1 -g -DREPORT
debug1: install

debug2: CFLAGS += -DDEBUG=2 -g -DREPORT
debug2: install

debug3: CFLAGS += -DDEBUG=3 -g -DREPORT
debug3: install

/usr/local/lib/librabbitmq.so:
	$(MAKE) -C ../rabbitmq-c install
