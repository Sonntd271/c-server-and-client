#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

//For Threads
#include <pthread.h>
#define MAX_SIZE 4096
#define NUM_THREADS 4

//Create alias for matrix
typedef double matrix[MAX_SIZE][MAX_SIZE];

int	N;		/* matrix size		*/
int	maxnum;		/* max number of element*/
char* Init;		/* matrix init type	*/
int	PRINT;		/* print switch		*/
matrix A;		/* matrix A		*/
matrix I = {0.0};  /* The A inverse matrix, which will be initialized to the identity matrix */
char* output_file; //the output will save in result folder

typedef struct thread_data {
    int tid;
}thread_data;

//Declaration thread and barrier
pthread_t threads[NUM_THREADS];
pthread_barrier_t barrier;  

/* Forward declarations */
void Init_Matrix(void);
void Print_Matrix(matrix M, char name[]);
void Init_Default(void);
int Read_Options(int argc, char** argv);
void* find_inverse_thread(void* arg);


//Main
int main(int argc, char** argv) {
    int i, timestart, timeend, iter;

    Init_Default();		/* Init default values	*/
    Read_Options(argc, argv);	/* Read arguments	*/
    Init_Matrix();		/* Init the matrix	*/

    pthread_barrier_init(&barrier, NULL, NUM_THREADS);

    //Checking for barrier
    if (pthread_barrier_init(&barrier, NULL, NUM_THREADS) != 0) {
    printf("No barrier\n");
    exit(1);
    }

    // Create array for multiple thread and run the threads
    struct thread_data thread_data[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].tid = i;
        int result = pthread_create(&threads[i], NULL, find_inverse_thread, (void*)&thread_data[i]);
    }

    // Wait for all threads to finish here
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // clear barrier
    pthread_barrier_destroy(&barrier);

    if (PRINT == 1) {
        Print_Matrix(I, "Inversed");
    }

}



void* find_inverse_thread(void* arg) {

    thread_data* t_data = (thread_data*) arg;
    int Each_Thread_Start = t_data->tid * (N / NUM_THREADS);
    int Each_THread_End = (t_data->tid + 1) * (N / NUM_THREADS);

    int row, col, p;
    double pivalue;

    for (p = 0; p < N; p++) {
        pivalue = A[p][p];
        for (col = 0; col < N; col++) {
            A[p][col] = A[p][col] / pivalue;
            I[p][col] = I[p][col] / pivalue;
        }
        assert(fabs(A[p][p] - 1.0) < 1e-9); 

        double multiplier;
        for (row = Each_Thread_Start; row < Each_THread_End; row++) {
            if (row != p) {
                multiplier = A[row][p];
                for (col = 0; col < N; col++) {
                    A[row][col] = A[row][col] - A[p][col] * multiplier;
                    I[row][col] = I[row][col] - I[p][col] * multiplier;
                }
                assert(fabs(A[row][p] - 0.0) < 1e-9);
            }
        }
        pthread_barrier_wait(&barrier);
    }
    
    pthread_exit(NULL);
}

/*###############################*/
void Init_Matrix()
{
    int row, col;


    for (row = 0; row < N; row++) {
        for (col = 0; col < N; col++) {
            if (row == col)
                I[row][col] = 1.0;
        }
    }
    
    char* Delete_Position = strchr(Init, '\n');
    if (Delete_Position != NULL) {
        *Delete_Position = '\0';  
    }



    if (strcmp(Init, "rand") == 0) {
        for (row = 0; row < N; row++) {
            for (col = 0; col < N; col++) {
                if (row == col) /* diagonal dominance */
                    A[row][col] = (double)(rand() % maxnum) + 5.0;
                else
                    A[row][col] = (double)(rand() % maxnum) + 1.0;
            }
        }
    }
    if (strcmp(Init, "fast") == 0) {
        for (row = 0; row < N; row++) {
            for (col = 0; col < N; col++) {
                if (row == col) /* diagonal dominance */
                    A[row][col] = 5.0;
                else
                    A[row][col] = 2.0;
            }
        }
    }

}

void Print_Matrix(matrix M, char name[])
{
    int row, col;
    char full_fp[255];
    sprintf(full_fp, "../computed_results/%s", output_file);
    FILE* fp = fopen(full_fp, "w");
    if (fp == NULL) {
        perror("Cannot open the file");
        exit(EXIT_FAILURE);
    }
    else
    {
        fprintf(fp, "%s Matrix:\n", name);
        for (row = 0; row < N; row++) {
            for (col = 0; col < N; col++)
                fprintf(fp, " %5.2f", M[row][col]);
            fprintf(fp, "\n");
        }
        fprintf(fp, "\n\n");
    }
}

void Init_Default()
{
    N = 5;
    Init = "fast";
    maxnum = 15.0;
    PRINT = 1;
}

int Read_Options(int argc, char** argv)
{
    char* prog;

    prog = *argv;
    while (++argv, --argc > 0)
        if (**argv == '-')
            switch (*++ * argv) {
            case 'n':
                --argc;
                N = atoi(*++argv);
                break;
            case 'h':
                printf("\nHELP: try matinv -u \n\n");
                exit(0);
                break;
            case 'u':
                printf("\nUsage: matinv [-n problemsize]\n");
                printf("           [-D] show default values \n");
                printf("           [-h] help \n");
                printf("           [-I init_type] fast/rand \n");
                printf("           [-m maxnum] max random no \n");
                printf("           [-P print_switch] 0/1 \n");
                exit(0);
                break;
            case 'D':
                printf("\nDefault:  n         = %d ", N);
                printf("\n          Init      = rand");
                printf("\n          maxnum    = 5 ");
                printf("\n          P         = 0 \n\n");
                exit(0);
                break;
            case 'I':
                --argc;
                Init = *++argv;
                break;
            case 'm':
                --argc;
                maxnum = atoi(*++argv);
                break;
            case 'P':
                --argc;
                PRINT = atoi(*++argv);
                break;
            case 'o':
                --argc;
                output_file = *++argv;
                break;
            default:
                printf("%s: ignored option: -%s\n", prog, *argv);
                printf("HELP: try %s -u \n\n", prog);
                break;
            }
}