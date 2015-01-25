#include "header_project.h"

int main(int argc, char *argv[]) {

	int nodes = NUM_NODES;

	Topology net(nodes);

	net.InitAdjMatrix();
	net.PrintAdjMatrix();
	net.InitXmlStruct();
	//net.PrintXmlStruct();

	return 0;
}
