/*
 * Author: Roberta Fumarola*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <pdel/structs/xml.h>
#include <pdel/structs/structs.h>
#include <pdel/structs/types.h>
#include <pdel/structs/xmlrpc.h>
#include <expat.h>
#include <pthread.h>


#define NUM_NODES 5
#define CHAR_CAPACITY 10
#define CHAR_ADDRESS 50
#define CHAR_INTERFACE 30

struct link{
	int capacity;
	int used;
	char srcAddr[CHAR_ADDRESS];
	char dstAddr[CHAR_ADDRESS];
	char srcInterface[CHAR_INTERFACE];
	char dstInterface[CHAR_INTERFACE];
};

struct charLink{
	char capacity[CHAR_CAPACITY];
	char used[CHAR_CAPACITY];
	char srcAddr[CHAR_ADDRESS];
	char dstAddr[CHAR_ADDRESS];
	char srcInterface[CHAR_INTERFACE];
	char dstInterface[CHAR_INTERFACE];
};

struct xmlLink{
	struct structs_array list;
};

struct xmlRoot{ //janfu
	int nodes;
	struct xmlLink *xmlVector;
};

class Topology{

private:
	int n;
	struct link **adjMatrix;
	struct xmlRoot *xmlStruct; //janfu
public:
	Topology(int nodes);	//Constructor
	void InitAdjMatrix();		//Inizialization of adj matrix
	void PrintAdjMatrix();		//Print adj matrix
	void InitXmlStruct();

	//void SaveTopology();
/*Per me stessa: ricordati di mettere i free alla fine!!*/
};
