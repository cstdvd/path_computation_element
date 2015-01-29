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

	system("./config_tap.sh");

	FILE *fp;
	char result [14];
	fp = popen("./tap_addr.sh","r");
	fread(result,1,sizeof(result),fp);
	fclose (fp);
	printf("Tap0 ip addr: %s\n",result);

	return 0;
}
