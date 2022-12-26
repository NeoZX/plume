#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <ibase.h>

#define PLUME_VERSION "0.5.2"

#define ERREXIT(status, rc) {isc_print_status(status); return rc;}

#define INDEX_LEN 64
#define INDEX_MAX 20000
#define IDX_MAX_THREADS 1024

#define QUERY_PRE_LENGTH 23
#define QUERY_POST_LENGTH 10

#define ERR_DB 1
#define ERR_MUTEX 2
#define ERR_ACT_IDX 3
#define ERR_ARG 4

#define SQL_VARCHAR(len) struct {short vary_length; char vary_string[(len)+1];}

#ifndef isc_dpb_parallel_workers
#define isc_dpb_parallel_workers 167
#endif
#define old_isc_dpb_parallel_workers 100

char *isc_database;
char *isc_user;
char *isc_password;
char *sel_str_act_idx =
    "select trim(cast(RDB$INDEX_NAME as char(63))) from RDB$INDICES "
    "where RDB$SYSTEM_FLAG<>1 and RDB$INDEX_INACTIVE=1 "
    "order by RDB$FOREIGN_KEY, RDB$RELATION_NAME;\0";
char *sel_str_stat_idx =
    "select trim(cast(RDB$INDEX_NAME as char(63))) from RDB$INDICES;\0";
char *sel_str;

struct upd_str
{
    char pre[QUERY_PRE_LENGTH];
    char post[QUERY_POST_LENGTH];
};
struct upd_str upd_str_template_act_idx =
{
    .pre = "ALTER INDEX \"\0",
    .post = "\" ACTIVE;\0"
};
struct upd_str upd_str_template_stat_idx =
{
    .pre = "SET STATISTICS INDEX \"\0",
    .post = "\";\0"
};
struct upd_str *upd_str_template = &upd_str_template_act_idx;

char idx_list[INDEX_MAX][INDEX_LEN];
int idx_num = 0;
pthread_mutex_t mutex_idx_num = PTHREAD_MUTEX_INITIALIZER;
int status[IDX_MAX_THREADS];
int threads_count = 1;
char fbd_parallel_workers = 0;
char code_isc_dpb_parallel_workers = isc_dpb_parallel_workers;
int log_level = 0;
const char action_activate[10] = "Activate\0";
const char action_set_stat[10] = "Set stat\0";
char *action = action_activate;
struct timespec start_main;

short goodbye = 0;

void help(char *name)
{
    printf("Usage: %s [options]\n"
           "Available options:\n"
           "\t-h help\n"
           "\t-v version\n"
           "\t-d database connections string\n"
           "\t-u username\n"
           "\t-p password\n"
           "\t-s set statistics indexes\n"
           "\t-q query retrieving the list of indexes to activate\n"
           "\t-t threads\n"
           "\t-P Firebird parallel workers\n"
           "\t--old_code_isc_dpb_parallel_workers use old code 100 instead of 167 "
           "(RedDatabase before 3.0.9 or HQBird 3)\n"
           "\t-l enable logging to stdout\n", name);
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
    sel_str = sel_str_act_idx;
    char *opts = "hvd:u:p:sq:t:P:l";
    int opt;
    int option_index = 0;
    static struct option long_options[] =
    {
        {"old_code_isc_dpb_parallel_workers", no_argument, 0, 0},
        {0,      0,           0, 0}
    };
    while ((opt = getopt_long(argc, argv, opts, long_options, &option_index)) != -1)
    {
        switch (opt)
        {
        case 0:
            switch (option_index)
            {
            case 0:
                code_isc_dpb_parallel_workers = old_isc_dpb_parallel_workers;
                break;
            default:
                printf("Unknown option %s", long_options[option_index].name);
                goodbye = 1;
            }
            break;
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
            if (sel_str == sel_str_act_idx)
            {
                sel_str = sel_str_stat_idx;
            }
            action = action_set_stat;
            break;
        case 'q':
            sel_str = optarg;
            break;
        case 't':
            threads_count = atoi(optarg);
            if (threads_count > IDX_MAX_THREADS)
            {
                threads_count = IDX_MAX_THREADS;
                printf("Threads count reduced to %d\n", IDX_MAX_THREADS);
            }
            break;
        case 'P':
            fbd_parallel_workers = atoi(optarg);
            break;
        case 'l':
            log_level = 1;
            break;
        }
    }
    return 0;
}

int get_index_list()
{
    //Database var
    char dpb_buffer[256], *dpb;
    short dpb_length = 0L;
    isc_db_handle db = 0L;
    isc_tr_handle trans = 0L;
    static char isc_tpb[5] = { isc_tpb_version1, isc_tpb_read, isc_tpb_read_committed,
                               isc_tpb_wait, isc_tpb_no_rec_version
                             };
    isc_stmt_handle stmt = 0L;
    ISC_STATUS_ARRAY db_status;
    XSQLDA *sqlda;
    long fetch_stat;
    SQL_VARCHAR(INDEX_LEN) idx_name;
    short flag0 = 0;

    dpb = dpb_buffer;
    *dpb = isc_dpb_version1;
    dpb_length = 1;
    isc_modify_dpb(&dpb, &dpb_length, isc_dpb_user_name, isc_user, strlen(isc_user));
    isc_modify_dpb(&dpb, &dpb_length, isc_dpb_password, isc_password, strlen(isc_password));

    //Connect to database
    if (isc_attach_database(db_status, strlen(isc_database), isc_database, &db, dpb_length, dpb))
    {
        fprintf(stderr, "Error attach to database %s\n", isc_database);
        ERREXIT(db_status, ERR_DB);
    }

    //start transaction
    if (isc_start_transaction(db_status, &trans, 1, &db, sizeof(isc_tpb), isc_tpb))
    {
        fprintf(stderr, "Error start transaction\n");
        ERREXIT(db_status, ERR_DB);
    }

    /* Allocate an output SQLDA. */
    sqlda = (XSQLDA *) malloc(XSQLDA_LENGTH(1));
    sqlda->sqln = 1;
    sqlda->sqld = 1;
    sqlda->version = SQLDA_VERSION1;

    //Create statement
    if (isc_dsql_allocate_statement(db_status, &db, &stmt))
    {
        fprintf(stderr, "Error allocate statement\n");
        free(sqlda);
        ERREXIT(db_status, ERR_DB);
    }

    //Prepare statement
    if (isc_dsql_prepare(db_status, &trans, &stmt, strlen(sel_str), sel_str, SQL_DIALECT_CURRENT, sqlda))
    {
        fprintf(stderr, "Error prepare statement\n%s\n", sel_str);
        free(sqlda);
        ERREXIT(db_status, ERR_DB);
    }

    sqlda->sqlvar[0].sqldata = (char *)&idx_name;
    sqlda->sqlvar[0].sqltype = SQL_VARYING + 1;
    sqlda->sqlvar[0].sqllen = INDEX_LEN;
    sqlda->sqlvar[0].sqlind  = &flag0;

    //Execute statement
    if (isc_dsql_execute(db_status, &trans, &stmt, SQL_DIALECT_CURRENT, NULL))
    {
        fprintf(stderr, "Error execute statement\n%s\n", sel_str);
        free(sqlda);
        ERREXIT(db_status, ERR_DB);
    }

    idx_num = 0;
    while ((fetch_stat = isc_dsql_fetch(db_status, &stmt, SQL_DIALECT_CURRENT, sqlda)) == 0)
    {
        memcpy(idx_list[idx_num], idx_name.vary_string, idx_name.vary_length);
        idx_list[idx_num][INDEX_LEN-1] = '\0';
        idx_num++;
        if (idx_num == INDEX_MAX)
        {
            break;
        }
    }

    printf("Found %d indexes\n", idx_num);

    if (fetch_stat != 100L)
    {
        fprintf(stderr, "Error fetch, fetch stat %ld \n", fetch_stat);
        free(sqlda);
        ERREXIT(db_status, ERR_DB);
    }

    //Free statement
    if (isc_dsql_free_statement(db_status, &stmt, DSQL_close))
    {
        fprintf(stderr, "Error free statement\n");
        free(sqlda);
        ERREXIT(db_status, ERR_DB)
    }

    //Commit transaction
    if (isc_commit_transaction(db_status, &trans))
    {
        fprintf(stderr, "Error commit transaction\n");
        free(sqlda);
        ERREXIT(db_status, ERR_DB);
    }

    //Close
    if (isc_detach_database(db_status, &db))
    {
        fprintf(stderr, "Error detach database\n");
        free(sqlda);
        ERREXIT(db_status, ERR_DB);
    }

    free(sqlda);

    return 0;
}

void * activate_index(void *thr_id_ptr)
{
    //Database var
    char dpb_buffer[256], *dpb;
    short dpb_length = 0L;
    isc_db_handle db = 0L;
    isc_tr_handle trans;
    static char isc_tpb[5] = { isc_tpb_version1, isc_tpb_write, isc_tpb_read_committed,
                               isc_tpb_wait, isc_tpb_no_rec_version
                             };
    ISC_STATUS_ARRAY db_status;
    char query[QUERY_PRE_LENGTH + INDEX_LEN + QUERY_POST_LENGTH];

    int thr_id = (int) thr_id_ptr;
    int idx_num_thread = 0;
    int mutex_status = 0;

    struct timespec tra_begin, tra_end;

    char thread_name[16];
    sprintf(thread_name, "ActIdx#%08X", thr_id);
    prctl(PR_SET_NAME, thread_name);

    dpb = dpb_buffer;
    *dpb++ = isc_dpb_version1;
    if (fbd_parallel_workers > 0)
    {
        *dpb++ = code_isc_dpb_parallel_workers;
        *dpb++ = sizeof(fbd_parallel_workers);
        *dpb++ = fbd_parallel_workers;
    }
    dpb_length = dpb - dpb_buffer;

    dpb = dpb_buffer;
    isc_modify_dpb(&dpb, &dpb_length, isc_dpb_user_name, isc_user, strlen(isc_user));
    isc_modify_dpb(&dpb, &dpb_length, isc_dpb_password, isc_password, strlen(isc_password));

    //Connect to database
    if (isc_attach_database(db_status, strlen(isc_database), isc_database, &db, dpb_length, dpb))
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

    //unlock idx_num
    mutex_status = pthread_mutex_unlock(&mutex_idx_num);
    if (mutex_status != 0)
    {
        fprintf(stderr, "Error %d unlock mutex\n", mutex_status);
        status[thr_id] = ERR_MUTEX;
        return thr_id_ptr;
    }

    if (log_level)
    {
        clock_gettime(CLOCK_REALTIME, &tra_end);
    }

    while ((idx_num_thread < INDEX_MAX) && (*idx_list[idx_num_thread]))
    {
        //Logging
        if (log_level) {
            clock_gettime(CLOCK_REALTIME, &tra_begin);
            //TODO: Activate or set statistics
            const double begin_delta = (double) (tra_begin.tv_sec - tra_end.tv_sec)
                                       + (double ) (tra_begin.tv_nsec - tra_end.tv_nsec) / 1e9;
            const double begin_time = (double) (tra_begin.tv_sec -
                                                start_main.tv_sec)
                                      + (double ) (tra_begin.tv_nsec - start_main.tv_nsec) / 1e9;
            printf("%4d %9.3f %7.3f %-9s index %s\n", thr_id, begin_time, begin_delta, action,
                   idx_list[idx_num_thread]);
        }

        //start transaction
        trans = 0L;
        if (isc_start_transaction(db_status, &trans, 1, &db, sizeof(isc_tpb), isc_tpb))
        {
            printf("Warning trouble on index %s\n", idx_list[idx_num_thread]);
            isc_print_status(db_status);
        }

        //did not master the parameterized query
        //form a query
        strcpy(query, upd_str_template->pre);
        strcat(query, idx_list[idx_num_thread]);
        strcat(query, upd_str_template->post);

        if (isc_dsql_exec_immed2(db_status, &db, &trans, strlen(query), query, SQL_DIALECT_CURRENT, NULL, NULL))
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
        //Logging
        if (log_level)
        {
            clock_gettime(CLOCK_REALTIME, &tra_end);
            const double end_delta = (double) (tra_end.tv_sec - tra_begin.tv_sec)
                                     + (double ) (tra_end.tv_nsec - tra_begin.tv_nsec) / 1e9;
            const double end_time = (double) (tra_end.tv_sec - start_main.tv_sec)
                                    + (double ) (tra_end.tv_nsec - start_main.tv_nsec) / 1e9;
            printf("%4d %9.3f %7.3f Committed index %s\n", thr_id, end_time, end_delta, idx_list[idx_num_thread]);
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

    //Close database attach
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
    if (isc_database == NULL)
    {
        fprintf(stderr, "Database connection string not specified.\n");
        exit(ERR_ARG);
    }
    if (isc_user == NULL)
    {
        fprintf(stderr, "Username not specified.\n");
        exit(ERR_ARG);
    }
    if (isc_password == NULL)
    {
        fprintf(stderr, "Password not specified.\n");
        exit(ERR_ARG);
    }

    //Check. Logging enabled
    if (log_level)
    {
        clock_gettime(CLOCK_REALTIME, &start_main);
    }

    //Get index list
    err = get_index_list();
    if (err)
    {
        fprintf(stderr, "Error get index list\n");
        return err;
    }

    //Check. no indexes to activate or set statistics
    if (idx_num == 0)
    {
        return(0);
    }
    idx_num = 0;

    //Activate index
    //Logging
    if (log_level)
    {
        printf("%-4s %-9s %-7s\n", "Thr", "time", "delta");
    }

    //Start threads
    for (current_thread = 0; current_thread < threads_count; current_thread++)
    {
        pthread_create(&(thr_act_idx[current_thread]), NULL, activate_index,
                       (void *) current_thread);
    }

    //Wait threads
    for (current_thread = 0; current_thread < threads_count; current_thread++)
    {
        pthread_join(thr_act_idx[current_thread], NULL);
        if (status[current_thread] > 0)
        {
            fprintf(stderr, "Error %d on thread %d\n", status[current_thread], current_thread);
            err = 1;
        }
    }

    return err;
}
