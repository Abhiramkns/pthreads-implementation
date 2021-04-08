#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#define NN	0x500000
#define MINIMUMSIZE	1024
#define MAXSUBLISTS	(NN / MINIMUMSIZE)


struct sublist {
	int start;
	int end;
};

int N;
int P;
int args[64];
//= {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} 

float A[NN];

struct sublist sublist_info[MAXSUBLISTS];

volatile int head, tail;

volatile int sort_complete, sleepcount;
pthread_mutex_t mut;
pthread_cond_t cond_empty, cond_full;
pthread_barrier_t barr;

void insert_sublist(int start, int end)
{
	//printf("Insert sub list : %d, %d (Top : %d, sort complete : %d)\n", start, end, top, sort_complete);
	pthread_mutex_lock(&mut);
	//printf("Insert sub list :step 1\n");
	
	while (((tail + 1) % MAXSUBLISTS) == head)
	{
		//printf("Thread going to wait XXXX : <%d>\n",pthread_self());
		pthread_cond_wait(&cond_empty, &mut);
	}

	sublist_info[tail].start = start;
	sublist_info[tail].end = end;

	tail = (tail + 1) % MAXSUBLISTS;
	//printf("Insert sub list :step 2 : %d --- %d\n", count, MAXSUBLISTS);
	//if (top == 1)
	pthread_cond_signal(&cond_full);
	pthread_mutex_unlock(&mut);
}

struct sublist remove_sublist(void)
{
	struct sublist retval;

	//printf("Remove sub list\n");
	pthread_mutex_lock(&mut);
	while ((head == tail) && (!sort_complete))
	{
		//printf("top : %d, sortcomplete : %d\n",top, sort_complete);
		sleepcount++;
		if (sleepcount < P)
		{
			//printf("Thread going to wait : <%d> sleep count : %d - %d\n",pthread_self(), sleepcount, P);
			pthread_cond_wait(&cond_full, &mut);
			sleepcount--;
		}
		else
		{
			sort_complete = 1;
			pthread_cond_broadcast(&cond_full);
		}
	}

	//printf("Remove from sublist top : %d, sortcomplete : %d\n",top, sort_complete);
	if (sort_complete == 0)
	{
		retval = sublist_info[head];
		head = (head + 1) % MAXSUBLISTS;		

		//if (top == MAXSUBLISTS - 1)
		pthread_cond_signal(&cond_empty);
	}
	else
	{
		retval.start = 0;
		retval.end = 0;
	}

	pthread_mutex_unlock(&mut);

	//printf("Remove sublist : %d, %d\n", retval.start, retval.end);
	return retval;
}

void sort(int *a, int start, int end){
    int i, j;
	float val;
	for (i=start+1; i<=end; i++)
	{
		val = a[i];
		j = i-1;
		while (j >= start && a[j] > val)
		{
			a[j+1] = a[j];
			j--;
		}
		a[j+1] = val;
	}
}

void bucketsort(int start, int end){
    int n = end-start+1;
    float b[n][n];
    for(int i = 0; i < n; i++){
        b[i][0] = 0;
    }
    for(int i = start; i <= end; i++){
        float bi = n*A[i];
        b[bi][b[bi][0]] = A[i];
        b[bi][0]++;
    }
    for(int i = 0; i < n; i++){
        sort(A, start, end);
    }
    int index = 0;
    for(int i = 0; i < n; i++){
        for(int j = 0; j < b[i][0]; j++)
            A[index++] = b[i][j];
    }
}

void *thread_func(void *arg)
{
	struct sublist s;
	int done = 0;
	char mesg[100];
	int fd;
	int procno;

	procno = *(int *)arg;
	pthread_barrier_wait(&barr);
	printf("Thread id : %d - sublistlise : %d\n",procno, MAXSUBLISTS);
	if (procno == 0) {

		s.start = 0; 
		s.end = N-1;
	}
	else s = remove_sublist();
	do {
		if (s.start == 0 && s.end == 0) done = 1;
		else 
		{
			bucketsort(s.start, s.end);
			s = remove_sublist();
		}
	} while (!done);
	printf("Terminating the thread function\n");
	return NULL;
}

int main(int argc, char *argv[]){
    pthread_t thr[64];
	if (argc != 3){
		printf("Usage : quiksort <NPROC> <NELEMENTS>\n");
		exit(0);
	}
    int i, j;
	P = atoi(argv[1]);
	N = atoi(argv[2]);
	pthread_barrier_init(&barr, NULL, P);
	sleepcount = 0;
	sort_complete = 0;
	for (i=0; i<64; i++) args[i] = i;
	pthread_mutex_init(&mut, NULL);
	pthread_cond_init(&cond_empty, NULL);
	pthread_cond_init(&cond_full, NULL);
	// Initialize and Shuffle
	for (i=0; i<N; i++)
		A[i] = i;
	j = 1137;
	for (i=0; i<N; i++){
		float t = A[j];
		A[j] = A[i];
		A[i] = t;
		j = (j + 337) % N;
	}
    int minA = INT_MAX, maxA = INT_MIN; 
	for (i = 0; i<N; i++){
        if(minA > A[i])minA = A[i];
        else if(maxA < A[i])maxA = A[i];
        printf("%f ", A[i])
    }

    for(i=0; i<N; i++){
        A[i] = ((A[i]-minA)/(maxA - minA));
    }

	printf("\n");
	for (i=1; i<P; i++)
		pthread_create(&thr[i], NULL, thread_func, &args[i]);
	thread_func(&args[0]);
	for (i=1; i<P; i++)
		pthread_join(thr[i], NULL);
    for(i = 0; i < N; i++){
        A[i] = A[i]*(maxA - minA) + minA;
    }

	for (i=0; i<N; i++)
		printf("%8f",A[i]);
    printf("\n");
}