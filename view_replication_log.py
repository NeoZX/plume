#!/usr/bin/python3

from replication_log_v1 import ReplicationLogV1
from datetime import datetime
from sys import argv


def fb_guid2str(fb_guid):
    guid = '{{{:X}{:X}{:X}{:X}-{:X}{:X}-{:X}{:X}-{:X}{:X}-{:X}{:X}{:X}{:X}{:X}{:X}}}'.format(
        fb_guid[3], fb_guid[2], fb_guid[1], fb_guid[0],
        fb_guid[5], fb_guid[4], fb_guid[7], fb_guid[6], fb_guid[8], fb_guid[9],
        fb_guid[10], fb_guid[11], fb_guid[12], fb_guid[13], fb_guid[14], fb_guid[15])
    return guid


size = {'header': 48,
        'segment_header': 12,
        'tag': 1,
        'att': 4,
        'tra': 4,
        'svp': 4,
        'version': 4,
        'flags': 4,
        'length': 4,
        'charset': 4,
        'dpb_length': 2,
        'blob_id': 4 + 4,
        'gen_id': 4,
        'gen_value': 8}

if __name__ == '__main__':

    if len(argv) == 1:
        print("Usage: {} <replication_log_file>".format(argv[0]))
        exit(1)

    replication_file = ReplicationLogV1.from_file(argv[1])

    size['att'] *= replication_file.header.protocol
    size['tra'] *= replication_file.header.protocol
    size['svp'] *= replication_file.header.protocol

    print('= HEADERS ========================================')
    print('  version ', replication_file.header.version)
    print('    state ', str(replication_file.header.state)[14:])
    print('     GUID ', fb_guid2str(replication_file.header.replication_guid))
    print(' sequence ', replication_file.header.sequence)
    print(' protocol ', replication_file.header.protocol)
    print('   length ', replication_file.header.journal_length)

    print('= SEGMENTS =======================================')
    print('    OFFSET TAG                     ATT/TRA DATA')
    pos = size['header']
    for segment in replication_file.segments.segment:
        print('0x{:08X} START SEGMENT {} PACKETS LENGTH 0x{:X}'.format(pos,
                                                             datetime.fromtimestamp(segment.unixtime).strftime(
                                                                 "%Y-%m-%d %H:%M:%S"), segment.packets_length))
        pos += size['segment_header']
        for packet in segment.packets.packet:
            tra = ''
            data = ''
            offset = size['tag']
            if packet.tag == replication_file.Tags.cleanuptransactions:
                tra = 'non-tra'
            elif type(packet.data) is replication_file.Einitialize:
                data = 'version:{} flags:0x{:X} length:0x{:X}'.format(packet.data.version, packet.data.flags,
                                                                  packet.data.length)
                offset += size['version'] + size['flags'] + size['length'] + packet.data.length
            elif type(packet.data) is replication_file.Att:
                tra = packet.data.att
                offset += size['att']
            elif type(packet.data) is replication_file.Tra:
                tra = packet.data.tra
                offset += size['tra']
            elif type(packet.data) is replication_file.AttTra:
                tra = packet.data.att
                data = packet.data.tra
                offset += size['att'] + size['tra']
            elif type(packet.data) is replication_file.TraSvp:
                tra = packet.data.tra
                data = packet.data.undo
                offset += size['tra'] + size['svp']
            elif type(packet.data) is replication_file.TraExec:
                tra = packet.data.tra
                data = packet.data.exec
                offset += size['tra'] + size['length'] + packet.data.length
            elif type(packet.data) is replication_file.TraExecIntl:
                tra = packet.data.tra
                offset += size['tra'] + size['charset'] + size['length']
                data = '{} length 0x{:X} at 0x{:X} {}'.format(packet.data.charset, packet.data.length, pos + offset,
                                                          packet.data.exec)
                offset += packet.data.length
            elif type(packet.data) is replication_file.IscDpbData:
                tra = 'non-tra'
                offset += size['dpb_length'] + size['dpb_length']
                data = 'dpb length 0x{:X} at 0x{:X}'.format(packet.data.dpb_length,
                                                        pos + offset)
                offset += packet.data.length
            elif type(packet.data) is replication_file.TraBlob:
                tra = packet.data.tra
                offset += size['tra'] + size['blob_id'] + size['length']
                data = 'Blob 0x{:X}:0x{:X} length 0x{:X} at 0x{:X}'.format(packet.data.high_id, packet.data.low_id,
                                                                       packet.data.length, pos + offset)
                offset += packet.data.length
            elif type(packet.data) is replication_file.TraTableData:
                tra = packet.data.tra
                offset += size['tra'] + size['length'] + packet.data.table_name_length + size['length']
                data = '{} length {} at 0x{:X}'.format(packet.data.table_name, packet.data.data_length, pos + offset)
                offset += packet.data.data_length
            elif type(packet.data) is replication_file.TraTableUpdateData:
                tra = packet.data.tra
                offset += size['tra'] + size['length'] + packet.data.table_name_length + size['length']
                data = '{} org length {} at 0x{:X}'.format(packet.data.table_name, packet.data.org_data_length,
                                                           pos + offset)
                offset += packet.data.org_data_length + size['length']
                data += ', new length {} at 0x{:X}'.format(packet.data.new_data_length, pos + offset)
                offset += packet.data.new_data_length
            elif type(packet.data) is replication_file.ChangeGenerator:
                tra = 'non-tra'
                data = 'gen_id {} value {}'.format(packet.data.gen_id, packet.data.value)
                offset += size['gen_id'] + size['gen_value']
            elif type(packet.data) is replication_file.ChangeGenerator2:
                tra = 'non-tra'
                data = '{} value {}'.format(packet.data.gen_name, packet.data.value)
                offset += size['length'] + packet.data.gen_name_len + size['gen_value']
            print('0x{:08X} {:20} {:10} {}'.format(pos, str(packet.tag)[5:], tra, data))
            pos += offset

    replication_file.close()
