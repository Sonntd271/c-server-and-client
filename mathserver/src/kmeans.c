#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define MAX_POINTS 4096
#define MAX_CLUSTERS 32
#define NUM_THREADS 4 // Set the number of threads

typedef struct point
{
    float x; // The x-coordinate of the point
    float y; // The y-coordinate of the point
    int cluster; // The cluster that the point belongs to
} point;

// Change input arg to thread data
typedef struct thread_data
{
    int tid;
    bool something_changed;
} thread_data;

int	N;		// number of entries in the data
int k;      // number of centroids
FILE* fp;
char output_file[50];
point data[MAX_POINTS];		// Data coordinates
point cluster[MAX_CLUSTERS]; // The coordinates of each cluster center (also called centroid)

pthread_barrier_t barrier;

void read_data(int argc, char* argv[])
{
    N = 1797;
    k = 9;
    
    int option;
    char file_path[100];
    file_path[0] = '\0';
    
    while ((option = getopt(argc, argv, "k:f:o:")) != -1) {
        switch (option)
        {
            case 'k':
                k = atoi(optarg);
                break;
            case 'f':
                strcpy(file_path, optarg);
                break;
            case 'o':
                strcpy(output_file, optarg);
                break;
        }
    }

    if (file_path[0] == '\0')
    {
        perror("No file provided");
        exit(EXIT_FAILURE);
    }

    fp = fopen(file_path, "r");
    
    if (fp == NULL) {
        perror("Cannot open the file");
        exit(EXIT_FAILURE);
    }
   
    // Initialize points from the data file
    float temp;
    for (int i = 0; i < N; i++)
    {
        fscanf(fp, "%f %f", &data[i].x, &data[i].y);
        data[i].cluster = -1; // Initialize the cluster number to -1
    }
    // printf("Read the problem data!\n");
    // Initialize centroids randomly
    srand(0); // Setting 0 as the random number generation seed
    for (int i = 0; i < k; i++)
    {
        int r = rand() % N;
        cluster[i].x = data[r].x;
        cluster[i].y = data[r].y;
    }
    fclose(fp);
    remove(file_path);
}

int get_closest_centroid(int i, int k)
{
    /* find the nearest centroid */
    int nearest_cluster = -1;
    double xdist, ydist, dist, min_dist;
    min_dist = dist = INT_MAX;
    for (int c = 0; c < k; c++)
    { // For each centroid
        // Calculate the square of the Euclidean distance between that centroid and the point
        xdist = data[i].x - cluster[c].x;
        ydist = data[i].y - cluster[c].y;
        dist = xdist * xdist + ydist * ydist; // The square of Euclidean distance
        //printf("%.2lf \n", dist);
        if (dist <= min_dist)
        {
            min_dist = dist;
            nearest_cluster = c;
        }
    }
    //printf("-----------\n");
    return nearest_cluster;
}

bool assign_clusters_to_points()
{
    bool something_changed = false;
    int old_cluster = -1, new_cluster = -1;
    for (int i = 0; i < N; i++)
    { // For each data point
        old_cluster = data[i].cluster;
        new_cluster = get_closest_centroid(i, k);
        data[i].cluster = new_cluster; // Assign a cluster to the point i
        if (old_cluster != new_cluster)
        {
            something_changed = true;
        }
    }
    return something_changed;
}

void* assign_clusters_thread(void* arg) 
{
    thread_data* t_data = (thread_data*) arg;
    int thread_id = t_data->tid;

    int chunk_size = N / NUM_THREADS;
    int start = thread_id * chunk_size;
    int end = (thread_id == (NUM_THREADS - 1)) ? N : (start + chunk_size);

    bool something_changed = assign_clusters_to_points(start, end);
    t_data->something_changed = something_changed;

    pthread_barrier_wait(&barrier); // Wait for all threads to complete

    pthread_exit(NULL);
}

void update_cluster_centers()
{
    /* Update the cluster centers */
    int c;
    int count[MAX_CLUSTERS] = { 0 }; // Array to keep track of the number of points in each cluster
    point temp[MAX_CLUSTERS] = { 0.0 };

    for (int i = 0; i < N; i++)
    {
        c = data[i].cluster;
        count[c]++;
        temp[c].x += data[i].x;
        temp[c].y += data[i].y;
    }
    for (int i = 0; i < k; i++)
    {
        cluster[i].x = temp[i].x / count[i];
        cluster[i].y = temp[i].y / count[i];
    }
}

int kmeans(int k)
{
    bool somechange[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    thread_data tdata[NUM_THREADS];
    bool loop = true;

    pthread_barrier_init(&barrier, NULL, NUM_THREADS);

    int iter = 0;
    do {
        iter++; // Keep track of number of iterations

        for (int i = 0; i < NUM_THREADS; i++)
        {
            tdata[i].tid = i;
            pthread_create(&threads[i], NULL, &assign_clusters_thread, &tdata[i]);
        }
        for (int i = 0; i < NUM_THREADS; i++)
        {
            pthread_join(threads[i], NULL);
            somechange[i] = tdata[i].something_changed;
        }

        loop = false;
        for (int i = 0; i < NUM_THREADS; i++)
        {
            if (somechange[i])
            {
                loop = true;
                break;
            }
        }
        pthread_barrier_destroy(&barrier);

        update_cluster_centers();
    } while (loop);

    // printf("Number of iterations taken = %d\n", iter);
    // printf("Computed cluster numbers successfully!\n");
}

void write_results()
{
    char full_fp[255];
    sprintf(full_fp, "../computed_results/%s", output_file);
    FILE* fp = fopen(full_fp, "w");
    if (fp == NULL) {
        perror("Cannot open the file");
        exit(EXIT_FAILURE);
    }
    else
    {
        for (int i = 0; i < N; i++)
        {
            fprintf(fp, "%.2f %.2f %d\n", data[i].x, data[i].y, data[i].cluster);
        }
    }
    // printf("Wrote the results to a file!\n");
}

int main(int argc, char* argv[])
{
    read_data(argc, argv); 
    kmeans(k);
    write_results();
}