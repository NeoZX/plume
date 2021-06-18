#!/usr/bin/python3

from change_log_v1 import ChangeLogV1
from sys import argv


def fb_guid2str(fb_guid):
    guid = '{{{:X}{:X}{:X}{:X}-{:X}{:X}-{:X}{:X}-{:X}{:X}-{:X}{:X}{:X}{:X}{:X}{:X}}}'.format(
        fb_guid[3], fb_guid[2], fb_guid[1], fb_guid[0],
        fb_guid[5], fb_guid[4], fb_guid[7], fb_guid[6], fb_guid[8], fb_guid[9],
        fb_guid[10], fb_guid[11], fb_guid[12], fb_guid[13], fb_guid[14], fb_guid[15])
    return guid


size = {'header': 48,
        'block_header': 16,
        'tag': 1,
        'blob_id': 4 + 4,
        'length': 4,
        'atom_id': 4,
        'charset': 1,
        'atom': 1,
        'gen_value': 8}

if __name__ == '__main__':

    if len(argv) == 1:
        print("Usage: {} <replication_log_file>".format(argv[0]))
        exit(1)

    replication_file = ChangeLogV1.from_file(argv[1])

    print('= HEADERS ========================================')
    print('  version ', replication_file.header.version)
    print('    state ', str(replication_file.header.state)[14:])
    print('     GUID ', fb_guid2str(replication_file.header.replication_guid))
    print(' sequence ', replication_file.header.sequence)
    print('   length ', replication_file.header.log_length)

    pos = size['header']
    print('= BLOCKS ========================================')

    for block in replication_file.blocks.block:
        tra = ' '
        if block.flags.block_begin_trans:
            tra += 'START'
        if block.flags.block_end_trans:
            tra += 'END'
        print('0x{:08X} BLOCK PROTOCOL {} LENGTH {:d}{} TRANSACTION {}'.format(pos, block.protocol, block.length, tra,
                                                                               block.tra))
        pos += size['block_header']
        atom = {}
        atom_id = 0
        for operation in block.operations.operation:
            offset = size['tag']
            data = ''
            if operation.tag in (replication_file.Tags.insertrecord, replication_file.Tags.deleterecord):
                offset += size['atom_id'] + size['length']
                data += '{} length {} at 0x{:X}'.format(atom[operation.data.atom_id], operation.data.length,
                                                            pos + offset)
                offset += operation.data.length
            elif operation.tag == replication_file.Tags.updaterecord:
                offset += size['atom_id'] + size['length']
                data += '{} org length {} at 0x{:X}'.format(atom[operation.data.atom_id], operation.data.org_length,
                                                            pos + offset)
                offset += operation.data.org_length + size['length']
                data += ' new length {} at 0x{:X}'.format(operation.data.new_length, pos + offset)
                offset += operation.data.new_length
            elif operation.tag is replication_file.Tags.storeblob:
                offset += size['blob_id']
                blob_length = 0
                blob_part_count = 0
                for blob in operation.data.blob:
                    blob_length += blob.length
                    blob_part_count += 1
                data += 'Blob 0x{:X}:0x{:X} length {} at 0x{:X}'.format(operation.data.blob_high,
                                                                        operation.data.blob_low,
                                                                        blob_length, pos + offset)
                offset += 2 * blob_part_count + blob_length
            elif operation.tag == replication_file.Tags.executesql:
                offset += size['atom_id'] + size['length']
                data = '{} {}'.format(atom[operation.data.atom_id], operation.data.text)
                offset += operation.data.length
            elif operation.tag == replication_file.Tags.executesqlintl:
                offset += size['atom_id'] + size['charset'] + size['length']
                data = '{} {}'.format(atom[operation.data.atom_id], operation.data.text)
                offset += operation.data.length
            elif operation.tag == replication_file.Tags.setsequence:
                data = '{} {}'.format(atom[operation.data.atom_id], operation.data.value)
                offset += size['atom_id'] + size['gen_value']
            elif operation.tag == replication_file.Tags.defineatom:
                # todo: there may be problems with encodings of the atom
                data += str(operation.data.name)
                atom[atom_id] = operation.data.name
                atom_id += 1
                offset += size['atom'] + operation.data.length
            print('0x{:08X} {:19} {}'.format(pos, str(operation.tag)[5:], data))
            pos += offset

    replication_file.close()
