/* Sample implementation of wc utility. */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>

typedef unsigned long count_t;  /* Counter type */

/* Totals counters: chars, words, lines */
count_t total_ccount = 0;
count_t total_wcount = 0;
count_t total_lcount = 0;

char **file_list = NULL;
int file_count;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct file_data {
    count_t *ccount;
    count_t *wcount;
    count_t *lcount;
};
typedef struct file_data file_data;

/* Print error message and exit with error status. If PERR is not 0,
display current errno status. */
static void
error_print (int perr, char *fmt, va_list ap)
{
    vfprintf (stderr, fmt, ap);
    if (perr)
        perror (" ");
    else
        fprintf (stderr, "\n");
    exit (1);  
}

/* Print error message and exit with error status. */
static void
errf (char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    error_print (0, fmt, ap);
    va_end (ap);
}

/* Print error message followed by errno status and exit
    with error code. */
static void
perrf (char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    error_print (1, fmt, ap);
    va_end (ap);
}

/* Output counters for given file */
void
report (char *file, count_t ccount, count_t wcount, count_t lcount)
{
    printf ("%6lu %6lu %6lu %s\n", lcount, wcount, ccount, file);
}

/* Return true if C is a valid word constituent */
static int
isword (unsigned char c)
{
    return isalpha (c);
}

/* Increase character and, if necessary, line counters */
#define COUNT(c)       \
   (*ccount)++;        \
   if ((c) == '\n') \
     (*lcount)++;

/* Get next word from the input stream. Return 0 on end
of file or error condition. Return 1 otherwise. */
int
getword (FILE *fp, count_t *ccount, count_t *wcount, count_t *lcount)
{
    int c;
    int word = 0;

    if (feof (fp))
        return 0;
   
    while ((c = getc (fp)) != EOF)
    { 
        if (isword (c))
        {
            (*wcount)++;
            break;
        }
        COUNT (c);
    }

    for (; c != EOF; c = getc (fp))
    {
        COUNT (c);
        if (!isword (c))
            break;
    }

    return c != EOF;
}
   
/* Process file FILE. */
void
counter (char *file, count_t *ccount, count_t *wcount, count_t *lcount)
{
    FILE *fp = fopen (file, "r");

    if (!fp)
        perrf ("cannot open file `%s'", file);

    *ccount = *wcount = *lcount = 0;
    while (getword (fp, ccount, wcount, lcount))
    ;
    fclose (fp);

    //report (file, ccount, wcount, lcount);
    /*
    total_ccount += ccount;
    total_wcount += wcount;
    total_lcount += lcount;
    */
}
/* The index function gives the index of the file to the thread.
   It assigns the index of the agrv and returns -1 if there are no processes.
*/
int
get_index() {
    static int i = 0;
    int j;
    pthread_mutex_lock(&mutex);
    j = i+1;
    i++;
    pthread_mutex_unlock(&mutex);
    if (j < file_count)
        return j;
    else
        return -1;
    
}

void *
parallel_wc(void *data) {
    file_data *my_files = (file_data*) data; 
    int i;

    while((i = get_index()) != -1) {
        counter(file_list[i], &(my_files->ccount[i]), 
                              &(my_files->wcount[i]),
                              &(my_files->lcount[i]));
    }
    return data;
}

int
main (int argc, char **argv)
{
    int i, j;
    pthread_t *tid;    
    file_data *list; 

    count_t cc, wc, lc;

    int no_cpu;
    
    no_cpu = sysconf( _SC_NPROCESSORS_ONLN );    // create threads based on the number of threads the system can run at a time
    
    tid = (pthread_t *) malloc (no_cpu * sizeof(pthread_t));
    
    if (argc < 2)
        errf ("usage: wc FILE [FILE...]");

    file_list = argv;
    file_count = argc;
    for (i = 0; i < no_cpu; i++) {                                        // creating only a fixed number of threads(4 in this system).
        list = (file_data *) malloc (sizeof(file_data));
        list->ccount = (count_t *) malloc ((argc+1)*sizeof(count_t));     // size of the array=number of files. 
        list->wcount = (count_t *) malloc ((argc+1)*sizeof(count_t));     // any thread can get any file name.
        list->lcount = (count_t *) malloc ((argc+1)*sizeof(count_t));
        
        for(j = 1; j <= argc; j++) {
            list->ccount[j] = list->wcount[j] = list->lcount[j] = 0;
        }
                
        pthread_create(&tid[i], NULL, parallel_wc, (void *)list);
    }

    for(i = 0; i < no_cpu; i++) {
        pthread_join(tid[i], (void *)&list);
        for(j = 1; j < argc; j++) {
            if (list->ccount[j] && list->wcount[j] && list->lcount[j]) {                   
                report(argv[j], list->ccount[j], list->wcount[j], list->lcount[j]);   // summation of the counts.
                total_ccount += list->ccount[j];
                total_wcount += list->wcount[j];
                total_lcount += list->lcount[j];
            }
        }
    }
    if (argc > 2)
        report ("total", total_ccount, total_wcount, total_lcount);
    return 0;
}

