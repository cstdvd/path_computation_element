/*
 * config_topology.cc
 *
 *  Created on: Jan 24, 2015
 * Last modify: Jan 26, 2015
 *      Author: Roberta Fumarola
 * Description: Implementation of all the methods of Topology Class.
 * 				Implementation of ausiliarity functions for saving/loading a Topology object.
 */


#include "header_project.h"
extern int simul;

/******************* BEGIN TOPOLOGY CLASS METHODS ******************************/

//Constructor
Topology::Topology(int nodes){

	int i,j;
	n = nodes;
	adjMatrix = (struct topologyLink**) calloc(n,sizeof(struct topologyLink));
	for (i=0;i<n;i++){
		adjMatrix[i] = (struct topologyLink*) calloc(n,sizeof(struct topologyLink));
	}

	for(i=0;i<n;i++){
		for(j=0;j<n;j++){
			adjMatrix[i][j].srcAddr = (char*) calloc (CHAR_ADDRESS,sizeof(char));
			adjMatrix[i][j].dstAddr = (char*) calloc (CHAR_ADDRESS,sizeof(char));
			adjMatrix[i][j].srcInterface = (char*) calloc (CHAR_INTERFACE,sizeof(char));
			adjMatrix[i][j].dstInterface = (char*) calloc (CHAR_INTERFACE,sizeof(char));
		}
	}

	loopbackArray = (struct loopback*)calloc(n,sizeof(struct loopback));
	for (i=0;i<n;i++){
		loopbackArray[i].loopAddr = (char*) calloc(CHAR_ADDRESS,sizeof(char));
	}

	xmlStruct = (struct xmlRoot2*) malloc(sizeof (struct xmlRoot2*));

	xmlStruct->nodes = nodes;

	xmlStruct->loopbackInterfaces = (struct loopbackAddr*) malloc(sizeof (struct loopbackAddr));
	xmlStruct->loopbackInterfaces->list.length = n;

	xmlStruct->xmlVector = (struct topLink*) malloc(sizeof(struct topLink));
	xmlStruct->xmlVector->list.length = n*n;

	l = (struct topologyLink*) calloc((n*n),sizeof(struct topologyLink));
	for(i=0;i<(n*n);i++){
		l[i].srcAddr = (char*) calloc (CHAR_ADDRESS,sizeof(char));
		l[i].dstAddr = (char*) calloc (CHAR_ADDRESS,sizeof(char));
		l[i].srcInterface = (char*) calloc (CHAR_INTERFACE,sizeof(char));
		l[i].dstInterface = (char*) calloc (CHAR_INTERFACE,sizeof(char));
	}

}

//Destructor
Topology::~Topology(){

	int i,j;

	for(i=0;i<(n*n);i++){
		free(l[i].srcAddr);
		free(l[i].dstAddr);
		free(l[i].srcInterface);
		free(l[i].dstInterface);
	}

	free(l);
	free(xmlStruct->xmlVector);
	free(xmlStruct->loopbackInterfaces);
	free(xmlStruct);

	for(i=0;i<n;i++){
		for(j=0;j<n;j++){
			free(adjMatrix[i][j].srcAddr);
			free(adjMatrix[i][j].dstAddr);
			free(adjMatrix[i][j].srcInterface);
			free(adjMatrix[i][j].dstInterface);
		}
	}

	for(i=0;i<n;i++){
		free(adjMatrix[i]);
	}
	free(adjMatrix);
}

void Topology::PrintAdjMatrix(){

	int i,j;

	for(i=0;i<n;i++){

			for(j=0;j<n;j++){
				printf("%d\t\t\t",adjMatrix[i][j].capacity);
			}
			printf("\n");

			for(j=0;j<n;j++){
				printf("%d\t\t\t",adjMatrix[i][j].used);
			}
			printf("\n");

			for(j=0;j<n;j++){
				if (strcmp(adjMatrix[i][j].srcAddr,"NULL")==0)
					printf("%s\t\t\t",adjMatrix[i][j].srcAddr);
				else
					printf("%s\t\t",adjMatrix[i][j].srcAddr);
			}
			printf("\n");

			for(j=0;j<n;j++){
				if(strcmp(adjMatrix[i][j].dstAddr,"NULL")==0)
					printf("%s\t\t\t",adjMatrix[i][j].dstAddr);
				else
					printf("%s\t\t",adjMatrix[i][j].dstAddr);
			}
			printf("\n");

			for(j=0;j<n;j++){
				if(strcmp(adjMatrix[i][j].srcInterface,"NULL")==0)
					printf("%s\t\t\t",adjMatrix[i][j].srcInterface);
				else
					printf("%s\t\t",adjMatrix[i][j].srcInterface);
			}
			printf("\n");

			for(j=0;j<n;j++){
				if(strcmp(adjMatrix[i][j].dstInterface,"NULL")==0)
					printf("%s\t\t\t",adjMatrix[i][j].dstInterface);
				else
					printf("%s\t\t",adjMatrix[i][j].dstInterface);
			}
			printf("\n");
			printf("------------------------------------------------------\n");

		}
}

void Topology::PrintLoopbackArray(){

	int i=0;

	for(i=0;i<n;i++){
		printf("Loopback address node %i: %s\n",i,loopbackArray[i].loopAddr);
	}
}


void Topology::InitAdjMatrix(){

	int i,j;

	//A very bad initialization because of malfunction of iostream....
	for(i=0;i<n;i++){
		for(j=0;j<n;j++){
			strcpy(adjMatrix[i][j].srcAddr,"NULL");
			strcpy(adjMatrix[i][j].dstAddr,"NULL");
			strcpy(adjMatrix[i][j].srcInterface,"NULL");
			strcpy(adjMatrix[i][j].dstInterface,"NULL");
			adjMatrix[i][j].capacity = -1;
			adjMatrix[i][j].used = 10;
		}
	}

	if(simul==0){
		//From R1 to R2
		strcpy(adjMatrix[0][1].srcAddr,"10.1.1.1");
		strcpy(adjMatrix[0][1].dstAddr,"10.1.1.2");
		strcpy(adjMatrix[0][1].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[0][1].dstInterface,"Ethernet1/0");
		adjMatrix[0][1].capacity = 1024;
		adjMatrix[0][1].used = 0;

		//From R1 to R3
		strcpy(adjMatrix[0][2].srcAddr,"10.2.2.1");
		strcpy(adjMatrix[0][2].dstAddr,"10.2.2.2");
		strcpy(adjMatrix[0][2].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[0][2].dstInterface,"Ethernet1/1");
		adjMatrix[0][2].capacity = 1024;
		adjMatrix[0][2].used = 0;

		//From R2 to R1
		strcpy(adjMatrix[1][0].srcAddr,"10.1.1.2");
		strcpy(adjMatrix[1][0].dstAddr,"10.1.1.1");
		strcpy(adjMatrix[1][0].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[1][0].dstInterface,"Ethernet1/0");
		adjMatrix[1][0].capacity = 1024;
		adjMatrix[1][0].used = 0;

		//From R2 to R5
		strcpy(adjMatrix[1][4].srcAddr,"10.5.5.1");
		strcpy(adjMatrix[1][4].dstAddr,"10.5.5.2");
		strcpy(adjMatrix[1][4].srcInterface,"Ethernet1/2");
		strcpy(adjMatrix[1][4].dstInterface,"Ethernet1/2");
		adjMatrix[1][4].capacity = 1024;
		adjMatrix[1][4].used = 0;

		//From R3 to R1
		strcpy(adjMatrix[2][0].srcAddr,"10.2.2.2");
		strcpy(adjMatrix[2][0].dstAddr,"10.2.2.1");
		strcpy(adjMatrix[2][0].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[2][0].dstInterface,"Ethernet1/1");
		adjMatrix[2][0].capacity = 1024;
		adjMatrix[2][0].used = 0;

		//From R3 to R4
		strcpy(adjMatrix[2][3].srcAddr,"10.3.3.1");
		strcpy(adjMatrix[2][3].dstAddr,"10.3.3.2");
		strcpy(adjMatrix[2][3].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[2][3].dstInterface,"Ethernet1/0");
		adjMatrix[2][3].capacity = 1024;
		adjMatrix[2][3].used = 0;

		//From R4 to R3
		strcpy(adjMatrix[3][2].srcAddr,"10.3.3.2");
		strcpy(adjMatrix[3][2].dstAddr,"10.3.3.1");
		strcpy(adjMatrix[3][2].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[3][2].dstInterface,"Ethernet1/0");
		adjMatrix[3][2].capacity = 1024;
		adjMatrix[3][2].used = 0;

		//From R4 to R5
		strcpy(adjMatrix[3][4].srcAddr,"10.4.4.1");
		strcpy(adjMatrix[3][4].dstAddr,"10.4.4.2");
		strcpy(adjMatrix[3][4].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[3][4].dstInterface,"Ethernet1/1");
		adjMatrix[3][4].capacity = 1024;
		adjMatrix[3][4].used = 0;

		//From R5 to R2
		strcpy(adjMatrix[4][1].srcAddr,"10.5.5.2");
		strcpy(adjMatrix[4][1].dstAddr,"10.5.5.1");
		strcpy(adjMatrix[4][1].srcInterface,"Ethernet1/2");
		strcpy(adjMatrix[4][1].dstInterface,"Ethernet1/2");
		adjMatrix[4][1].capacity = 1024;
		adjMatrix[4][1].used = 0;

		//From R5 to R4
		strcpy(adjMatrix[4][3].srcAddr,"10.4.4.2");
		strcpy(adjMatrix[4][3].dstAddr,"10.4.4.1");
		strcpy(adjMatrix[4][3].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[4][3].dstInterface,"Ethernet1/1");
		adjMatrix[4][3].capacity = 1024;
		adjMatrix[4][3].used = 0;
	}
	else{
		//From R1 to R2
		strcpy(adjMatrix[0][1].srcAddr,"10.1.1.1");
		strcpy(adjMatrix[0][1].dstAddr,"10.1.1.2");
		strcpy(adjMatrix[0][1].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[0][1].dstInterface,"Ethernet1/0");
		adjMatrix[0][1].capacity = 1024;
		adjMatrix[0][1].used = 10;

		//From R1 to R6
		strcpy(adjMatrix[0][5].srcAddr,"10.2.2.1");
		strcpy(adjMatrix[0][5].dstAddr,"10.2.2.2");
		strcpy(adjMatrix[0][5].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[0][5].dstInterface,"Ethernet0/0");
		adjMatrix[0][5].capacity = 1024;
		adjMatrix[0][5].used = 10;

		//From R2 to R1
		strcpy(adjMatrix[1][0].srcAddr,"10.1.1.2");
		strcpy(adjMatrix[1][0].dstAddr,"10.1.1.1");
		strcpy(adjMatrix[1][0].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[1][0].dstInterface,"Ethernet0/0");
		adjMatrix[1][0].capacity = 1024;
		adjMatrix[1][0].used = 10;

		//From R2 to R5
		strcpy(adjMatrix[1][4].srcAddr,"10.4.4.1");
		strcpy(adjMatrix[1][4].dstAddr,"10.4.4.2");
		strcpy(adjMatrix[1][4].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[1][4].dstInterface,"Ethernet0/0");
		adjMatrix[1][4].capacity = 1024;
		adjMatrix[1][4].used = 10;

		//From R2 to R3
		strcpy(adjMatrix[1][2].srcAddr,"10.3.3.1");
		strcpy(adjMatrix[1][2].dstAddr,"10.3.3.2");
		strcpy(adjMatrix[1][2].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[1][2].dstInterface,"Ethernet1/0");
		adjMatrix[1][2].capacity = 1024;
		adjMatrix[1][2].used = 10;

		//From R3 to R2
		strcpy(adjMatrix[2][1].srcAddr,"10.3.3.2");
		strcpy(adjMatrix[2][1].dstAddr,"10.3.3.1");
		strcpy(adjMatrix[2][1].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[2][1].dstInterface,"Ethernet1/1");
		adjMatrix[2][1].capacity = 1024;
		adjMatrix[2][1].used = 10;

		//From R3 to R4
		strcpy(adjMatrix[2][3].srcAddr,"10.7.7.1");
		strcpy(adjMatrix[2][3].dstAddr,"10.7.7.2");
		strcpy(adjMatrix[2][3].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[2][3].dstInterface,"Ethernet0/0");
		adjMatrix[2][3].capacity = 1024;
		adjMatrix[2][3].used = 10;

		//From R4 to R3
		strcpy(adjMatrix[3][2].srcAddr,"10.7.7.2");
		strcpy(adjMatrix[3][2].dstAddr,"10.7.7.1");
		strcpy(adjMatrix[3][2].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[3][2].dstInterface,"Ethernet0/0");
		adjMatrix[3][2].capacity = 1024;
		adjMatrix[3][2].used = 10;

		//From R4 to R5
		strcpy(adjMatrix[3][4].srcAddr,"10.8.8.2");
		strcpy(adjMatrix[3][4].dstAddr,"10.8.8.1");
		strcpy(adjMatrix[3][4].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[3][4].dstInterface,"Ethernet1/2");
		adjMatrix[3][4].capacity = 1024;
		adjMatrix[3][4].used = 10;

		//From R4 to R8
		strcpy(adjMatrix[3][7].srcAddr,"10.13.13.2");
		strcpy(adjMatrix[3][7].dstAddr,"10.13.13.1");
		strcpy(adjMatrix[3][7].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[3][7].dstInterface,"Ethernet1/1");
		adjMatrix[3][7].capacity = 1024;
		adjMatrix[3][7].used = 10;

		//From R5 to R2
		strcpy(adjMatrix[4][1].srcAddr,"10.4.4.2");
		strcpy(adjMatrix[4][1].dstAddr,"10.4.4.1");
		strcpy(adjMatrix[4][1].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[4][1].dstInterface,"Ethernet0/0");
		adjMatrix[4][1].capacity = 1024;
		adjMatrix[4][1].used = 10;

		//From R5 to R4
		strcpy(adjMatrix[4][3].srcAddr,"10.8.8.1");
		strcpy(adjMatrix[4][3].dstAddr,"10.8.8.2");
		strcpy(adjMatrix[4][3].srcInterface,"Ethernet1/2");
		strcpy(adjMatrix[4][3].dstInterface,"Ethernet1/0");
		adjMatrix[4][3].capacity = 1024;
		adjMatrix[4][3].used = 10;

		//From R5 to R6
		strcpy(adjMatrix[4][5].srcAddr,"10.6.6.2");
		strcpy(adjMatrix[4][5].dstAddr,"10.6.6.1");
		strcpy(adjMatrix[4][5].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[4][5].dstInterface,"Ethernet1/4");
		adjMatrix[4][5].capacity = 1024;
		adjMatrix[4][5].used = 10;

		//From R5 to R8
		strcpy(adjMatrix[4][7].srcAddr,"10.9.9.1");
		strcpy(adjMatrix[4][7].dstAddr,"10.9.9.2");
		strcpy(adjMatrix[4][7].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[4][7].dstInterface,"Ethernet0/0");
		adjMatrix[4][7].capacity = 1024;
		adjMatrix[4][7].used = 10;

		//From R6 to R1
		strcpy(adjMatrix[5][0].srcAddr,"10.2.2.2");
		strcpy(adjMatrix[5][0].dstAddr,"10.2.2.1");
		strcpy(adjMatrix[5][0].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[5][0].dstInterface,"Ethernet1/0");
		adjMatrix[5][0].capacity = 1024;
		adjMatrix[5][0].used = 10;

		//From R6 to R5
		strcpy(adjMatrix[5][4].srcAddr,"10.6.6.1");
		strcpy(adjMatrix[5][4].dstAddr,"10.6.6.2");
		strcpy(adjMatrix[5][4].srcInterface,"Ethernet1/4");
		strcpy(adjMatrix[5][4].dstInterface,"Ethernet1/0");
		adjMatrix[5][4].capacity = 1024;
		adjMatrix[5][4].used = 10;

		//From R6 to R7
		strcpy(adjMatrix[5][6].srcAddr,"10.5.5.2");
		strcpy(adjMatrix[5][6].dstAddr,"10.5.5.1");
		strcpy(adjMatrix[5][6].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[5][6].dstInterface,"Ethernet0/0");
		adjMatrix[5][6].capacity = 1024;
		adjMatrix[5][6].used = 10;

		//From R6 to R8
		strcpy(adjMatrix[5][7].srcAddr,"10.10.10.1");
		strcpy(adjMatrix[5][7].dstAddr,"10.10.10.2");
		strcpy(adjMatrix[5][7].srcInterface,"Ethernet1/3");
		strcpy(adjMatrix[5][7].dstInterface,"Ethernet1/0");
		adjMatrix[5][7].capacity = 1024;
		adjMatrix[5][7].used = 10;

		//From R6 to R10
		strcpy(adjMatrix[5][9].srcAddr,"10.11.11.1");
		strcpy(adjMatrix[5][9].dstAddr,"10.11.11.2");
		strcpy(adjMatrix[5][9].srcInterface,"Ethernet1/2");
		strcpy(adjMatrix[5][9].dstInterface,"Ethernet1/0");
		adjMatrix[5][9].capacity = 1024;
		adjMatrix[5][9].used = 10;

		//From R6 to R11
		strcpy(adjMatrix[5][10].srcAddr,"10.14.14.1");
		strcpy(adjMatrix[5][10].dstAddr,"10.14.14.2");
		strcpy(adjMatrix[5][10].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[5][10].dstInterface,"Ethernet0/0");
		adjMatrix[5][10].capacity = 1024;
		adjMatrix[5][10].used = 10;

		//From R7 to R6
		strcpy(adjMatrix[6][5].srcAddr,"10.5.5.1");
		strcpy(adjMatrix[6][5].dstAddr,"10.5.5.2");
		strcpy(adjMatrix[6][5].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[6][5].dstInterface,"Ethernet1/0");
		adjMatrix[6][5].capacity = 1024;
		adjMatrix[6][5].used = 10;

		//From R7 to R11
		strcpy(adjMatrix[6][10].srcAddr,"10.29.29.1");
		strcpy(adjMatrix[6][10].dstAddr,"10.29.29.2");
		strcpy(adjMatrix[6][10].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[6][10].dstInterface,"Ethernet1/3");
		adjMatrix[6][10].capacity = 1024;
		adjMatrix[6][10].used = 10;

		//From R8 to R4
		strcpy(adjMatrix[7][3].srcAddr,"10.13.13.1");
		strcpy(adjMatrix[7][3].dstAddr,"10.13.13.2");
		strcpy(adjMatrix[7][3].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[7][3].dstInterface,"Ethernet1/1");
		adjMatrix[7][3].capacity = 1024;
		adjMatrix[7][3].used = 10;

		//From R8 to R5
		strcpy(adjMatrix[7][4].srcAddr,"10.9.9.2");
		strcpy(adjMatrix[7][4].dstAddr,"10.9.9.1");
		strcpy(adjMatrix[7][4].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[7][4].dstInterface,"Ethernet1/1");
		adjMatrix[7][4].capacity = 1024;
		adjMatrix[7][4].used = 10;

		//From R8 to R6
		strcpy(adjMatrix[7][5].srcAddr,"10.10.10.2");
		strcpy(adjMatrix[7][5].dstAddr,"10.10.10.1");
		strcpy(adjMatrix[7][5].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[7][5].dstInterface,"Ethernet1/3");
		adjMatrix[7][5].capacity = 1024;
		adjMatrix[7][5].used = 10;

		//From R8 to R10
		strcpy(adjMatrix[7][9].srcAddr,"10.12.12.2");
		strcpy(adjMatrix[7][9].dstAddr,"10.12.12.1");
		strcpy(adjMatrix[7][9].srcInterface,"Ethernet1/2");
		strcpy(adjMatrix[7][9].dstInterface,"Ethernet0/0");
		adjMatrix[7][9].capacity = 1024;
		adjMatrix[7][9].used = 10;

		//From R9 to R10
		strcpy(adjMatrix[8][9].srcAddr,"10.30.30.2");
		strcpy(adjMatrix[8][9].dstAddr,"10.30.30.1");
		strcpy(adjMatrix[8][9].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[8][9].dstInterface,"Ethernet1/1");
		adjMatrix[8][9].capacity = 1024;
		adjMatrix[8][9].used = 10;

		//From R9 to R17
		strcpy(adjMatrix[8][16].srcAddr,"10.21.21.1");
		strcpy(adjMatrix[8][16].dstAddr,"10.21.21.2");
		strcpy(adjMatrix[8][16].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[8][16].dstInterface,"Ethernet0/0");
		adjMatrix[8][16].capacity = 1024;
		adjMatrix[8][16].used = 10;

		//From R9 to R18
		strcpy(adjMatrix[8][17].srcAddr,"10.23.23.1");
		strcpy(adjMatrix[8][17].dstAddr,"10.23.23.2");
		strcpy(adjMatrix[8][17].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[8][17].dstInterface,"Ethernet1/0");
		adjMatrix[8][17].capacity = 1024;
		adjMatrix[8][17].used = 10;

		//From R10 to R6
		strcpy(adjMatrix[9][5].srcAddr,"10.11.11.2");
		strcpy(adjMatrix[9][5].dstAddr,"10.11.11.1");
		strcpy(adjMatrix[9][5].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[9][5].dstInterface,"Ethernet1/2");
		adjMatrix[9][5].capacity = 1024;
		adjMatrix[9][5].used = 10;

		//From R10 to R8
		strcpy(adjMatrix[9][7].srcAddr,"10.12.12.2");
		strcpy(adjMatrix[9][7].dstAddr,"10.12.12.1");
		strcpy(adjMatrix[9][7].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[9][7].dstInterface,"Ethernet1/2");
		adjMatrix[9][7].capacity = 1024;
		adjMatrix[9][7].used = 10;

		//From R10 to R9
		strcpy(adjMatrix[9][8].srcAddr,"10.30.30.1");
		strcpy(adjMatrix[9][8].dstAddr,"10.30.30.2");
		strcpy(adjMatrix[9][8].srcInterface,"Ethernet1/4");
		strcpy(adjMatrix[9][8].dstInterface,"Ethernet0/0");
		adjMatrix[9][8].capacity = 1024;
		adjMatrix[9][8].used = 10;

		//From R10 to R11
		strcpy(adjMatrix[9][10].srcAddr,"10.15.15.2");
		strcpy(adjMatrix[9][10].dstAddr,"10.15.15.1");
		strcpy(adjMatrix[9][10].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[9][10].dstInterface,"Ethernet1/0");
		adjMatrix[9][10].capacity = 1024;
		adjMatrix[9][10].used = 10;

		//From R10 to R14
		strcpy(adjMatrix[9][13].srcAddr,"10.20.20.1");
		strcpy(adjMatrix[9][13].dstAddr,"10.20.20.2");
		strcpy(adjMatrix[9][13].srcInterface,"Ethernet1/2");
		strcpy(adjMatrix[9][13].dstInterface,"Ethernet1/2");
		adjMatrix[9][13].capacity = 1024;
		adjMatrix[9][13].used = 10;

		//From R10 to R18
		strcpy(adjMatrix[9][17].srcAddr,"10.24.24.1");
		strcpy(adjMatrix[9][17].dstAddr,"10.24.24.2");
		strcpy(adjMatrix[9][17].srcInterface,"Ethernet1/3");
		strcpy(adjMatrix[9][17].dstInterface,"Ethernet1/1");
		adjMatrix[9][17].capacity = 1024;
		adjMatrix[9][17].used = 10;

		//From R11 to R6
		strcpy(adjMatrix[10][5].srcAddr,"10.14.14.2");
		strcpy(adjMatrix[10][5].dstAddr,"10.14.14.1");
		strcpy(adjMatrix[10][5].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[10][5].dstInterface,"Ethernet1/1");
		adjMatrix[10][5].capacity = 1024;
		adjMatrix[10][5].used = 10;

		//From R11 to R7
		strcpy(adjMatrix[10][6].srcAddr,"10.29.29.2");
		strcpy(adjMatrix[10][6].dstAddr,"10.29.29.1");
		strcpy(adjMatrix[10][6].srcInterface,"Ethernet1/3");
		strcpy(adjMatrix[10][6].dstInterface,"Ethernet1/0");
		adjMatrix[10][6].capacity = 1024;
		adjMatrix[10][6].used = 10;

		//From R11 to R10
		strcpy(adjMatrix[10][9].srcAddr,"10.15.15.1");
		strcpy(adjMatrix[10][9].dstAddr,"10.15.15.2");
		strcpy(adjMatrix[10][9].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[10][9].dstInterface,"Ethernet1/1");
		adjMatrix[10][9].capacity = 1024;
		adjMatrix[10][9].used = 10;

		//From R11 to R12
		strcpy(adjMatrix[10][11].srcAddr,"10.16.16.1");
		strcpy(adjMatrix[10][11].dstAddr,"10.16.16.2");
		strcpy(adjMatrix[10][11].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[10][11].dstInterface,"Ethernet0/0");
		adjMatrix[10][11].capacity = 1024;
		adjMatrix[10][11].used = 10;

		//From R11 to R14
		strcpy(adjMatrix[10][13].srcAddr,"10.17.17.1");
		strcpy(adjMatrix[10][13].dstAddr,"10.17.17.2");
		strcpy(adjMatrix[10][13].srcInterface,"Ethernet1/2");
		strcpy(adjMatrix[10][13].dstInterface,"Ethernet0/0");
		adjMatrix[10][13].capacity = 1024;
		adjMatrix[10][13].used = 10;

		//From R12 to R12
		strcpy(adjMatrix[11][10].srcAddr,"10.16.16.2");
		strcpy(adjMatrix[11][10].dstAddr,"10.16.16.1");
		strcpy(adjMatrix[11][10].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[11][10].dstInterface,"Ethernet1/1");
		adjMatrix[11][10].capacity = 1024;
		adjMatrix[11][10].used = 10;

		//From R12 to R13
		strcpy(adjMatrix[11][12].srcAddr,"10.18.18.1");
		strcpy(adjMatrix[11][12].dstAddr,"10.18.18.2");
		strcpy(adjMatrix[11][12].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[11][12].dstInterface,"Ethernet0/0");
		adjMatrix[11][12].capacity = 1024;
		adjMatrix[11][12].used = 10;

		//From R13 to R12
		strcpy(adjMatrix[12][11].srcAddr,"10.18.18.2");
		strcpy(adjMatrix[12][11].dstAddr,"10.18.18.1");
		strcpy(adjMatrix[12][11].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[12][11].dstInterface,"Ethernet1/0");
		adjMatrix[12][11].capacity = 1024;
		adjMatrix[12][11].used = 10;

		//From R13 to R14
		strcpy(adjMatrix[12][13].srcAddr,"10.19.19.2");
		strcpy(adjMatrix[12][13].dstAddr,"10.19.19.1");
		strcpy(adjMatrix[12][13].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[12][13].dstInterface,"Ethernet1/0");
		adjMatrix[12][13].capacity = 1024;
		adjMatrix[12][13].used = 10;

		//From R14 to R10
		strcpy(adjMatrix[13][9].srcAddr,"10.20.20.2");
		strcpy(adjMatrix[13][9].dstAddr,"10.20.20.1");
		strcpy(adjMatrix[13][9].srcInterface,"Ethernet1/2");
		strcpy(adjMatrix[13][9].dstInterface,"Ethernet1/2");
		adjMatrix[13][9].capacity = 1024;
		adjMatrix[13][9].used = 10;

		//From R14 to R11
		strcpy(adjMatrix[13][10].srcAddr,"10.17.17.2");
		strcpy(adjMatrix[13][10].dstAddr,"10.17.17.1");
		strcpy(adjMatrix[13][10].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[13][10].dstInterface,"Ethernet1/2");
		adjMatrix[13][10].capacity = 1024;
		adjMatrix[13][10].used = 10;

		//From R14 to R13
		strcpy(adjMatrix[13][12].srcAddr,"10.19.19.1");
		strcpy(adjMatrix[13][12].dstAddr,"10.19.19.2");
		strcpy(adjMatrix[13][12].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[13][12].dstInterface,"Ethernet1/0");
		adjMatrix[13][12].capacity = 1024;
		adjMatrix[13][12].used = 10;

		//From R14 to R15
		strcpy(adjMatrix[13][14].srcAddr,"10.28.28.1");
		strcpy(adjMatrix[13][14].dstAddr,"10.28.28.2");
		strcpy(adjMatrix[13][14].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[13][14].dstInterface,"Ethernet0/0");
		adjMatrix[13][14].capacity = 1024;
		adjMatrix[13][14].used = 10;

		//From R14 to R16
		strcpy(adjMatrix[13][15].srcAddr,"10.25.25.2");
		strcpy(adjMatrix[13][15].dstAddr,"10.25.25.1");
		strcpy(adjMatrix[13][15].srcInterface,"Ethernet1/3");
		strcpy(adjMatrix[13][15].dstInterface,"Ethernet1/0");
		adjMatrix[13][15].capacity = 1024;
		adjMatrix[13][15].used = 10;

		//From R15 to R14
		strcpy(adjMatrix[14][13].srcAddr,"10.28.28.2");
		strcpy(adjMatrix[14][13].dstAddr,"10.28.28.1");
		strcpy(adjMatrix[14][13].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[14][13].dstInterface,"Ethernet1/1");
		adjMatrix[14][13].capacity = 1024;
		adjMatrix[14][13].used = 10;

		//From R15 to R16
		strcpy(adjMatrix[14][15].srcAddr,"10.27.27.2");
		strcpy(adjMatrix[14][15].dstAddr,"10.27.27.1");
		strcpy(adjMatrix[14][15].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[14][15].dstInterface,"Ethernet1/1");
		adjMatrix[14][15].capacity = 1024;
		adjMatrix[14][15].used = 10;

		//From R16 to R14
		strcpy(adjMatrix[15][13].srcAddr,"10.25.25.2");
		strcpy(adjMatrix[15][13].dstAddr,"10.25.25.1");
		strcpy(adjMatrix[15][13].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[15][13].dstInterface,"Ethernet1/3");
		adjMatrix[15][13].capacity = 1024;
		adjMatrix[15][13].used = 10;

		//From R16 to R15
		strcpy(adjMatrix[15][14].srcAddr,"10.27.27.2");
		strcpy(adjMatrix[15][14].dstAddr,"10.27.27.1");
		strcpy(adjMatrix[15][14].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[15][14].dstInterface,"Ethernet1/0");
		adjMatrix[15][14].capacity = 1024;
		adjMatrix[15][14].used = 10;

		//From R16 to R18
		strcpy(adjMatrix[15][17].srcAddr,"10.26.26.1");
		strcpy(adjMatrix[15][17].dstAddr,"10.26.26.2");
		strcpy(adjMatrix[15][17].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[15][17].dstInterface,"Ethernet1/1");
		adjMatrix[15][17].capacity = 1024;
		adjMatrix[15][17].used = 10;

		//From R17 to R9
		strcpy(adjMatrix[16][8].srcAddr,"10.21.21.2");
		strcpy(adjMatrix[16][8].dstAddr,"10.21.21.1");
		strcpy(adjMatrix[16][8].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[16][8].dstInterface,"Ethernet1/1");
		adjMatrix[16][8].capacity = 1024;
		adjMatrix[16][8].used = 10;

		//From R17 to R18
		strcpy(adjMatrix[16][17].srcAddr,"10.22.22.2");
		strcpy(adjMatrix[16][17].dstAddr,"10.22.22.1");
		strcpy(adjMatrix[16][17].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[16][17].dstInterface,"Ethernet1/2");
		adjMatrix[16][17].capacity = 1024;
		adjMatrix[16][17].used = 10;

		//From R18 to R9
		strcpy(adjMatrix[17][8].srcAddr,"10.23.23.2");
		strcpy(adjMatrix[17][8].dstAddr,"10.23.23.1");
		strcpy(adjMatrix[17][8].srcInterface,"Ethernet1/0");
		strcpy(adjMatrix[17][8].dstInterface,"Ethernet1/0");
		adjMatrix[17][8].capacity = 1024;
		adjMatrix[17][8].used = 10;

		//From R18 to R10
		strcpy(adjMatrix[17][9].srcAddr,"10.24.24.2");
		strcpy(adjMatrix[17][9].dstAddr,"10.24.24.1");
		strcpy(adjMatrix[17][9].srcInterface,"Ethernet1/1");
		strcpy(adjMatrix[17][9].dstInterface,"Ethernet1/3");
		adjMatrix[17][9].capacity = 1024;
		adjMatrix[17][9].used = 10;

		//From R18 to R16
		strcpy(adjMatrix[17][15].srcAddr,"10.26.26.2");
		strcpy(adjMatrix[17][15].dstAddr,"10.26.26.1");
		strcpy(adjMatrix[17][15].srcInterface,"Ethernet0/0");
		strcpy(adjMatrix[17][15].dstInterface,"Ethernet0/0");
		adjMatrix[17][15].capacity = 1024;
		adjMatrix[17][15].used = 10;

		//From R18 to R17
		strcpy(adjMatrix[17][16].srcAddr,"10.22.22.2");
		strcpy(adjMatrix[17][16].dstAddr,"10.22.22.1");
		strcpy(adjMatrix[17][16].srcInterface,"Ethernet1/2");
		strcpy(adjMatrix[17][16].dstInterface,"Ethernet1/0");
		adjMatrix[17][16].capacity = 1024;
		adjMatrix[17][16].used = 10;
	}
}

void Topology::InitLoopbackAddresses(){

	if(simul==0){
		strcpy(loopbackArray[0].loopAddr,"172.16.1.1");
		strcpy(loopbackArray[1].loopAddr,"172.16.1.2");
		strcpy(loopbackArray[2].loopAddr,"172.16.1.3");
		strcpy(loopbackArray[3].loopAddr,"172.16.1.4");
		strcpy(loopbackArray[4].loopAddr,"172.16.1.5");
	}
	else{
		int i=0;
		char j[3];
		for(i=0;i<n;i++){
			sprintf(j,"%i",i+1);
			strcpy(loopbackArray[i].loopAddr,"172.16.1.");
			strcat(loopbackArray[i].loopAddr,j);
			printf("loopbackArray[%i]= %s\n",i,loopbackArray[i].loopAddr);
		}
	}

}

void Topology::InitXmlStruct(){

	int i,j,k,t,c;
	k=0;
	t=0;
	c=0;

	for(i=0;i<n;i++){

		for(j=0;j<n;j++){

			k=i+j+t;


			l[k].capacity = adjMatrix[i][j].capacity;
			//printf("l[%d].capacity= %d\n",k,l[k].capacity);
			l[k].used = adjMatrix[i][j].used;
			//printf("l[%d].used= %d\n",k,l[k].used);
			strcpy(l[k].srcAddr,adjMatrix[i][j].srcAddr);
			//printf("l[%d].srcAddr= %s\n",k,l[k].srcAddr);
			strcpy(l[k].dstAddr,adjMatrix[i][j].dstAddr);
			//printf("l[%d].dstAddr= %s\n",k,l[k].dstAddr);
			strcpy(l[k].srcInterface,adjMatrix[i][j].srcInterface);
			//printf("l[%d].srcInterface= %s\n",k,l[k].srcInterface);
			strcpy(l[k].dstInterface,adjMatrix[i][j].dstInterface);
			//printf("l[%d].dstInterface= %s\n",k,l[k].dstInterface);

		}
		t=k-c;
		c++;
	}
	xmlStruct->xmlVector->list.elems = l;
	xmlStruct->loopbackInterfaces->list.elems = loopbackArray;
}

void Topology::SaveTopology(){

	// Descriptor for 'struct topologyLink'
	static const struct structs_field topologyLink_fields[] = {
			STRUCTS_STRUCT_FIELD(topologyLink, capacity, &structs_type_int),
			STRUCTS_STRUCT_FIELD(topologyLink, used, &structs_type_int),
			STRUCTS_STRUCT_FIELD(topologyLink, srcAddr, &structs_type_string),
			STRUCTS_STRUCT_FIELD(topologyLink, dstAddr, &structs_type_string),
			STRUCTS_STRUCT_FIELD(topologyLink, srcInterface, &structs_type_string),
			STRUCTS_STRUCT_FIELD(topologyLink, dstInterface, &structs_type_string),
			STRUCTS_STRUCT_FIELD_END
	};

	static const struct structs_type topology_type =
			STRUCTS_STRUCT_TYPE(topologyLink, &topologyLink_fields);

	// Descriptor for field 'list' in 'struct topLink': array of strings
	static const struct structs_type list_array_type =
			STRUCTS_ARRAY_TYPE(&topology_type, "link", "link");

	// Descriptor for 'struct topLink'
	static const struct structs_field topLink_fields[] = {
		    STRUCTS_STRUCT_FIELD(topLink, list, &list_array_type),
		    STRUCTS_STRUCT_FIELD_END
	};

	static const struct structs_type topLink_type =
			STRUCTS_STRUCT_TYPE(topLink, &topLink_fields);

	// Descriptor for a variable of type 'struct topLink *'
	static const struct structs_type topLink_prt_type =
			STRUCTS_POINTER_TYPE(&topLink_type, "struct topLink");


	// Descriptor for field 'list' in 'struct loopbackAddr': array of strings
	static const struct structs_type loop_array_type =
			STRUCTS_ARRAY_TYPE(&structs_type_string, "loopbackAddr", "loopbackAddr");

	// Descriptor for 'struct loopbackAddr'
	static const struct structs_field loop_fields[] = {
			STRUCTS_STRUCT_FIELD(loopbackAddr, list, &loop_array_type),
			STRUCTS_STRUCT_FIELD_END
	};

	static const struct structs_type loop_type =
			STRUCTS_STRUCT_TYPE(loopbackAddr, &loop_fields);

	// Descriptor for a variable of type 'struct loopbackAddr *'
	static const struct structs_type loop_prt_type =
			STRUCTS_POINTER_TYPE(&loop_type, "struct loopbackAddr");

	// Descriptor for 'struct xmlRoot2'
	static const struct structs_field xmlRoot_fields[] = {
		    STRUCTS_STRUCT_FIELD(xmlRoot2, nodes, &structs_type_int),
		    STRUCTS_STRUCT_FIELD(xmlRoot2, xmlVector, &topLink_prt_type),
			STRUCTS_STRUCT_FIELD(xmlRoot2, loopbackInterfaces, &loop_prt_type),
		    STRUCTS_STRUCT_FIELD_END
	};

	static const struct structs_type xmlRoot_type =
			STRUCTS_STRUCT_TYPE(xmlRoot2, &xmlRoot_fields);

	FILE *Ptr;

	if(simul==0){
		if((Ptr=fopen("topology_xml","w"))==NULL){

			printf("errore apertura topology_xml\n");
		}
		else{
			structs_xml_output(&xmlRoot_type, "Topology", NULL, xmlStruct, Ptr, NULL, 0);
		}
	}
	else{
		if((Ptr=fopen("topology_xml_simul","w"))==NULL){

			printf("errore apertura topology_xml_simul\n");
		}
		else{
			structs_xml_output(&xmlRoot_type, "Topology", NULL, xmlStruct, Ptr, NULL, 0);
		}
	}

	fclose(Ptr);
}

void Topology::LoadTopology(struct xmlRoot2* xmlTopology){

	int i,j,k,t,c;
	k=0;
	t=0;
	c=0;

	l = (struct topologyLink*) xmlTopology->xmlVector->list.elems;
	xmlStruct = xmlTopology;

	for(i=0;i<n;i++){

		for(j=0;j<n;j++){

			k=i+j+t;

			adjMatrix[i][j].capacity = l[k].capacity;
			adjMatrix[i][j].used = l[k].used;
			strcpy(adjMatrix[i][j].srcAddr,l[k].srcAddr);
			strcpy(adjMatrix[i][j].dstAddr,l[k].dstAddr);
			strcpy(adjMatrix[i][j].srcInterface,l[k].srcInterface);
			strcpy(adjMatrix[i][j].dstInterface,l[k].dstInterface);
		}
		t=k-c;
		c++;
	}

	loopbackArray = (struct loopback *) xmlStruct->loopbackInterfaces->list.elems;
}

struct topologyLink ** Topology::Matrix()
{
	return adjMatrix;
}

struct loopback * Topology::LoopArray(){
	return loopbackArray;
}

bool Topology::UpdateTopology(int *path,int len,int c){
	int i;
	for(i=0;i<(len-1);i++){
		adjMatrix[path[i]][path[i+1]].used+=c;
	}
	return true;
}

/******************* END TOPOLOGY CLASS METHODS ******************************/

/******************* BEGIN AUSILIARITY FUNCTIONS *****************************/

void ImportTopology(struct xmlRoot2* xmlTopology){

	// Descriptor for 'struct topologyLink'
		static const struct structs_field topologyLink_fields[] = {
				STRUCTS_STRUCT_FIELD(topologyLink, capacity, &structs_type_int),
				STRUCTS_STRUCT_FIELD(topologyLink, used, &structs_type_int),
				STRUCTS_STRUCT_FIELD(topologyLink, srcAddr, &structs_type_string),
				STRUCTS_STRUCT_FIELD(topologyLink, dstAddr, &structs_type_string),
				STRUCTS_STRUCT_FIELD(topologyLink, srcInterface, &structs_type_string),
				STRUCTS_STRUCT_FIELD(topologyLink, dstInterface, &structs_type_string),
				STRUCTS_STRUCT_FIELD_END
		};

		static const struct structs_type topology_type =
				STRUCTS_STRUCT_TYPE(topologyLink, &topologyLink_fields);

		// Descriptor for field 'list' in 'struct topLink': array of strings
		static const struct structs_type list_array_type =
				STRUCTS_ARRAY_TYPE(&topology_type, "link", "link");

		// Descriptor for 'struct topLink'
		static const struct structs_field topLink_fields[] = {
			    STRUCTS_STRUCT_FIELD(topLink, list, &list_array_type),
			    STRUCTS_STRUCT_FIELD_END
		};

		static const struct structs_type topLink_type =
				STRUCTS_STRUCT_TYPE(topLink, &topLink_fields);

		// Descriptor for a variable of type 'struct topLink *'
		static const struct structs_type topLink_prt_type =
				STRUCTS_POINTER_TYPE(&topLink_type, "struct topLink");


		// Descriptor for field 'list' in 'struct loopbackAddr': array of strings
		static const struct structs_type loop_array_type =
				STRUCTS_ARRAY_TYPE(&structs_type_string, "loopbackAddr", "loopbackAddr");

		// Descriptor for 'struct loopbackAddr'
		static const struct structs_field loop_fields[] = {
				STRUCTS_STRUCT_FIELD(loopbackAddr, list, &loop_array_type),
				STRUCTS_STRUCT_FIELD_END
		};

		static const struct structs_type loop_type =
				STRUCTS_STRUCT_TYPE(loopbackAddr, &loop_fields);

		// Descriptor for a variable of type 'struct loopbackAddr *'
		static const struct structs_type loop_prt_type =
				STRUCTS_POINTER_TYPE(&loop_type, "struct loopbackAddr");

		// Descriptor for 'struct xmlRoot2'
		static const struct structs_field xmlRoot_fields[] = {
			    STRUCTS_STRUCT_FIELD(xmlRoot2, nodes, &structs_type_int),
			    STRUCTS_STRUCT_FIELD(xmlRoot2, xmlVector, &topLink_prt_type),
				STRUCTS_STRUCT_FIELD(xmlRoot2, loopbackInterfaces, &loop_prt_type),
			    STRUCTS_STRUCT_FIELD_END
		};

		static const struct structs_type xmlRoot_type =
				STRUCTS_STRUCT_TYPE(xmlRoot2, &xmlRoot_fields);

	FILE *Ptr;

	if(simul==0){
		if((Ptr=fopen("topology_xml","r"))==NULL){

			printf("errore apertura topology_xml\n");
		}
		else{
			structs_xml_input(&xmlRoot_type, "Topology", NULL,NULL, Ptr, xmlTopology, STRUCTS_XML_UNINIT, NULL);
		}
	}
	else{
		if((Ptr=fopen("topology_xml_simul","r"))==NULL){

			printf("errore apertura topology_xml_simul\n");
		}
		else{
			structs_xml_input(&xmlRoot_type, "Topology", NULL,NULL, Ptr, xmlTopology, STRUCTS_XML_UNINIT, NULL);
		}
	}

	fclose(Ptr);
}

/******************* END AUSILIARITY FUNCTIONS *****************************/
