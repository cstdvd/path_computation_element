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

	int nodes = NUM_NODES;

	Topology net(nodes);

	net.InitAdjMatrix();
	net.PrintAdjMatrix();
	net.InitXmlStruct();
	net.SaveTopology();
	return 0;
}
