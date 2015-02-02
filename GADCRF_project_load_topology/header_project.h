/*
 * header_project.h
 *
 *  Created on: Jan 24, 2015
 * Last modify: Jan 26, 2015
 *      Author: Roberta Fumarola
 * Description: Declaration of all the structs and Topology Class.
 * 				Prototypes of ausiliarity functions for saving/loading a Topology object.
 *
 */

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
#define CHAR_ADDRESS 50
#define CHAR_INTERFACE 30
#define MAX_CAP 500

struct topologyLink{
	int capacity;
	int used;
	char *srcAddr;
	char *dstAddr;
	char *srcInterface;
	char *dstInterface;
};

struct topLink{
	struct structs_array list;
};

struct xmlRoot2{
	int nodes;
	struct topLink *xmlVector;
};

class Topology{

private:

	//Attributes for topology manipulation
	int n;
	struct topologyLink **adjMatrix;

	//Attributes for import/export topology
	struct xmlRoot2 *xmlStruct;
	struct topologyLink *l;

public:
	Topology(int nodes);	//Constructor
	~Topology();			//Destructor
	void InitAdjMatrix();	//Inizialization of adj matrix
	void PrintAdjMatrix();	//Print adj matrix
	void InitXmlStruct();	//Inizialization of xml structs
	void SaveTopology();	//Export adj matrix in XML file
	void LoadTopology(struct xmlRoot2* xmlTopology); //Load imported topology
	struct topologyLink ** Matrix();
};

//Import topology from XML file
void ImportTopology(struct xmlRoot2* xmlTopology);

int* find_path(struct topologyLink ** net, int nodes, int src, int dest, int c);
