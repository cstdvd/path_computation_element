/*
 * load_topology.cc
 *
 *  Created on: Jan 24, 2015
 * Last modify: Jan 26, 2015
 *      Author: Roberta Fumarola
 * Description: Parsing from XML file to class Topology C++
 */

#include "header_project.h"

int main(int argc, char *argv[]) {

	struct xmlRoot2 *xmlTopology;
	int nodes = 0;

	xmlTopology = (struct xmlRoot2*) malloc(sizeof(struct xmlRoot2));

	ImportTopology(xmlTopology);
	nodes = xmlTopology->nodes;

	Topology net(nodes);
	net.LoadTopology(xmlTopology);
	net.PrintAdjMatrix();

	return 0;
}
