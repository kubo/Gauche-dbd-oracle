What's is Gauche-dbd-oracle
===========================

This is an Oracle module for Gauche_.

.. _Gauche: http://practical-scheme.net/gauche/

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

Usage
=====

Gauche-dbd-oracle is a DBD module of Gauche-dbi_. Look at the Gauche-dbi_ for usage.

.. _Gauche-dbi: http://practical-scheme.net/gauche/man/?l=en&p=dbi

The DSN format of dbd-oracle is: "dbi:oracle:TNS_NAME" or "dbi:oracle://HOSTNAME:PORT/SID_NAME".
Username and password are passed as keywords as follows::

   (define conn (dbi-connect "dbi:oracle://localhost/XE" :username "scott" :password "tiger"))

Restrictions
============

* Oralce 8i or lower is not supported.
* Transactions are not supported. All DMLs are automatically committed.
* Date and timestamp data types are retrieved as string values.
