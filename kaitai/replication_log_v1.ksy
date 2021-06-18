meta:
  id: replication_log_v1
  title: Firebird replication log version 1, protocol 1 and 2
  application: rdb_smp_server
  endian: le
doc: | 
  Firebird replication log version 1.
  Protocol 1 is used in Red Database 2.6
  Protocol 2 is used in Red Database 3.0 and HQBird 3.0
seq:
  - id: header
    size: 48
    type: header
  - id: segments
    type: segments
    size: header.segments_length
types:
  header:
    seq:
      - id: magic
        contents: [ "FBREPLLOG", 0x00 ]
        size: 10
      - id: version
        type: u2
      - id: state
        type: u4
        enum: journal_states
      - id: replication_guid
        size: 16
      - id: sequence
        type: u8
      - id: protocol
        type: u4
      - id: journal_length
        type: u4
    instances:
      segments_length:
        value: 'journal_length == 0 ? 0 : journal_length - 48'
  segments:
    seq:
      - id: segment
        type: segment
        repeat: eos
  segment:
    seq:
      - id: isc_date
        type: u4
      - id: isc_time
        type: u4
      - id: segment_length
        type: u4
      - id: packets
        type: packets
        size: segment_length
    instances:
      unixtime:
        value: '(isc_date - 40617 + 30) * 86400 + isc_time / 1000'
  packets:
    seq:
      - id: packet
        type: packet
        repeat: eos
  packet:
    seq:
      - id: tag
        type: u1
        enum: tags
      - id: data
        type:
          switch-on: tag
          cases:
            'tags::initialize': einitialize
            'tags::startattachment': att
            'tags::cleanupattachment': att
            'tags::starttransaction': att_tra
            'tags::committransaction': tra
            'tags::rollbacktransaction': tra
            'tags::startsavepoint': tra
            'tags::cleanupsavepoint': tra_svp
            'tags::insertrecord': tra_table_data
            'tags::updaterecord': tra_table_update_data
            'tags::deleterecord': tra_table_data
            'tags::changegenerator': change_generator
            'tags::storeblob': tra_blob
            'tags::suspendtxns': tra
            'tags::resumetxns': tra
            'tags::synctxns': tra
            'tags::preparetransaction': tra
            'tags::executestatement': tra_exec
            'tags::changegenerator2': change_generator2
            'tags::createdatabase': isc_dpb
            'tags::executestatementintl': tra_exec_intl
  att:
    seq:
      - id: att
        type: 
          switch-on: _root.header.protocol
          cases:
            1: s4
            2: s8
  tra:
    seq:
      - id: tra
        type: 
          switch-on: _root.header.protocol
          cases:
            1: s4
            2: s8
  svp:
    seq:
      - id: svp
        type: 
          switch-on: _root.header.protocol
          cases:
            1: s4
            2: s8
  att_tra:
    seq:
      - id: att
        type: 
          switch-on: _root.header.protocol
          cases:
            1: s4
            2: s8
      - id: tra
        type: 
          switch-on: _root.header.protocol
          cases:
            1: s4
            2: s8
  tra_svp:
    seq:
      - id: tra
        type: 
          switch-on: _root.header.protocol
          cases:
            1: s4
            2: s8
      - id: undo
        type: s4
  tra_exec:
    seq:
      - id: tra
        type: 
          switch-on: _root.header.protocol
          cases:
            1: s4
            2: s8
      - id: length
        type: u4
      - id: exec
        size: length
  tra_exec_intl:
    seq:
      - id: charset
        type: s4
        enum: charsets
      - id: tra
        type: 
          switch-on: _root.header.protocol
          cases:
            1: s4
            2: s8
      - id: length
        type: u4
      - id: exec
        size: length
  isc_dpb:
    seq:
      - id: dpb_length
        type: 
          switch-on: _root.header.protocol
          cases:
            1: s2
            2: s4
      - id: length
        type: 
          switch-on: _root.header.protocol
          cases:
            1: s2
            2: s4
      - id: dpb_data
        size: length
  tra_blob:
    seq:
      - id: tra
        type: 
          switch-on: _root.header.protocol
          cases:
            1: s4
            2: s8
      - id: high_id
        type: s4
      - id: low_id
        type: s4
      - id: length
        type: u4
      - id: blob
        size: length
  tra_table_data:
    seq:
      - id: tra
        type: 
          switch-on: _root.header.protocol
          cases:
            1: s4
            2: s8
      - id: table_name_length
        type: u4
      - id: table_name
        type: str
        encoding: ASCII
        size: table_name_length
      - id: data_length
        type: u4
      - id: data
        size: data_length
  tra_table_update_data:
    seq:
      - id: tra
        type: 
          switch-on: _root.header.protocol
          cases:
            1: s4
            2: s8
      - id: table_name_length
        type: u4
      - id: table_name
        type: str
        encoding: ASCII
        size: table_name_length
      - id: org_data_length
        type: u4
      - id: org_data
        size: org_data_length
      - id: new_data_length
        type: u4
      - id: new_data
        size: new_data_length
  change_generator:
    seq:
      - id: gen_id
        type: s4
      - id: value
        type: s8
  change_generator2:
    seq:
      - id: gen_name_len
        type: u4
      - id: gen_name
        type: str
        encoding: ASCII
        size: gen_name_len
      - id: value
        type: s8
  einitialize:
    seq:
      - id: version
        type: u4
      - id: flags
        type: u4
      - id: length
        type: u4
      - id: data
        size: length
enums:
  journal_states:
    0: free
    1: used
    2: full
    3: arch
  tags:
    1: initialize
    2: startattachment
    3: cleanupattachment
    4: starttransaction
    5: committransaction
    6: rollbacktransaction
    7: startsavepoint
    8: cleanupsavepoint
    9: insertrecord
    10: updaterecord
    11: deleterecord
    12: changegenerator
    13: storeblob
    14: suspendtxns
    15: resumetxns
    16: synctxns
    17: preparetransaction
    18: cleanuptransactions
    19: executestatement
    20: changegenerator2
    21: createdatabase
    22: executestatementintl
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
