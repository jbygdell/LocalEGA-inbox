Notifications
=============

The messages sent by the OpenSSH hooks capture when a file is
(re)uploaded, renamed or removed.

Messages are JSON formatted as:

For a file upload:

.. code::

		{
                                 'user': <str>,
                             'filepath': <str>,
                            'operation': "upload",
                             'filesize': <num>,
                   'file_last_modified': <num>, // a UNIX timestamp
                  'encrypted_checksums': [{ 'type': <str>, 'value': <checksum as HEX> },
                                          { 'type': <str>, 'value': <checksum as HEX> },
					  ...
					  ]
		}

The checksum algorithm type is 'md5', or 'sha256'.
'sha256' is preferred.

For a file removal:

.. code::

		{
                                 'user': <str>,
                             'filepath': <str>,
                            'operation': "remove",
		}

For a file renaming:

.. code::

		{
                                 'user': <str>,
                             'filepath': <str>,
                              'oldpath': <str>,
                            'operation': "rename",
		}
