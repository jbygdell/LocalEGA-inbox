.. _`inbox login system`:

Inbox login system
==================

Central EGA contains a database of users with credentials (per
LocalEGA instance).

The authentication is either via a password or an RSA key against
CentralEGA database itself.

The solution uses CentralEGA's user IDs but can also be extended to
use Elixir IDs (of which we strip the ``@elixir-europe.org`` suffix).

The procedure is as follows: the inbox is started without any created
user. When a user wants to log into the inbox (actually, only ``sftp``
uploads are allowed), the code looks up the username in a local
cache, and, if not found, queries the CentralEGA REST endpoint. Upon
return, we store the user credentials in the local cache and create
the user's home directory. The user now gets logged in if the password
or public key authentication succeeds. Upon subsequent login attempts,
only the local cache is queried, until the user's credentials
expire. The cache has a default TTL of one hour, and is wiped clean
upon reboot (as a cache should).


Configuration
^^^^^^^^^^^^^

The NSS and PAM modules are configured by the file ``/etc/ega/auth.conf``.

Some configuration parameters can be specified, while others have
default values in case they are not specified. Some of the parameters must be
specified (mostly those for which we can't invent a value!).

A sample configuration file can be found on the `EGA-auth
repository
<https://github.com/EGA-archive/EGA-auth/blob/master/auth.conf.sample>`_,
eg:

.. code-block:: none

   ##########################################
   # Remote database settings (using ReST)
   ##########################################
   
   # The username will be appended to the endpoints
   cega_endpoint_username = http://cega_users/user/
   cega_endpoint_uid = http://cega_users/id/
   cega_creds = user:password
   
   # Selects where the JSON object is rooted
   # Use a dotted format à la JQ, eg level1.level2.level3
   # Default: empty
   cega_json_prefix = 
   
   ##########################################
   # Local database settings (for NSS & PAM)
   ##########################################

   # Absolute path to the SQLite database.
   # Required setting. No default value.
   db_path = /run/ega-users.db
   
   # Sets how long a cache entry is valid, in seconds.
   # Default: 3600 (ie 1h).
   # cache_ttl = 86400
   
   # Per site configuration, to shift the users id range
   # Default: 10000
   #ega_uid_shift = 1000
   
   # The group to which all users belong.
   # For the moment, only only.
   # Required setting. No default.
   ega_gid = 997
   
   # This causes the PAM sessions to be chrooted into the user's home directory.
   # Useful for SFTP connections, but complicated for regular ssh
   # connections (since no proper environment exists there).
   # Default: false
   chroot_sessions = yes
   
   # Per site configuration, where the home directories are located
   # The user's name will be appended.
   # Required setting. No default.
   ega_dir = /ega/inbox
   ega_dir_attrs = 2750 # rwxr-s---
   
   # sets the umask for each session (in octal format)
   # Default: 027 # world-denied
   #ega_dir_umask = 027
   
   # When the password is asked
   # Default: "Please, enter your EGA password: "
   #prompt = Knock Knock:
   
   # The user's login shell.
   # Default: /bin/bash
   #ega_shell = /bin/aspshell-r


.. note:: After proper configuration, there is no user maintenance, it is
   automagic. The other advantage is to have a central location of the
   EGA user credentials.

   Moreover, it is also possible to add non-EGA users if necessary, by
   reproducing the same mechanism but outside the temporary
   cache. Those users will persist upon reboot.


Implementation
^^^^^^^^^^^^^^

The cache is a SQLite database, mounted in a ``ramfs`` partition (of
initial size 200M). A ``ramfs`` partition does not survive a reboot,
grows dynamically and does not use the swap partition (as a ``tmpfs``
partition would). By default such option is disabled but can be
enabled in the `inbox` entrypoint script.

The NSS+PAM source code has its own `repository
<https://github.com/EGA-archive/EGA-auth>`_. A makefile is provided
to compile and install the necessary shared libraries.

The *ega-sshd* service is configured to use PAM by creating the file
``/etc/pam.d/ega-sshd`` as follows.

.. literalinclude:: /../conf/pam.ega

The authentication code of the library (ie the ``auth`` *type*) checks
whether the user has a valid ssh public key. If it is not the case,
the user is prompted to input a password. Central EGA stores password
hashes using the `BLOWFISH
<https://en.wikipedia.org/wiki/Blowfish_(cipher)>`_ hashing
algorithm. LocalEGA also supports the usual ``md5``, ``sha256`` and
``sha512`` algorithms available on most Linux distribution (They are
part of the C library).

Updating a user password is not allowed (ie therefore the ``password``
*type* is configured to deny every access).

The ``session`` *type* handles the chrooting.

The ``account`` *type* of the PAM module is a pass-through. It
succeeds. It also "refreshes" the cache information is case it has
expired. This cache expiration mechanism will capture the situation
where the user's credentials have been reset. If the user stays logged
in and idle, the ssh session will expire. If the user is not idle,
then it is the same behaviour as if the user account was created
locally (ie. in /etc/passwd and /etc/shadow).
