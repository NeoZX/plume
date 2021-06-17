all: change_log_v1.py replication_log_v1.py

change_log_v1.py: kaitai/change_log_v1.ksy
	ksc -t python kaitai/change_log_v1.ksy

replication_log_v1.py: kaitai/replication_log_v1.ksy
	ksc -t python kaitai/replication_log_v1.ksy
