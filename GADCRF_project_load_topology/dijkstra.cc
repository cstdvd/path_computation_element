#include "header_project.h"
 
// A utility function to find the vertex with minimum distance value, from
// the set of vertices not yet included in shortest path tree
int minDistance(int n, int dist[], bool sptSet[])
{
   // Initialize min value
   int min = -1, min_index;
 
   for (int v = 0; v < n; v++)
     if (sptSet[v] == false && ((dist[v]<=min && min!=-1 && dist[v]!=-1)
    		 	 	 	 	 ||(dist[v]>min && min==-1)))
         min = dist[v], min_index = v;
 
   return min_index;
}
 
// A utility function to print the constructed distance array
void printSolution(int dist[], int n)
{
   printf("Node   Distance from Source\n");
   for (int i = 0; i < n; i++)
      printf("%d \t\t %d\n", i, dist[i]);
   printf("\n");
}
 
int* find_path(struct topologyLink ** net, int nodes, int src, int dest, int c)
{
     int n = nodes;
	 int dist[n];     // The output array.  dist[i] will hold the shortest
                      // distance from src to i
	 int prev[n];
 
     bool sptSet[n]; // sptSet[i] will true if vertex i is included in shortest
                     // path tree or shortest distance from src to i is finalized
     int temp;

     // Initialize all distances as INFINITE and stpSet[] as false
     for (int i = 0; i < n; i++){
        dist[i] = -1;
        prev[i] = -1;
        sptSet[i] = false;
     }
 
     // Distance of source vertex from itself is always 0
     dist[src] = 0;
 
     // Find shortest path for all vertices
     for (int count = 0; count < n-1; count++)
     {
       // Pick the minimum distance vertex from the set of vertices not
       // yet processed. u is always equal to src in first iteration.
       int u = minDistance(n, dist, sptSet);
 
       // Mark the picked vertex as processed
       sptSet[u] = true;
 
       // Update dist value of the adjacent vertices of the picked vertex.
       for (int v = 0; v < n; v++){
 
         // Update dist[v] only if is not in sptSet, there is an edge from 
         // u to v, and total weight of path from src to  v through u is 
         // smaller than current value of dist[v]
    	 //temp = ((net[u][v].capacity - net[u][v].used)==0)?-1:net[u][v].capacity - net[u][v].used;
    	 temp=net[u][v].capacity;

    	 if(!sptSet[v] && temp!=-1 && dist[u]!=-1 && ((dist[u]+temp<dist[v] && dist[v]!=-1)
    			 	 	 	 	 	 || dist[v]==-1)){
    		 dist[v]=dist[u]+temp;
    		 prev[v]=u;
    	 }

       }
     }
 
     // print the constructed distance array
     printSolution(dist, n);

     printf("Precedences vector: ");
     for(int i=0;i<n;i++)
    	 printf("%d ",prev[i]);
     printf("\n");

     int prec=dest, count=0;
     while(prec!=src){
    	 count++;
    	 prec=prev[prec];
     }

     int app=count;
     prec=dest;
     int* path=new int[count+1];
     while(prec!=src){
    	 path[app]=prec;
    	 prec=prev[prec];
    	 app--;
     }
     path[0]=src;

     printf("\nPath from node %d to node %d: ",src,dest);
     for(int i=0;i<count+1;i++)
         	 printf("%d ",path[i]);

     printf("\n\n");

	 return path;
}
