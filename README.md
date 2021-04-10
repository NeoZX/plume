# About the project
This utility was designed for multithreaded indices activation in the Firebird RDBMS. 
The utility will speed up the backup-restore operation or when creating a large number of new indexes.

# Requirements
It requires libfbclient.so.2 for Linux or fbclient.dll for Windows.

For effective work:

* the database file must be located on the SSD drive
* the amount of RAM must be sufficient for the location of temporary files sorting
* the base must be switched to asynchronous mode, the ForceWrite flag is off (remember to set it again)

# Usage example
Simple. 10 connections in parallel activate indexes:

    plume -u SYSDBA -p masterkey -t 10 -d localhost:mydatabase

Set statistics all indexes:

    plume -u SYSDBA -p masterkey -t 10 -s -d localhost:mydatabase

Activating indexes on an custom query, for example, activating indexes on tables table1 and table2:

    plume -u SYSDBA -p masterkey -t 10 -d localhost:mydatabase \
        -q "select cast(rdb\$index_name as char(63)) from rdb\$indices \
            where rdb\$relation_name in ('TABLE1', 'TABLE2');"

With using feature Firebird Parallel Workers (avaliable in [HQbird 3.0](https://ib-aid.com/en/hqbird/) and [RedDatabase 3.0.7](https://reddatabase.ru/products/))
10 connections in parallel activate indexes. Each index is activated at 4 workers:

    plume -u SYSDBA -p masterkey -t 10 -P 4 localhost:mydatabase

# Restrictions
The maximum index name length is 63 bytes. 
The maximum number of indices is 20,000. 
The maximum number of threads is 1024.
