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
	xmlStruct = (struct xmlRoot2*) malloc(sizeof (struct xmlRoot2*));

	xmlStruct->nodes = nodes;

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
					printf("%s  ",adjMatrix[i][j].srcInterface);
			}
			printf("\n");

			for(j=0;j<n;j++){
				if(strcmp(adjMatrix[i][j].dstInterface,"NULL")==0)
					printf("%s\t\t\t",adjMatrix[i][j].dstInterface);
				else
					printf("%s  ",adjMatrix[i][j].dstInterface);
			}
			printf("\n");
			printf("------------------------------------------------------\n");

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
				adjMatrix[i][j].used = -1;
		}
	}

	//From R1 to R2
	strcpy(adjMatrix[0][1].srcAddr,"10.1.1.1");
	strcpy(adjMatrix[0][1].dstAddr,"10.1.1.2");
	strcpy(adjMatrix[0][1].srcInterface,"Ethernet interface 1/0");
	strcpy(adjMatrix[0][1].dstInterface,"Ethernet interface 1/0");
	adjMatrix[0][1].capacity = 100;
	adjMatrix[0][1].used = 100;

	//From R1 to R3
	strcpy(adjMatrix[0][2].srcAddr,"10.2.2.1");
	strcpy(adjMatrix[0][2].dstAddr,"10.2.2.2");
	strcpy(adjMatrix[0][2].srcInterface,"Ethernet interface 1/1");
	strcpy(adjMatrix[0][2].dstInterface,"Ethernet interface 1/1");
	adjMatrix[0][2].capacity = 100;
	adjMatrix[0][2].used = 100;

	//From R2 to R1
	strcpy(adjMatrix[1][0].srcAddr,"10.1.1.2");
	strcpy(adjMatrix[1][0].dstAddr,"10.1.1.1");
	strcpy(adjMatrix[1][0].srcInterface,"Ethernet interface 1/0");
	strcpy(adjMatrix[1][0].dstInterface,"Ethernet interface 1/0");
	adjMatrix[1][0].capacity = 100;
	adjMatrix[1][0].used = 100;

	//From R2 to R5
	strcpy(adjMatrix[1][4].srcAddr,"10.5.5.1");
	strcpy(adjMatrix[1][4].dstAddr,"10.5.5.2");
	strcpy(adjMatrix[1][4].srcInterface,"Ethernet interface 1/2");
	strcpy(adjMatrix[1][4].dstInterface,"Ethernet interface 1/2");
	adjMatrix[1][4].capacity = 100;
	adjMatrix[1][4].used = 100;

	//From R3 to R1
	strcpy(adjMatrix[2][0].srcAddr,"10.2.2.2");
	strcpy(adjMatrix[2][0].dstAddr,"10.2.2.1");
	strcpy(adjMatrix[2][0].srcInterface,"Ethernet interface 1/1");
	strcpy(adjMatrix[2][0].dstInterface,"Ethernet interface 1/1");
	adjMatrix[2][0].capacity = 100;
	adjMatrix[2][0].used = 100;

	//From R3 to R4
	strcpy(adjMatrix[2][3].srcAddr,"10.3.3.1");
	strcpy(adjMatrix[2][3].dstAddr,"10.3.3.2");
	strcpy(adjMatrix[2][3].srcInterface,"Ethernet interface 1/0");
	strcpy(adjMatrix[2][3].dstInterface,"Ethernet interface 1/0");
	adjMatrix[2][3].capacity = 100;
	adjMatrix[2][3].used = 100;

	//From R4 to R3
	strcpy(adjMatrix[3][2].srcAddr,"10.3.3.2");
	strcpy(adjMatrix[3][2].dstAddr,"10.3.3.1");
	strcpy(adjMatrix[3][2].srcInterface,"Ethernet interface 1/0");
	strcpy(adjMatrix[3][2].dstInterface,"Ethernet interface 1/0");
	adjMatrix[3][2].capacity = 100;
	adjMatrix[3][2].used = 100;

	//From R4 to R5
	strcpy(adjMatrix[3][4].srcAddr,"10.4.4.1");
	strcpy(adjMatrix[3][4].dstAddr,"10.4.4.2");
	strcpy(adjMatrix[3][4].srcInterface,"Ethernet interface 1/1");
	strcpy(adjMatrix[3][4].dstInterface,"Ethernet interface 1/1");
	adjMatrix[3][4].capacity = 100;
	adjMatrix[3][4].used = 100;

	//From R5 to R2
	strcpy(adjMatrix[4][1].srcAddr,"10.5.5.2");
	strcpy(adjMatrix[4][1].dstAddr,"10.5.5.1");
	strcpy(adjMatrix[4][1].srcInterface,"Ethernet interface 1/2");
	strcpy(adjMatrix[4][1].dstInterface,"Ethernet interface 1/2");
	adjMatrix[4][1].capacity = 100;
	adjMatrix[4][1].used = 100;

	//From R5 to R4
	strcpy(adjMatrix[4][3].srcAddr,"10.4.4.2");
	strcpy(adjMatrix[4][3].dstAddr,"10.4.4.1");
	strcpy(adjMatrix[4][3].srcInterface,"Ethernet interface 1/1");
	strcpy(adjMatrix[4][3].dstInterface,"Ethernet interface 1/1");
	adjMatrix[4][3].capacity = 100;
	adjMatrix[4][3].used = 100;

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
			printf("l[%d].capacity= %d\n",k,l[k].capacity);
			l[k].used = adjMatrix[i][j].used;
			printf("l[%d].used= %d\n",k,l[k].used);
			strcpy(l[k].srcAddr,adjMatrix[i][j].srcAddr);
			printf("l[%d].srcAddr= %s\n",k,l[k].srcAddr);
			strcpy(l[k].dstAddr,adjMatrix[i][j].dstAddr);
			printf("l[%d].dstAddr= %s\n",k,l[k].dstAddr);
			strcpy(l[k].srcInterface,adjMatrix[i][j].srcInterface);
			printf("l[%d].srcInterface= %s\n",k,l[k].srcInterface);
			strcpy(l[k].dstInterface,adjMatrix[i][j].dstInterface);
			printf("l[%d].dstInterface= %s\n",k,l[k].dstInterface);

		}
		t=k-c;
		c++;
	}
	xmlStruct->xmlVector->list.elems = l;
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

	// Descriptor for 'struct xmlRoot2'
	static const struct structs_field xmlRoot_fields[] = {
		    STRUCTS_STRUCT_FIELD(xmlRoot2, nodes, &structs_type_int),
		    STRUCTS_STRUCT_FIELD(xmlRoot2, xmlVector, &topLink_prt_type),
		    STRUCTS_STRUCT_FIELD_END
	};

	static const struct structs_type xmlRoot_type =
			STRUCTS_STRUCT_TYPE(xmlRoot2, &xmlRoot_fields);

	FILE *Ptr;

	if((Ptr=fopen("topology_xml","w"))==NULL){

		printf("errore apertura topology_xml\n");
	}
	else{
		structs_xml_output(&xmlRoot_type, "Topology", NULL, xmlStruct, Ptr, NULL, 0);
	}
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

	// Descriptor for 'struct xmlRoot2'
	static const struct structs_field xmlRoot_fields[] = {
			STRUCTS_STRUCT_FIELD(xmlRoot2, nodes, &structs_type_int),
			STRUCTS_STRUCT_FIELD(xmlRoot2, xmlVector, &topLink_prt_type),
			STRUCTS_STRUCT_FIELD_END
	};

	static const struct structs_type xmlRoot_type =
			STRUCTS_STRUCT_TYPE(xmlRoot2, &xmlRoot_fields);

	FILE *Ptr;

	if((Ptr=fopen("topology_xml","r"))==NULL){

		printf("errore apertura topology_xml\n");
	}
	else{
		structs_xml_input(&xmlRoot_type, "Topology", NULL,NULL, Ptr, xmlTopology, STRUCTS_XML_UNINIT, NULL);
	}

	fclose(Ptr);
}

/******************* END AUSILIARITY FUNCTIONS *****************************/
