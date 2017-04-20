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

/*This structure is used to have the details of the thread.it has the starting and ending index 
of the array of files and has arrays for counting the number of lines, character and words */
struct file_data {
    int from;
    int to;
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

void *
parallel_wc(void *data) {
    file_data *my_files = (file_data*) data; 
    int i;

    for(i = my_files->from; i <= my_files->to; i++) {
        counter(file_list[i], &(my_files->ccount[i-my_files->from]), 
                              &(my_files->wcount[i-my_files->from]),
                              &(my_files->lcount[i-my_files->from]));
    }
    return data;
}


int
main (int argc, char **argv)
{
    int i, j;
    pthread_t *tid;        
    file_data *list;       //the array containing the arguments to the main function

    count_t cc, wc, lc;

    int jump;
    int no_cpu;
    
    no_cpu = sysconf( _SC_NPROCESSORS_ONLN );                  // create threads based on the number of threads the system can run at a time
    printf("%d",no_cpu);
    tid = (pthread_t *) malloc (no_cpu * sizeof(pthread_t));

    if (argc < 2)
        errf ("usage: wc FILE [FILE...]");

    if (argc > 5) {
        file_list = argv;

        jump = (argc -1)/no_cpu;

        for (i = 0; i < no_cpu; i++) {                                   /*from-starting index
                                                                           to-ending index          
                                                                           and append the remaining threads to the last thread*/
            list = (file_data *) malloc (sizeof(file_data));                
            list->from = i*jump+1;
            list->to = (i+1)*jump;
	
	        if (i == no_cpu-1) {
                 list->to = argc-1;
            }

            list->ccount = (count_t *) malloc ((list->to - list->from+1)*sizeof(count_t));
            list->wcount = (count_t *) malloc ((list->to - list->from+1)*sizeof(count_t));
            list->lcount = (count_t *) malloc ((list->to - list->from+1)*sizeof(count_t));        
            pthread_create(&tid[i], NULL, parallel_wc, (void *)list);           // creates threads and calls the parallel_wc fuction
        }

        /* As in the WC implementation, we cannot have local variables wcount,ccount and lcount(threads updating value).
           Added counts to the struct. 
           The below for loop joins all the threads and increments the total counts
        */
        for(i = 0; i < no_cpu; i++) {
            pthread_join(tid[i], (void *)&list);            
            for(j = list->from; j <= list->to; j++) {
                report(argv[j], list->ccount[j-list->from], list->wcount[j-list->from], list->lcount[j-list->from]);
                total_ccount += list->ccount[j-list->from];
                total_wcount += list->wcount[j-list->from];
                total_lcount += list->lcount[j-list->from];
            }
        }
    } else {
        for(i = 1; i < argc; i++) {
            cc = wc = lc = 0;    
            counter(argv[i], &cc, &wc, &lc);
            report(argv[i], cc, wc, lc);
            total_ccount += cc;
            total_wcount += wc;
            total_lcount += lc;
        }
    }
    if (argc > 2)
        report ("total", total_ccount, total_wcount, total_lcount);
    return 0;
}

