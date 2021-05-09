import struct
import sys
import uuid


class FBJournal:
    """Class description"""

    TYPE_SIZE = {'UCHAR': 1,
                 'USHORT': 2,
                 'SSHORT': 2,
                 'ULONG': 4,
                 'SLONG': 4,
                 'ISC_TIME': 4,
                 'ISC_DATE': 4,
                 'ISC_TIMESTAMP': 8,
                 'SINT64': 8,
                 'UINT64': 8,
                 'GUID': 16}

    TYPE_FMT = {'USHORT': 'H',
                'ULONG': 'I',
                'UINT64': 'Q'}

    STATE = ('free', 'used', 'full', 'arch')

    def read_and_unpack(self, data_type):
        buffer = self.fd.read(self.TYPE_SIZE[data_type])
        if data_type == 'GUID':
            buffer = buffer[1:2] + buffer[0:1] + buffer[3:4] + buffer[2:3] + \
                     buffer[5:6] + buffer[4:5] + buffer[7:8] + buffer[6:7] + \
                     buffer[9:10] + buffer[8:9] + buffer[11:12] + buffer[10:11] + \
                     buffer[13:14] + buffer[12:13] + buffer[15:16] + buffer[14:15]
            return uuid.UUID(bytes=buffer)
        else:
            return struct.unpack(self.TYPE_FMT[data_type], buffer)[0]

    def __init__(self, filename):
        self.filename = filename
        self.fd = open(self.filename, 'rb')
        self.signature = None
        self.version = None
        self.state = None
        self.fb_guid = None
        self.sequence = None
        self.protocol = None
        self.len = None

        buffer = self.fd.read(10)
        if buffer == b'FBREPLLOG\x00':
            self.signature = buffer
            self.version = self.read_and_unpack('USHORT')
            self.state = self.read_and_unpack('ULONG')
            self.fb_guid = self.read_and_unpack('GUID')
            self.sequence = self.read_and_unpack('UINT64')
            self.protocol = self.read_and_unpack('ULONG')
            self.len = self.read_and_unpack('ULONG')
        else:
            print("It's not RedDatabase journal", file=sys.stderr)
            exit(1)

    def print_headers(self):
        print('= HEADERS ========================================')
        print('  version ', self.version)
        print('    state ', self.STATE[self.state])
        print('     GUID ', self.fb_guid)
        print(' sequence ', self.sequence)
        print(' protocol ', self.protocol)
        print('   length ', self.len)


if __name__ == '__main__':
    fb_journal = FBJournal(sys.argv[1])
    fb_journal.print_headers()
