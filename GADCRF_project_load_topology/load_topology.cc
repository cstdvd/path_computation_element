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

	Topology net(nodes);//Topology net=new Topology(nodes);
	net.LoadTopology(xmlTopology);
	net.PrintAdjMatrix();

	printf("Select mode(0=GNS3, 1=real 2=demo)\n");
	int mode=0;
	scanf("%i",&mode);
	if(mode==0){
		system("chmod 700 ./script/config_tap.sh");
		system("./script/config_tap.sh");
		//system("./tap_addr.sh");
	}
	printf("test\n");
	system("ping 10.1.1.1 -c 5");
	printf("test ended\n");

	return 0;
}
