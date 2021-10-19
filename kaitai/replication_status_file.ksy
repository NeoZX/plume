meta:
  id: replication_status_file
  title: Firebird replication status file version 1 and 2
  application: rdb_smp_server, rdbserver
  endian: le
  bit-endian: le
doc: | 
  Replication status file using fore asynchronous replication.
  Used in Red Database 2.6 and Red Database 3.0
seq:
  - id: header
    type: header
  - id: transactions
    type: transactions
types:
  header:
    seq:
      - id: magic
        contents: [ "FBREPLCTL", 0x00 ]
        size: 10
      - id: version
        type: u2
      - id: txn_count
        type: u4
      - id: sequence
        type: u8
        if: version == 2
      - id: offset
        type: u4
  transactions:
    seq:
      - id: transaction
        type: transaction
        repeat: expr
        repeat-expr: _root.header.txn_count
  transaction:
    seq:
      - id: txn_hi
        type: s4
      - id: txn_low
        type: s4
      - id: seq_hi
        type: u4
      - id: seq_low
        type: u4
    instances:
      txn:
        value: txn_hi * 4294967296 + txn_low
      sequence:
        value: seq_hi * 4294967296 + seq_low
