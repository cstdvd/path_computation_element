/*
 * config_topology.cc
 *
 *  Created on: Jan 24, 2015
 *      Author: Roberta Fumarola
 */


#include "header_project.h"

//Constructor
Topology::Topology(int nodes){

	int i;
	n = nodes;
	adjMatrix = (struct link**) calloc(n,sizeof(struct link));
	for (i=0;i<n;i++){
		adjMatrix[i] = (struct link*) calloc(n,sizeof(struct link));
	}

	xmlStruct = (struct xmlRoot*) malloc(sizeof (struct xmlRoot));
	xmlStruct->nodes = nodes;
	xmlStruct->xmlVector = (struct xmlLink*) malloc(sizeof(struct xmlLink));
	xmlStruct->xmlVector->list.length = n*n;

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

	//Inizializzazione molto brutta visto che non funziona iostream...
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

	struct charLink *l;
	l = (struct charLink*) calloc((n*n),sizeof(struct charLink));

	for(i=0;i<n;i++){

		for(j=0;j<n;j++){

			k=i+j+t;

			sprintf(l[k].capacity,"%d",adjMatrix[i][j].capacity);
			sprintf(l[k].capacity,"%d",adjMatrix[i][j].used);

			strcpy(l[k].srcAddr,adjMatrix[i][j].srcAddr);
			strcpy(l[k].dstAddr,adjMatrix[i][j].dstAddr);
			strcpy(l[k].srcInterface,adjMatrix[i][j].srcInterface);
			strcpy(l[k].dstInterface,adjMatrix[i][j].dstInterface);
		}
		t=k-c;
		c++;
	}
	xmlStruct->xmlVector->list.elems = (struct charLink*)l;
}

/*
void Topology::SaveTopology(){

	// Descriptor for field 'list' in 'struct xmlLink': array of strings
	static const struct structs_type list_array_type =
			STRUCTS_ARRAY_TYPE(&structs_type_string, "list_elem", "list_elem");

	// Descriptor for 'struct xmlLink'
	static const struct structs_field xmlLink_fields[] = {
			STRUCTS_STRUCT_FIELD(xmlLink, list, &list_array_type),
			STRUCTS_STRUCT_FIELD_END
	};
	static const struct structs_type xmlLink_type =
			STRUCTS_STRUCT_TYPE(xmlLink, &xmlLink_fields);

	// Descriptor for a variable of type 'struct xmlLink *'
	static const struct structs_type xmlVector_ptr_type =
			STRUCTS_POINTER_TYPE(&xmlLink_type, "struct xmlVector");

	// Descriptor for 'struct janfu'
	static const struct structs_field xmlRoot_fields[] = {
		    STRUCTS_STRUCT_FIELD(xmlRoot, nodes, &structs_type_int),
		    STRUCTS_STRUCT_FIELD(xmlRoot, xmlVector, &xmlVector_ptr_type),
		    STRUCTS_STRUCT_FIELD_END
	};
	static const struct structs_type xmlRoot_type =
			STRUCTS_STRUCT_TYPE(xmlRoot, &xmlRoot_fields);

	FILE *Ptr;

	if((Ptr=fopen("topology_xml","a"))==NULL){

		printf("errore apertura topology_xml\n");
	}
	else{
		printf("ok\n");
			structs_xml_output(&xmlRoot_type, "xmlRoot", NULL, xmlStruct, Ptr, NULL, 0);
	}

}
*/
