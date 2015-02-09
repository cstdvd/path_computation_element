#include "header_project.h"

// Node with minimum distance (from the ones selected)
int minDistance(int n, int dist[], bool sptSet[])
{
	int min = -1, min_index;

	for (int v = 0; v < n; v++)
		if (sptSet[v] == false && ((dist[v]<=min && min!=-1 && dist[v]!=-1)
				||(dist[v]>min && min==-1)))
			min = dist[v], min_index = v;

	return min_index;
}

// Print distance of all the nodes from the source
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

	// Vectors inizialization (infinite distance = -1)
	for (int i = 0; i < n; i++){
		dist[i] = -1;
		prev[i] = -1;
		sptSet[i] = false;
	}

	dist[src] = 0;

	// Find minimum path for all the nodes
	for (int count = 0; count < n-1; count++)
	{
		int u = minDistance(n, dist, sptSet);

		sptSet[u] = true;

		/* Update distance vector:
		 * node must not be the one selected (sptSet),
		 * it must exist a link between u and v,
		 * link between u and v must has sufficient residual capacity
		 * weight between src and v must be less than the current one
		 * (weight is equal to the number of traversed router)
		 */
		for (int v = 0; v < n; v++)
		{
			rim = net[u][v].capacity - net[u][v].used;
			temp = (net[u][v].capacity==-1)?-1:1;

			if(!sptSet[v] && temp!=-1 && dist[u]!=-1 && rim>=c && ((dist[u]+temp<dist[v] && dist[v]!=-1)
					|| dist[v]==-1)){
				dist[v]=dist[u]+temp;
				prev[v]=u;
			}

		}
	}
	if (DEBUG){
		printSolution(dist, n);

		printf("Precedences vector: ");
		for(int i=0;i<n;i++)
			printf("%d ",prev[i]);
		printf("\n");
	}
	int prec=dest, count=0;
	while(prec!=src && count<n){
		count++;
		prec=prev[prec];
		if(prec==-1)
			return NULL;
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

/* Dijkstra Algorithm without constrains
 * Used for network simulation */
int* find_path_unconstrained(struct topologyLink ** net, int nodes, int src, int dest, int *s)
{
	int n = nodes;
	int dist[n];
	int prev[n];

	bool sptSet[n];
	int temp;

	// Vectors inizialization (infinite distance = -1)
	for (int i = 0; i < n; i++){
		dist[i] = -1;
		prev[i] = -1;
		sptSet[i] = false;
	}

	dist[src] = 0;

	// Find minimum path for all the nodes
	for (int count = 0; count < n-1; count++)
	{
		int u = minDistance(n, dist, sptSet);

		sptSet[u] = true;

		/* Update distance vector:
		 * node must not be the one selected (sptSet),
		 * it must exist a link between u and v,
		 * weight between src and v must be less than the current one
		 * (weight is equal to the number of traversed router)
		 */
		for (int v = 0; v < n; v++)
		{
			temp = (net[u][v].capacity==-1)?-1:1;

			if(!sptSet[v] && temp!=-1 && dist[u]!=-1 && ((dist[u]+temp<dist[v] && dist[v]!=-1)
					|| dist[v]==-1)){
				dist[v]=dist[u]+temp;
				prev[v]=u;
			}

		}
	}
	if (DEBUG){
		printSolution(dist, n);

		printf("Precedences vector: ");
		for(int i=0;i<n;i++)
			printf("%d ",prev[i]);
		printf("\n");
	}
	int prec=dest, count=0;
	while(prec!=src && count<n){
		count++;
		prec=prev[prec];
		if(prec==-1)
			return NULL;
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

	printf("\nPath from node %d to node %d with no capacity constrain: ",src,dest);
	for(int i=0;i<count+1;i++)
		printf("%d ",path[i]);

	printf("\n\n");

	return path;
}
