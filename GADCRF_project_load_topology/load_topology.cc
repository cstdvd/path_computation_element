/*
 * load_topology.cc
 *
 *  Created on: Jan 24, 2015
 * Last modify: Jan 26, 2015
 *      Author: Roberta Fumarola
 * Description: Parsing from XML file to class Topology C++
 */

#include "header_project.h"

void installLSP(Topology *net,int nodes);
void installLSPdemo(Topology *net,int nodes);

int main(int argc, char *argv[]) {

	struct xmlRoot2 *xmlTopology;
	int nodes = 0;

	xmlTopology = (struct xmlRoot2*) malloc(sizeof(struct xmlRoot2));

	ImportTopology(xmlTopology);
	nodes = xmlTopology->nodes;

	Topology net(nodes);//Topology net=new Topology(nodes);
	net.LoadTopology(xmlTopology);

	int mode;
	while(mode!=0 && mode!=2){
		printf("Select mode(0=GNS3, 1=real, 2=demo)\n> ");
		scanf("%i",&mode);
		switch(mode){
		case 0:
			//Enable tap interface and test it
			system("chmod 700 ./script/config_tap.sh");
			system("./script/config_tap.sh");
			printf("test\n");
			system("ping 10.1.1.1 -c 3");
			printf("test ended\n");
			break;
		case 1:
			printf("Real mode not implemented.\n");
			break;
		case 2:
			printf("Demo mode\n");
			break;
		default:
			printf("Command not found.\n");
			break;
		}
	}

	int choise;
	while(1){
		printf("********* MENU *********\n");
		printf("1: Print adjacency matrix\n");
		printf("2: Install LSP\n");
		printf("3: Exit\n");
		printf("> ");
		scanf("%i",&choise);
		switch(choise){
		case 1:
			net.PrintAdjMatrix();
			break;
		case 2:
			if(mode==0)
				installLSP(&net,nodes);
			else if(mode==2)
				installLSPdemo(&net,nodes);
			break;
		case 3:
			return 0;
		default:
			printf("Command not found\n");
			break;
		}
	}

	return 0;
}

void installLSP(Topology *net,int nodes){

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
	// per prova

	system("expect ./script/cef.sh 10.1.1.1 Ethernet0/0 Ethernet1/1 Ethernet1/0 Ethernet1/3");
	system("expect ./script/cef.sh 10.1.1.2 Ethernet1/0 Ethernet1/2");
	system("expect ./script/cef.sh 10.2.2.2 Ethernet1/1 Ethernet1/0");
	system("expect ./script/cef.sh 10.5.5.2 Ethernet0/0 Ethernet1/1 Ethernet1/2");
	system("expect ./script/cef.sh 10.3.3.2 Ethernet1/1 Ethernet1/0");
}

void installLSPdemo(Topology *net,int nodes){
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

}
