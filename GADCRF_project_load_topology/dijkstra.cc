#include "header_project.h"
 
// Nodo con minima distanza (tra quelli non considerati)
int minDistance(int n, int dist[], bool sptSet[])
{
   int min = -1, min_index;
 
   for (int v = 0; v < n; v++)
     if (sptSet[v] == false && ((dist[v]<=min && min!=-1 && dist[v]!=-1)
    		 	 	 	 	 ||(dist[v]>min && min==-1)))
         min = dist[v], min_index = v;
 
   return min_index;
}
 
// Stampa distanza di ogni nodo dal sorgente
void printSolution(int dist[], int n)
{
   printf("Node   Distance from Source\n");
   for (int i = 0; i < n; i++)
      printf("%d \t\t %d\n", i, dist[i]);
   printf("\n");
}
 
int* find_path(struct topologyLink ** net, int nodes, int src, int dest, int c, int *s)
{
     int n = nodes;
	 int dist[n];
	 int prev[n];
 
     bool sptSet[n];
     int temp, rim;

     // Inizializza vettori (distanza infinita rappresentata da -1)
     for (int i = 0; i < n; i++){
        dist[i] = -1;
        prev[i] = -1;
        sptSet[i] = false;
     }
 
     dist[src] = 0;
 
     // Trova il cammino minimo per tutti i nodi
     for (int count = 0; count < n-1; count++)
     {
       int u = minDistance(n, dist, sptSet);
 
       sptSet[u] = true;
 
       // Aggiorna vettore distanza:
       // il nodo non deve essere quello selezionato (sptSet),
       // deve esistere il link tra u e v,
       // il link tra u e v deve avere capacità residua sufficiente
       // il peso tra src e v deve essere minore del corrente
       // (come peso viene cinsiderato il numero di passi)
       for (int v = 0; v < n; v++)
       {
    	 rim = net[u][v].capacity - net[u][v].used;
    	 temp = (net[u][v].capacity==-1)?-1:1;

    	 if(!sptSet[v] && temp!=-1 && dist[u]!=-1 && ((dist[u]+temp<dist[v] && rim>=c && dist[v]!=-1)
    			 	 	 	 	 	 || dist[v]==-1)){
    		 dist[v]=dist[u]+temp;
    		 prev[v]=u;
    	 }

       }
     }
 
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
     *s = count+1;
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
