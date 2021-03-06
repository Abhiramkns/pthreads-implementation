#include <pthread.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define BITS 29

#define BARRIER_COUNT 1000
pthread_barrier_t barrier;

struct rs_args {
  int id;         /* thread index. */
  unsigned *val;  /* array. */
  unsigned *tmp;  /* temporary array. */
  int n;          /* size of array. */
  int *nzeros;    /* array of zero counters. */
  int *nones;     /* array of one counters. */
  int t;          /* number of threads. */
};

struct rs_args *args;


void copy_array (unsigned *dest, unsigned *src, int n)
{
  for ( ; n > 0; n-- )
    *dest++ = *src++;
}

void print_array (unsigned *val, int n)
{
  int i;
  for ( i = 0; i < n; i++ )
    printf ("%d \n", val[i]);
  printf ("\n");
}

/* Fill array with random values. */
void random_array (unsigned *val, int n)
{
  int i;
  for ( i = 0; i < n; i++ ) {
  	val[i] = (unsigned)lrand48() & (unsigned)((1 << BITS) - 1);  	
  }
}

/* Check if array is sorted. */
int array_is_sorted (unsigned *val, int n)
{
  int i;
  for ( i = 1; i < n; i++ )
    if ( val[i-1] > val[i] )
      return 0;
  return 1;
}

void radix_sort_thread (unsigned *val,unsigned *tmp, int start, int n, int *nzeros, int *nones, int thread_index,int t)
{
  
  unsigned *src, *dest;
  int bit_pos;
  int index0, index1;
  int i;
  printf("###### Got in main function, thread %d\n", thread_index);

  src = val;
  dest = tmp;

  for ( bit_pos = 0; bit_pos < BITS; bit_pos++ ) {

    nzeros[thread_index] = 0;
    for ( i = start; i < start + n; i++ ) {
      if ( ((src[i] >> bit_pos) & 1) == 0 ) {
	  	nzeros[thread_index]++;      	
      }	
    }
    nones[thread_index] = n - nzeros[thread_index];
	
    pthread_barrier_wait(&barrier);

    index0 = 0;
    index1 = 0;
    for ( i = 0; i < thread_index; i++ ) {
      index0 += nzeros[i];
      index1 += nones[i];
    }
    index1 += index0;
    for ( ; i < t; i++ ) {
      index1 += nzeros[i];
	}
	
    pthread_barrier_wait(&barrier);

    for ( i = start; i < start + n; i++ ) {
      if ( ((src[i] >> bit_pos) & 1) == 0 ) {
	  	dest[index0++] = src[i];      	
      } else {
	  	dest[index1++] = src[i];      	
      }
    }
	
    pthread_barrier_wait(&barrier);
	
    tmp = src;
    src = dest;
    dest = tmp;
  }
  printf ("\n====== Printing nzeros array of thread %d\n\n", thread_index);
  print_array (nzeros, n);
  printf ("\n====== Printing nones array of thread %d\n\n", thread_index);
  print_array (nones, n);
  
}


void thread_work (int rank)
{
  int start, count, n;
  int index = rank;
  printf("\n####### Thread_work: THREAD %d = %d \n\n", rank, args[index].id);
  // pthread_barrier_wait(&barrier);
  
  n = args[index].n / args[index].t; /* Number of elements this thread is in charge of */
  start = args[index].id * n; /* Thread is in charge of [start, start+n] elements */

  radix_sort_thread (args[index].val, args[index].tmp, start, n,
  		     args[index].nzeros, args[index].nones, args[index].id, args[index].t);
}



void radix_sort (unsigned *val, int n, int t)
{
  unsigned *tmp;
  int *nzeros, *nones;
  int r, i;
  
  long thread;
  pthread_t* thread_handles;

  tmp = (unsigned *) malloc (n * sizeof(unsigned));
  if (!tmp) { fprintf (stderr, "Malloc failed.\n"); exit(1); }

  nzeros = (int *) malloc (t * sizeof(int));
  if (!nzeros) { fprintf (stderr, "Malloc failed.\n"); exit(1); }
  nones = (int *) malloc (t * sizeof(int));
  if (!nones) { fprintf (stderr, "Malloc failed.\n"); exit(1); }

  thread_handles = malloc (t * sizeof(pthread_t));
  pthread_barrier_init (&barrier, NULL, t);
  
  for ( i = 0; i < t; i++ ) {
    args[i].id = i;
    args[i].val = val;
    args[i].tmp = tmp;
    args[i].n = n;
    args[i].nzeros = nzeros;
    args[i].nones = nones;
    args[i].t = t;
	
	printf ("####### CREATING THREAD id = %d\n", args[i].id);
    pthread_create (&thread_handles[i], NULL, thread_work, i);
  }
  
  printf ("####### THREADS SHOULD BE WORKING NOW \n");
  
  for ( i = 0; i < t; i++ ) {
    pthread_join (thread_handles[i], NULL);
    printf ("####### THREAD %d SHOULD BE FINISHED \n", i);
  }

  pthread_barrier_destroy(&barrier);
  free (thread_handles);
  free (args);

  printf ("\n====== Before return to main: val array ======\n");
  print_array (val, n);
  printf ("\n====== Before return to main: tmp array ======\n");
  print_array (tmp, n);

  if ( BITS % 2 == 1 ) {
    copy_array (val, tmp, n);  	
  }

  free (nzeros);
  free (nones);
  free (tmp);
}



void main (int argc, char *argv[]) 
{
  int n, t;
  unsigned *val;
  time_t start, end;
  int ok;

  /* User input: number of elements [n]. An array of size [n] will be then 
  	generated based on this user input. This input is not mandatory. */
  n = 1e6; /* By default one million, or 1 followed by 6 zeros */
  if ( argc > 1 )
    n = atoi (argv[1]);
  if ( n < 1 ) { fprintf (stderr, "Invalid number of elements.\n"); exit(1); }

  /* User input: number of threads [t]. Must be evenly divisible by the
  	number of elements [n]. This input is not mandatory. */
  t = 1;
  if ( argc > 2 )
    t = atoi (argv[2]);
  if ( t < 1 ) { fprintf (stderr, "Invalid number of threads.\n"); exit(1); }
  if ( (n / t) * t != n ) {
    fprintf (stderr, "Number of threads must divide number of elements.\n");
    exit(1);
  }

  /* Allocate array. */
  val = (unsigned *) malloc (n * sizeof(unsigned));
  if (!val) { fprintf (stderr, "Malloc failed.\n"); exit(1); }

  /* Initialize array. */
  printf ("Initializing array... "); fflush (stdout);
  random_array (val, n);
  // printf ("Done.\n");

  /* Allocate thread arguments. */
  args = (struct rs_args *) malloc (t * sizeof(struct rs_args));
  if (!args) { fprintf (stderr, "Malloc failed.\n"); exit(1); }
  
  printf ("\n====== In main, the original array ======\n");
  print_array (val, n);

  /* Sort array. */
  printf ("Sorting array... \n"); fflush (stdout);
  start = time (0);
  radix_sort (val, n, t); /* The main algorithm. */
  end = time (0);
  printf ("Done.\n");
  printf ("Elapsed time = %.0f seconds.\n", difftime(end, start));

  /* Check result. */
  printf ("Testing array... "); fflush (stdout);
  ok = array_is_sorted (val, n);
  printf ("Done.\n");
  if ( ok )
    printf ("Array is correctly sorted.\n");
  else
    printf ("Oops! Array is not correctly sorted.\n");

  if ( ok && n <= 30 ) {
	printf ("\n====== After return to main: tmp array ======\n");
    print_array (val, n);
  }
  
  /* Free array. */
  free (val);
}
