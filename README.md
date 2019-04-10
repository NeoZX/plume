# About the project
This utility was designed for multithreaded index activation in the Firebird RDBMS. 
The utility will speed up the backup-restore operation or when creating a large number of new indexes.

# Requirements
It requires libfbclient.so.2 for Linux or fbclient.dll for Windows.

For effective work:

* the database file must be located on the SSD drive
* the amount of RAM must be sufficient for the location of temporary files sorting
* the base must be switched to asynchronous mode, the ForceWrite flag is off (remember to set it again)

# Usage example
    plume -u SYSDBA -p masterkey -t 10 -d localhost:mydatabase

# Restrictions
The maximum index name length is 32 bytes. 
The maximum number of indices is 20,000. 
The maximum number of threads is 1024.
