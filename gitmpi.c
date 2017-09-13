 // Author : Somay Jain
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

// t: timestep (0 or 1)
// X: width of the array
// Y: height of the array
// x, y : the element index desired.
#define IDX(x, y, t, X, Y) (((t) * X * Y) + ((y) * X) + (x))

// Will give the rows process pid will work on (inclusive).
void getWorkingRows(int pid, int numProcessors, int Y, int *lo, int *high);

void printRow(int *buffer, int X);

int isInside(int x, int y, int X, int Y);

void simulateRow(int y, int X, int numRows, int currentTimestep, int *buffer);

int main(int argc, char** argv)
{
    if(argc < 5)
    {
        printf("Too few command line arguments\n");
        printf("Usage: life <input file name> <# of generations> <X_limit> <Y_limit>\n");
        exit(1);
    }
    MPI_Status status;
    int tag = 12345;

    int X, Y, nIter;
    int pid, numProcessors;
    int currentTimestep = 0;

    MPI_Init(&argc,&argv);
    
    nIter = atoi(argv[2]);
    X = atoi(argv[3]);
    Y = atoi(argv[4]);
    
    MPI_Comm_size(MPI_COMM_WORLD, &numProcessors);
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);

    int lo, high, numRows;
    getWorkingRows(pid, numProcessors, Y, &lo, &high);
    numRows = high - lo + 3;

    // Size of the buffer = ((high - lo + 1) + 2 (padding)) * X * 2
    int *buffer = (int*) calloc(numRows * X * 2, sizeof(int));

    // printf("Process %d working on %d->%d\n", pid, lo, high);

    if(pid == 0)
    {
        // Get the bounadries of the other processes, so that we know whom to send.
        int *boundaries = (int*) malloc(sizeof(int) * numProcessors);
        for (int i=0; i<numProcessors; i++)
        {
            int temp;
            getWorkingRows(i, numProcessors, Y, &temp, boundaries + i);
        }

        FILE *fp = fopen(argv[1], "r");
        int x, y;
        int coord[2];
        while(fscanf(fp, "%d%d", &coord[0], &coord[1]) != EOF)
        {
            if (coord[1] <= high)
            {
                // This data belongs to current process, save it in the buffer
                buffer[IDX(coord[0], coord[1] - lo + 1, 0, X, numRows)] = 1;
                // printf("%d belongs to process 0, putting at %d\n", y, y-lo + 1);
                continue;
            }
            for (int i=1; i<numProcessors; i++)
            {
                if( coord[1] > boundaries[i-1] && coord[1] <= boundaries[i])
                {
                    // Belongs to process i.
                    // coord[0] = x; coord[1] = y;
                    MPI_Send(coord, 2, MPI_INT, i, tag, MPI_COMM_WORLD);
                    break;
                }
            }
        }

        // Send done to all processes.
        coord[0] = coord[1] = -1;
        for(int i=1; i<numProcessors; i++)
            MPI_Send(coord, 2, MPI_INT, i, tag, MPI_COMM_WORLD);

        fclose(fp);
    }
    else
    {
        int done = 0;
        int coord[2];
        // In a loop, keep receiving messages.
        // If you get a termination message, move out and wait.
        while(!done)
        {
            MPI_Recv(coord, 2, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
            // printf("Process %d received %d %d\n", pid, coord[0], coord[1]);

            if(coord[0] == -1 && coord[1] == -1)
                done = 1;
            else
            {
                buffer[IDX(coord[0], coord[1] - lo + 1, 0, X, numRows)] = 1;
                // printf("%d belongs to process %d, putting at %d\n", coord[1], pid, coord[1] - lo + 1);
            }
        }
    }

    // Reading complete, should synchronize here?
    // MPI_Barrier(MPI_COMM_WORLD);


    MPI_Request request[2], send_req[2];
    MPI_Status recv_status[2];
    int err_code;
    for(int iter = 0; iter < nIter; iter++)
    {
        // Non blocking receive from the process above
        if(pid > 0)
        {
            // printf("%d/%d\n", buffer[IDX(0, 0, currentTimestep, X, numRows)], numRows * X * 2);
            err_code = MPI_Irecv(&buffer[IDX(0, 0, currentTimestep, X, numRows)], 
                                X, MPI_INT, pid - 1, tag, MPI_COMM_WORLD, &request[0]);
            if(err_code != MPI_SUCCESS)
            {
                printf("Error in Non blocking receive from the process above\n");
            }
        }
        // Non blocking receive from the process below
        if(pid < numProcessors - 1)
        {
            // printf("%d/%d\n", buffer[IDX(0, numRows-1, currentTimestep, X, numRows)], numRows * X * 2);
            err_code = MPI_Irecv(&buffer[IDX(0, numRows-1, currentTimestep, X, numRows)],
                                X, MPI_INT, pid + 1, tag, MPI_COMM_WORLD, &request[1]);
            if(err_code != MPI_SUCCESS)
            {
                printf("Error in Non blocking receive from the process below\n");
            }
        }

        // Non blocking send to the process below
        if(pid < numProcessors - 1)
        {
            // printf("%d/%d\n", buffer[IDX(0, numRows-2, currentTimestep, X, numRows)], numRows * X * 2);
            err_code = MPI_Isend(&buffer[IDX(0, numRows-2, currentTimestep, X, numRows)],
                                X, MPI_INT, pid + 1, tag, MPI_COMM_WORLD, &send_req[0]);

            if(err_code != MPI_SUCCESS)
            {
                printf("Error in Non blocking send to the process below\n");
            }
            // printf("Process %d sending to %d\n", pid, pid + 1);
            // printRow(&buffer[IDX(0, numRows-2, currentTimestep, X, numRows)], X);
        }
        // Non blocking send to the process above
        if(pid > 0)
        {
            // printf("%d/%d\n", buffer[IDX(0, 1, currentTimestep, X, numRows)], numRows * X * 2);
            err_code = MPI_Isend(&buffer[IDX(0, 1, currentTimestep, X, numRows)],
                                X, MPI_INT, pid - 1, tag, MPI_COMM_WORLD, &send_req[1]);
            if(err_code != MPI_SUCCESS)
            {
                printf("Error in Non blocking send to the process above\n");
            }
            // printf("Process %d sending to %d\n", pid, pid - 1);
            // printRow(&buffer[IDX(0, 1, currentTimestep, X, numRows)], X);
        }

        if(pid == 0)
            simulateRow(1, X, numRows, currentTimestep, buffer);

        // Do computation from y=[2 - numRows-2)
        for(int y=2; y<numRows - 2; y++)
        {
            simulateRow(y, X, numRows, currentTimestep, buffer);
        }
        
        if(pid == numProcessors - 1)
            simulateRow(numRows-2, X, numRows, currentTimestep, buffer);

        // Check if the data from above/below is received. If so, perform
        // the computation.
        int guard0 = 1, guard1 = 1;
        if(pid > 0)
            guard0 = 0;
        if(pid < numProcessors - 1)
            guard1 = 0;

        while((!guard0) || (!guard1))
        {
            if(!guard0)
            {
                MPI_Test(&request[0], &guard0, &recv_status[0]);
                if(guard0)
                    simulateRow(1, X, numRows, currentTimestep, buffer);
            }

            if(!guard1)
            {
                MPI_Test(&request[1], &guard1, &recv_status[1]);
                if(guard1)
                    simulateRow(numRows-2, X, numRows, currentTimestep, buffer);
            }
        }


        /*
        // Wait for boundary data to be received.
        if(pid > 0)
        {
            MPI_Wait(&request[0], &recv_status[0]);
            simulateRow(1, X, numRows, currentTimestep, buffer);
            // printf("Process %d received from %d\n", pid, pid - 1);
            // printRow(&buffer[IDX(0, 0, currentTimestep, X, numRows)], X);

        }
        if(pid < numProcessors - 1)
        {
            MPI_Wait(&request[1], &recv_status[1]);
            simulateRow(numRows-2, X, numRows, currentTimestep, buffer);
            // printf("Process %d received from %d\n", pid, pid + 1);
            // printRow(&buffer[IDX(0, numRows-1, currentTimestep, X, numRows)], X);
            
        }
        */
     
        currentTimestep = 1 - currentTimestep;
        MPI_Barrier(MPI_COMM_WORLD);
    }

    // Print the ans.
    for(int x = 0; x < X; x++)
    {
        for(int y=1; y < numRows - 1; y++)
        {
            if(buffer[IDX(x, y, currentTimestep, X, numRows)] == 1)
                printf("%d %d\n", x, lo + y - 1);
        }
    }

    // Finalize the MPI environment.
    MPI_Finalize();
    return 0;
}


void getWorkingRows(int pid, int numProcessors, int Y, int *lo, int *high)
{
    // Assumption: number of processes < Y.
    int temp1 = Y / numProcessors;
    int temp2 = Y % numProcessors;
    if (pid < temp2)
    {    
        // return temp1 + 1;
        *lo = pid * (temp1 + 1);
        *high = (pid + 1) * (temp1 + 1) - 1;
        return;
    }
    else
    {
        *lo = temp2 * (temp1 + 1) + (pid - temp2) * temp1;
        *high = temp2 * (temp1 + 1) + (pid + 1 - temp2) * temp1 - 1;
        return;
    }
}

void printRow(int *buffer, int X)
{
    for(int i=0; i<X; i++)
    {
        printf("%d ", buffer[i]);
    }
    printf("\n");
}

int isInside(int x, int y, int X, int Y)
{
    if (x < 0 || x >= X)
        return 0;
    if(y < 0 || y >= Y)
        return 0;
    return 1;
}

void simulateRow(int y, int X, int numRows, int currentTimestep, int *buffer)
{
    int dirx[8] = { 0,  1,  1,  1,  0, -1, -1,  -1};
    int diry[8] = {-1, -1,  0,  1,  1,  1,  0,  -1};
    for(int x=0; x<X; x++)
    {
        int num_neighbours = 0;
        for(int k=0; k<8; k++)
        {
            int neighbourX = x + dirx[k];
            int neighbourY = y + diry[k];
            // Check if this is inside
            if(isInside(neighbourX, neighbourY, X, numRows))
            {
                // printf("%d/%d\n", IDX(neighbourX, neighbourY, currentTimestep, X, numRows), numRows * X * 2);
                if(buffer[IDX(neighbourX, neighbourY, currentTimestep, X, numRows)] == 1)
                    num_neighbours += 1;
            }
        }
        if(buffer[IDX(x, y, currentTimestep, X, numRows)] == 1)
        {
            // Cell was alive.
            // printf("%d/%d\n", IDX(x, y, 1 - currentTimestep, X, numRows), numRows * X * 2);
            if(num_neighbours == 2 || num_neighbours == 3)
                buffer[IDX(x, y, 1 - currentTimestep, X, numRows)] = 1;
            else
                buffer[IDX(x, y, 1 - currentTimestep, X, numRows)] = 0;
        }
        else
        {
            // printf("%d/%d\n", IDX(x, y, 1 - currentTimestep, X, numRows), numRows * X * 2);
            if(num_neighbours == 3)
                buffer[IDX(x, y, 1 - currentTimestep, X, numRows)] = 1;
            else
                buffer[IDX(x, y, 1 - currentTimestep, X, numRows)] = 0;
        }
    }
}