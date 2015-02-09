/*
 * load_topology.cc
 *
 *      Author: Roberta Fumarola, David Costa, Gaetano Alboreto
 * Description: Parsing from XML file to class Topology C++
 */

#include "header_project.h"

int simul;

int main(int argc, char *argv[]) {

	int nodes = 0;

	printf("0: Export GNS3 topology\n");
	printf("1: Export DEMO topology\n");
	printf(">\n");
	scanf("%i",&simul);

	if(simul==0)
		nodes = NUM_NODES;
	else
		nodes = NUM_NODES_DEMO;

	Topology net(nodes);

	net.InitAdjMatrix();
	net.InitLoopbackAddresses();
	net.PrintAdjMatrix();
	net.PrintLoopbackArray();
	net.InitXmlStruct();
	net.SaveTopology();
	return 0;
}
