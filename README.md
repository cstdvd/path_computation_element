#PCE
Path Computation Element for Cisco routers

##Esportazione e Importazione di una topologia di rete

Ci sono due cartelle: 
- GADCRF_project_save_topology;
- GADCRF_project_load_topology;

La prima cartella contiene i files e l'eseguibile per esportare la matrice delle adiacenze in XML.
La seconda cartella contiene i files e l'eseguibile per importare la matrice delle adience da XML (ed eventualmente riesportare in XML).

Comandi per compilare da terminale load e save topology
```
gcc load_topology.cc config_topology.cpp -lpdel -lexpat -lpthread -lstdc++
gcc save_topology.cc config_topology.cpp -lpdel -lexpat -lpthread -lstdc++
```
##Risoluzione delle dipendenze sulle librerie

Aprite il gestore pacchetti e installate le seguenti librerie:
```
libxml2
libxml++
libxml++-dev
libstdc++
expat
libexpat1-dev
libssl
```

Copiate le due cartelle expat-2.1.0 e libpdel-0.6.1 nella home. 
### Configurazione expat:
Posizionatevi nella cartella expat-2.1.0 nella home e date i seguenti comandi:
```
./configure
make
sudo make install
```
###Configurazione libpdel:
Posizionatevi nella cartella libpdel-0.6.1 nella home e date i seguenti comandi:
```
make
sudo make install
```
