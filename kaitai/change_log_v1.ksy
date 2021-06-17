meta:
  id: change_log_v1
  title: Firebird change log
  application: firebird
  endian: le
  bit-endian: le
doc: | 
  Firebird change log version 1.
  Protocol 1 is used in Firebird 4.0
seq:
  - id: header
    size: 48
    type: header
  - id: blocks
    type: blocks
    size: header.blocks_length
types:
  header:
    seq:
      - id: magic
        contents: [ "FBCHANGELOG", 0x00 ]
        size: 12
      - id: version
        type: u2
      - id: state
        type: u2
        enum: journal_states
      - id: replication_guid
        size: 16
      - id: sequence
        type: s8
      - id: log_length
        type: s8
    instances:
      blocks_length:
        value: 'log_length == 0 ? 0 : log_length - 48'
  blocks:
    seq:
      - id: block
        type: block
        repeat: eos
  block:
    seq:
      - id: tra
        type: s8
      - id: protocol
        type: u2
      - id: flags
        size: 2
        type: flags
      - id: length
        type: u4
      - id: operations
        type: operations
        size: length
  operations:
    seq:
      - id: operation
        type: operation
        repeat: eos
  operation:
    seq:
      - id: tag
        type: u1
        enum: tags
      - id: data
        type:
          switch-on: tag
          cases:
            'tags::insertrecord': insertrecord
            'tags::updaterecord': updaterecord
            'tags::deleterecord': deleterecord
            'tags::storeblob': storeblob
            'tags::executesql': executesql
            'tags::executesqlintl': executesqlintl
            'tags::setsequence': setsequence
            'tags::defineatom': defineatom
  insertrecord:
    seq:
      - id: atom_id
        type: s4
      - id: length
        type: s4
      - id: record
        size: length
  updaterecord:
    seq:
      - id: atom_id
        type: s4
      - id: org_length
        type: s4
      - id: org_record
        size: org_length      
      - id: new_length
        type: s4
      - id: new_record
        size: org_length      
  deleterecord:
    seq:
      - id: atom_id
        type: s4
      - id: length
        type: s4
      - id: record
        size: length
  storeblob:
    seq:
      - id: blob_high
        type: s4
      - id: blob_low
        type: s4
      - id: blob
        type: blob
        # todo add eos
        repeat: until
        repeat-until: _.length == 0
  blob:
    seq:
      - id: length
        type: s2
      - id: blob_data
        size: length
  executesql:
    seq:
      - id: atom_id
        type: s4
        enum: charsets
      - id: length
        type: s4
      - id: text
        size: length
  executesqlintl:
    seq:
      - id: atom_id
        type: s4
      - id: charset
        type: s1
        enum: charsets
      - id: length
        type: s4
      - id: text
        size: length
  setsequence:
    seq:
      - id: atom_id
        type: s4
      - id: value
        type: s8
  defineatom:
    seq:
      - id: length
        type: u1
      - id: name
        size: length
  flags:
    seq:
      - id: block_begin_trans
        type: b1
      - id: block_end_trans
        type: b1
enums:
  journal_states:
    0: free
    1: used
    2: full
    3: arch
  tags:
    1: starttransaction
    2: preparetransaction
    3: committransaction
    4: rollbacktransaction
    5: cleanuptransaction
    6: startsavepoint
    7: releasesavepoint
    8: rollbacksavepoint
    9: insertrecord
    10: updaterecord
    11: deleterecord
    12: storeblob
    13: executesql
    14: setsequence
    15: executesqlintl
    16: defineatom
  charsets:
    0: none
    1: binary
    2: ascii
    3: unicode_fss
    4: utf8
    5: sjis
    6: eucj
    7: jis_0208
    8: unicode_ucs2
    9: dos_737
    10: dos_437
    11: dos_850
    12: dos_865
    13: dos_860
    14: dos_863
    15: dos_775
    16: dos_858
    17: dos_862
    18: dos_864
    19: next
    21: iso8859_1
    22: iso8859_2
    23: iso8859_3
    34: iso8859_4
    35: iso8859_5
    36: iso8859_6
    37: iso8859_7
    38: iso8859_8
    39: iso8859_9
    40: iso8859_13
    44: ksc5601
    45: dos_852
    46: dos_857
    47: dos_861
    48: dos_866
    49: dos_869
    50: cyrl
    51: win1250
    52: win1251
    53: win1252
    54: win1253
    55: win1254
    58: win1255
    59: win1256
    60: win1257
    61: utf16
    62: utf32
    63: koi8r
    64: koi8u
    65: win1258
    66: tis620
    67: gbk
    68: cp943c
    69: gb18030
    127: dynamic
