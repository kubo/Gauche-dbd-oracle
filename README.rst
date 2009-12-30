What's is Gauche-dbd-oracle
===========================

This is an Oracle module for Gauche-dbi_.
Supported Oracle versions are Oracle 9i and upper.

.. _Gauche-dbi: http://www.kahua.org/show/dev/DBI

How to Install
==============

Oracle Full Client on Unix
--------------------------

First, make sure that ORACLE_HOME is set correctly and LD_LIBRARY_PATH
indicates $ORACLE_HOME/lib.

Second, run the following command::

  gauche-package install Gauche-dbd-oracle-0.0.1.tgz

Oracle Instant Client on Unix
-----------------------------

First, download Oracle Instant Client zip packages: Basic and SDK.
Rpm packages are not supported.

Second, unzip them.::

  mkdir /opt
  cd /opt
  unzip instantclient-basic-linux32-11.1.0.7.zip
  unzip instantclient-sdk-linux32-11.1.0.7.zip

Third, make a symbolic link if it doesn't exist::

  cd /opt/instantclient_11_1
  ln -s libclntsh.so.11.1 libclntsh.so

Forth, make sure that LD_LIBRARY_PATH indicates the installed directory::

  LD_LIBRARY_PATH=/opt/instantclient_11_1
  export LD_LIBRARY_PATH

Fifth, run the following command::

  gauche-package install -C --with-instant-client=/opt/instantclient_11_1 Gauche-dbd-oracle-0.0.1.tgz
