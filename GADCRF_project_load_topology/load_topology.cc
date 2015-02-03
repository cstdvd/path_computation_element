/*
 * load_topology.cc
 *
 *  Created on: Jan 24, 2015
 * Last modify: Jan 26, 2015
 *      Author: Roberta Fumarola
 * Description: Parsing from XML file to class Topology C++
 */

#include "header_project.h"

void InstallLSP(Topology *net,int nodes);

int main(int argc, char *argv[]) {

	struct xmlRoot2 *xmlTopology;
	int nodes = 0;

	xmlTopology = (struct xmlRoot2*) malloc(sizeof(struct xmlRoot2));

	ImportTopology(xmlTopology);
	nodes = xmlTopology->nodes;

	Topology net(nodes);//Topology net=new Topology(nodes);
	net.LoadTopology(xmlTopology);

	printf("Select mode(0=GNS3, 1=real 2=demo)\n");
	int mode=0;
	scanf("%i",&mode);
	if(mode==0){
		//Enable tap interface and test it
		system("chmod 700 ./script/config_tap.sh");
		system("./script/config_tap.sh");
		printf("test\n");
		system("ping 10.1.1.1 -c 3");
		printf("test ended\n");
	}

	int choise=0;
	while(1){
		printf("********* MENU *********\n");
		printf("1: Print adjacency matrix\n");
		printf("2: Install LSP\n");
		printf("> ");
		scanf("%i",&choise);
		switch(choise){
		case 1:
			net.PrintAdjMatrix();
			break;
		case 2:
			InstallLSP(&net,nodes);

		}
	}

	return 0;
}

void InstallLSP(Topology *net,int nodes){

	int size;
	int capacity;
	int src;
	int dst;

	printf("Source node:\n> ");
	scanf("%i",&src);
	printf("Destination node:\n> ");
	scanf("%i",&dst);
	printf("Capacity:\n> ");
	scanf("%i",&capacity);
	int* path = find_path(net->Matrix(),nodes,src,dst,capacity,&size);
	if(path==NULL){
		printf("It's not possible to install an LSP\n");
		return;
	}
	net->UpdateTopology(path,size,capacity);
	//Aprire i terminali per configurare i router!

	for(int i=0;i<size;i++){
		int index=path[i];
		for(int j=0;j<nodes;j++){
			if(net->Matrix()[index][j].capacity!=-1){
				char dest[100]="";
				strcat(dest,"expect ./script/telnet_test.sh ");
				strcat(dest,net->Matrix()[index][j].srcAddr);
				printf("%s\n",dest);
				system(dest);
				break;
			}
		}
	}
}


