/*
 * load_topology.cc
 *
 *  Created on: Jan 24, 2015
 * Last modify: Jan 26, 2015
 *      Author: Roberta Fumarola
 * Description: Parsing from XML file to class Topology C++
 */

#include "header_project.h"

int simul;

int main(int argc, char *argv[]) {

	int nodes = 0;

	printf("0: Stampa topologia per GNS3\n");
	printf("1: Stampa topologia per simulazione\n");
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
