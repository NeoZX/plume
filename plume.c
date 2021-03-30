#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <ibase.h>

#define PLUME_VERSION "0.3"

#define ERREXIT(status, rc)	{isc_print_status(status); return rc;}

#define INDEX_LEN	64
#define INDEX_MAX	20000
#define IDX_MAX_THREADS 1024

#define ERR_DB 1
#define ERR_MUTEX 2
#define ERR_ACT_IDX 3

#define SQL_VARCHAR(len) struct {short vary_length; char vary_string[(len)+1];}

#ifndef isc_dpb_parallel_workers
#define isc_dpb_parallel_workers 100
#endif

char *isc_database;
char *isc_user;
char *isc_password;
char sel_str_act_idx[160] =
    "select cast(RDB$INDEX_NAME as char(63)) from RDB$INDICES "
    "where RDB$SYSTEM_FLAG<>1 and RDB$INDEX_INACTIVE=1 "
    "order by RDB$FOREIGN_KEY, RDB$RELATION_NAME;";
char sel_str_stat_idx[60] =
    "select cast(RDB$INDEX_NAME as char(63)) from RDB$INDICES";
char *sel_str = sel_str_act_idx;
struct upd_str
{
    short index_name_pos;
    char query[90];
};
struct upd_str upd_str_template_act_idx =
{
    .query = "ALTER INDEX                                                                 ACTIVE",
    .index_name_pos = 12
};
struct upd_str upd_str_template_stat_idx =
{
    .query = "SET STATISTICS INDEX                                                                ",
    .index_name_pos = 21
};
struct upd_str *upd_str_template = &upd_str_template_act_idx;

char idx_list[INDEX_MAX][INDEX_LEN];
int idx_num = 0;
pthread_mutex_t mutex_idx_num = PTHREAD_MUTEX_INITIALIZER;
int status[IDX_MAX_THREADS];
int threads_count = 1;
char fbd_parallel_workers = 0;

short goodbye = 0;

void help(char *name)
{
    printf("Usage: %s [options]\n"
           "Avaliable options:\n"
           "\t-h help\n"
           "\t-v version\n"
           "\t-d database connections string\n"
           "\t-u username\n"
           "\t-p password\n"
           "\t-s set statistics index\n"
           "\t-q query retrieving the list of indexes to activate\n"
           "\t-t threads\n"
           "\t-P Firebird parallel workers\n"
           , name);
}

void version()
{
    int buffer;
    printf("Plume version " PLUME_VERSION "\n");
    printf("Restrictions:\n");
    printf("\tmax index name length %d\n", buffer = INDEX_LEN);
    printf("\tmax index counts %d\n", buffer = INDEX_MAX);
    printf("\tmax threads %d\n", buffer = IDX_MAX_THREADS);
}

int parse(int argc, char *argv[])
{
    char *opts = "hvd:u:p:sq:t:P:";
    int opt;
    while((opt = getopt(argc, argv, opts)) != -1)
    {
        switch(opt)
        {
        case 'h':
            help(argv[0]);
            goodbye = 1;
            break;
        case 'v':
            version();
            goodbye = 1;
            break;
        case 'd':
            isc_database = optarg;
            break;
        case 'u':
            isc_user = optarg;
            break;
        case 'p':
            isc_password = optarg;
            break;
        case 's':
            upd_str_template = &upd_str_template_stat_idx;
            if (sel_str == sel_str_act_idx) {
                sel_str = sel_str_stat_idx;
            }
            break;
        case 'q':
            sel_str = optarg;
            break;
        case 't':
            threads_count = atoi(optarg);
            if (threads_count > IDX_MAX_THREADS) {
                threads_count = IDX_MAX_THREADS;
                printf("Threads count reduced to %d\n", IDX_MAX_THREADS);
            }
            break;
        case 'P':
            fbd_parallel_workers = atoi(optarg);
            break;
        }
    }
    return 0;
}

int get_index_list()
{
	//Database var
	char			    dpb_buffer[256], *dpb;
	short		    	dpb_length = 0L;
	isc_db_handle		db = 0L;
	isc_tr_handle		trans = 0L;
	static char         isc_tpb[5] = {isc_tpb_version1,
                                     isc_tpb_read,
                                     isc_tpb_read_committed,
                                     isc_tpb_wait,
                                     isc_tpb_no_rec_version};
	isc_stmt_handle		stmt = 0L;
	ISC_STATUS_ARRAY	db_status;
	XSQLDA			    *sqlda;
	long			    fetch_stat;
	SQL_VARCHAR(INDEX_LEN)	idx_name;
	short		        flag0 = 0;

	int idx_num = 0;

	dpb = dpb_buffer;
	*dpb++ = isc_dpb_version1;
	dpb_length = dpb - dpb_buffer;

	dpb = dpb_buffer;
	isc_expand_dpb(&dpb, (short *) &dpb_length,
		isc_dpb_user_name, isc_user,
		isc_dpb_password, isc_password,
		NULL);

	//Connect to database
	if (isc_attach_database(db_status, 0, isc_database, &db, dpb_length, dpb))
	{
		fprintf(stderr, "Error attach to database %s\n", isc_database);
		ERREXIT(db_status, 1);
	}

	//start transaction
	if (isc_start_transaction(db_status, &trans, 1, &db, 5, isc_tpb))
	{
		fprintf(stderr, "Error start transaction\n");
		ERREXIT(db_status, 1);
	}

	/* Allocate an output SQLDA. */
	sqlda = (XSQLDA *) malloc(XSQLDA_LENGTH(1));
	sqlda->sqln = 1;
	sqlda->sqld = 1;
	sqlda->version = 1;

	//Create statement
	if (isc_dsql_allocate_statement(db_status, &db, &stmt))
	{
		fprintf(stderr, "Error allocate statement\n");
		ERREXIT(db_status, 1);
	}

	//Prepare statement
	if (isc_dsql_prepare(db_status, &trans, &stmt, 0, sel_str, SQL_DIALECT_CURRENT, sqlda))
	{
		fprintf(stderr, "Error prepare statement\n%s\n", sel_str);
		ERREXIT(db_status, 1);
	}

	sqlda->sqlvar[0].sqldata = (char *)&idx_name;
	sqlda->sqlvar[0].sqltype = SQL_VARYING + 1;
	sqlda->sqlvar[0].sqllen = INDEX_LEN;
	sqlda->sqlvar[0].sqlind  = &flag0;

	//Execute statement
	if (isc_dsql_execute(db_status, &trans, &stmt, 1, NULL))
	{
		fprintf(stderr, "Error execute statement\n%s\n", sel_str);
		ERREXIT(db_status, 1);
	}

	//Примечание: если использовать как строку, тогда терминировать '\0'
	while ((fetch_stat = isc_dsql_fetch(db_status, &stmt, 1, sqlda)) == 0)
	{
		memcpy(idx_list[idx_num], idx_name.vary_string, idx_name.vary_length);
		idx_list[idx_num][INDEX_LEN-1] = '\0';
		idx_num++;
		if (idx_num == INDEX_MAX) {
            break;
		}
	}

	printf("Found %d indexes\n", idx_num);

	if (fetch_stat != 100L)
	{
		fprintf(stderr, "Error fetch, fetch stat %ld \n", fetch_stat);
		ERREXIT(db_status, 1);
	}

	//Free statement
	if (isc_dsql_free_statement(db_status, &stmt, DSQL_close))
	{
		fprintf(stderr, "Error free statement\n");
		ERREXIT(db_status, 1)
	}

	//Commit transaction
	if (isc_commit_transaction(db_status, &trans))
	{
		fprintf(stderr, "Error commit transaction\n");
		ERREXIT(db_status, 1);
	}

	//Close
	if (isc_detach_database(db_status, &db))
	{
		fprintf(stderr, "Error detach database\n");
		ERREXIT(db_status, 1);
	}

	free(sqlda);

    return 0;
}

void * activate_index(void *thr_id_ptr)
{
	//Database var
	char			    dpb_buffer[256], *dpb;
	short		    	dpb_length = 0L;
	isc_db_handle		db = 0L;
	isc_tr_handle		trans = 0L;
	static char         isc_tpb[5] = {isc_tpb_version1,
                                     isc_tpb_write,
                                     isc_tpb_read_committed,
                                     isc_tpb_wait,
                                     isc_tpb_no_rec_version};
	ISC_STATUS_ARRAY	db_status;
    struct upd_str upd_str = *upd_str_template;

	int thr_id = (int) thr_id_ptr;
	int idx_num_thread = 0;
	int mutex_status = 0;

	dpb = dpb_buffer;
	*dpb++ = isc_dpb_version1;
	if (fbd_parallel_workers > 0)
	{
		*dpb++ = isc_dpb_parallel_workers;
		*dpb++ = sizeof(fbd_parallel_workers);
		*dpb++ = fbd_parallel_workers;
	}
	dpb_length = dpb - dpb_buffer;

	dpb = dpb_buffer;
	isc_expand_dpb(&dpb, (short *) &dpb_length,
		isc_dpb_user_name, isc_user,
		isc_dpb_password, isc_password,
		NULL);

	//Connect to database
	if (isc_attach_database(db_status, 0, isc_database, &db, dpb_length, dpb))
	{
	    isc_print_status(db_status);
	    status[thr_id] = ERR_DB;
	    return thr_id_ptr;
	}

    //lock idx_num
    mutex_status = pthread_mutex_lock(&mutex_idx_num);
    if (mutex_status != 0)
    {
        fprintf(stderr, "Error %d lock mutex\n", mutex_status);
	    status[thr_id] = ERR_MUTEX;
	    return thr_id_ptr;
    }

	idx_num_thread = idx_num++;

	//unlock  idx_num
    mutex_status = pthread_mutex_unlock(&mutex_idx_num);
    if (mutex_status != 0)
    {
        fprintf(stderr, "Error %d unlock mutex\n", mutex_status);
	    status[thr_id] = ERR_MUTEX;
	    return thr_id_ptr;
    }

    while((*idx_list[idx_num_thread]) && (idx_num_thread < INDEX_MAX))
    {
        //printf("Activate index %s\n", idx_list[idx_num_thread]);

        //start transaction
        trans = 0L;
        if (isc_start_transaction(db_status, &trans, 1, &db, 5, isc_tpb))
        {
            printf("Warning trouble on index %s\n", idx_list[idx_num_thread]);
            isc_print_status(db_status);
        }

        //did not master the parameterized query
        memcpy(upd_str.query + upd_str.index_name_pos, idx_list[idx_num_thread], INDEX_LEN - 1);

        if (isc_dsql_exec_immed2(db_status, &db, &trans, 0, upd_str.query, SQL_DIALECT_CURRENT, NULL, NULL))
        {
            printf("Warning trouble on index %s\n", idx_list[idx_num_thread]);
            isc_print_status(db_status);
            status[thr_id] = ERR_ACT_IDX;
        }

        //Commit transaction
        if (isc_commit_transaction(db_status, &trans))
        {
            printf("Warning trouble on index %s\n", idx_list[idx_num_thread]);
            isc_print_status(db_status);
            status[thr_id] = ERR_DB;
            return thr_id_ptr;
        }
        //lock idx_num
        mutex_status = pthread_mutex_lock(&mutex_idx_num);
        if (mutex_status != 0)
        {
            fprintf(stderr, "Error %d lock mutex\n", mutex_status);
            status[thr_id] = ERR_MUTEX;
            return thr_id_ptr;
        }

        idx_num_thread = idx_num++;

        //unlock  idx_num
        mutex_status = pthread_mutex_unlock(&mutex_idx_num);
        if (mutex_status != 0)
        {
            fprintf(stderr, "Error %d unlock mutex\n", mutex_status);
            status[thr_id] = ERR_MUTEX;
            return thr_id_ptr;
        }
    }

	//Close datatabase attac
	if (isc_detach_database(db_status, &db))
	{
        isc_print_status(db_status);
        status[thr_id] = ERR_DB;
        return thr_id_ptr;
	}

    return 0;
}

int main(int argc, char *argv[])
{
    pthread_t thr_act_idx[IDX_MAX_THREADS];
    int err = 0;
    int current_thread;

    //Parse arguments
	if (argc == 1)
	{
		help(argv[0]);
		return 0;
	}

	parse(argc, argv);

	if (goodbye == 1)
	{
		return 0;
	}

	//Validate arg

	//Get index list
    err = get_index_list();
    if (err) {
        fprintf(stderr, "Error get index list\n");
        return err;
    }

    //Activate index

    //Start threads
    for (current_thread = 0; current_thread < threads_count; current_thread++)
    {
        pthread_create(&(thr_act_idx[current_thread]), NULL, activate_index, (void *) current_thread);
    }

    //Wait threads
    for (current_thread = 0; current_thread < threads_count; current_thread++)
    {
        pthread_join(thr_act_idx[current_thread], NULL);
        if (status[current_thread] > 0) {
            fprintf(stderr, "Error %d on thread %d\n", status[current_thread], current_thread);
            err = 1;
        }
    }

    return err;
}
