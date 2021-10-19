#!/usr/bin/python3

from replication_status_file import ReplicationStatusFile
from sys import argv

if __name__ == '__main__':

    if len(argv) == 1:
        print("Usage: {} <replication_log_file>".format(argv[0]))
        exit(1)

    status_file = ReplicationStatusFile.from_file(argv[1])

    print('= HEADERS ========================================')
    print('Last processed journal : ', status_file.header.sequence)
    print('    Active transaction : ', status_file.header.txn_count)
    print('                Offset : ', status_file.header.offset)
    if status_file.header.txn_count > 0:
        print('= TRANSACTIONS ===================================')
        print('   Journal | Transaction id')
        for transaction in status_file.transactions.transaction:
            print(' {:9d} | {:d} '.format(transaction.sequence, transaction.txn))
