#include "header_project.h"

void showConfigureNet(struct loopback* loopArray, struct topologyLink** net, int n){
	for(int i=0; i<n; i++){
		printf("Username:\radmin\rPassword:\r\rR%d# config t\rR%d(config)# ip cef\r",i,i);
		for(int j=0; j<n; j++){
			if(net[i][j].capacity!=-1)
				printf("R%d(config)# interface %s\rR%d(config-if)# tag-switching ip\r"
						"R%d(config-if)# exit\r",i,net[i][j].srcInterface,i,i);
		}
		printf("R%d(config)# mpls traffic-eng tunnels\r",i);
		for(int j=0; j<n; j++){
			if(net[i][j].capacity!=-1)
				printf("R%d(config)# interface %s\rR%d(config-if)# mpls traffic-eng tunnels\r"
						"R%d(config-if) ip rsvp bandwidth 1024 1024\rR%d(config-if)# exit\r",
						i,net[i][j].srcInterface,i,i,i);
		}
		printf("R%d(config)# router ospf 100\rR%d(config-router)# mpls traffic-eng area 1\r"
				"R%d(config-router)# mpls traffic-eng router-id Loopback0\rR%d(config-router)#"
				" network %s 0.0.0.0 area1\rR%d(config-router)# exit\rR%d(config)# exit\rR%d# exit\r\r",
				i,i,i,i,loopArray[i].loopAddr,i,i,i);
	}
}

void showConfigureLSP(int s, char* src, char* dest, char*cap, char*id, int size, struct topologyLink **net){
	printf("Username:\radmin\rPassword:\r\rR%d# config t\rR%d(config)# interface Tunnel%s\r"
			"R%d(config-if)# ip unnumbered Loopback0\rR%d(config-if)# tunnel destination %s\r"
			"R%d(config-if)# tunnel mode mpls traffic-eng\rR%d(config-if)# tunnel mpls traffic-eng autoroute announce\r"
			"R%d(config-if)# tunnel mpls traffic-eng priority 2 2\rR%d(config-if)# tunnel mpls traffic-eng bandwidth %s\r"
			"R%d(config-if)# tunnel mpls traffic-eng path-option 1 explicit name path%s\r"
			"R%d(config-if)# ip explicit-path name path%s enable\r",
			s,s,id,s,s,dest,s,s,s,s,cap,s,id,s,id);
	for(int i=0;i<size;i++)
		printf("R%d(cfg-ip-expl-path)# next-address %s\r",s,net[i][i+1].dstAddr);
	printf("R%d(cfg-ip-expl-path)# exit\rR%d(config)# exit\rR%d# exit\r\r",s,s,s);
}
