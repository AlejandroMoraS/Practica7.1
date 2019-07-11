#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <mmu.h>

#define NUMPROCS 4
#define PAGESIZE 4096
#define PHISICALMEMORY 12*PAGESIZE
#define TOTFRAMES PHISICALMEMORY/PAGESIZE
#define RESIDENTSETSIZE PHISICALMEMORY/(PAGESIZE*NUMPROCS)

extern char *base;
extern int framesbegin;
extern int idproc;
extern int systemframetablesize;
extern int ptlr;

extern struct SYSTEMFRAMETABLE *systemframetable;
extern struct PROCESSPAGETABLE *ptbr;


int getfreeframe();
int searchvirtualframe();
int getfifo();

int pagefault(char *vaddress)
{
    int i;
    int frame,vframe;
    long pag_a_expulsar;
    int fd;
    char buffer[PAGESIZE];
    int pag_del_proceso;
    int posPagExit;
    int posFramVM;
    int frameFree;
    // A partir de la dirección que provocó el fallo, calculamos la página
    pag_del_proceso=(long) vaddress>>12;


    // Si la página del proceso está en un marco virtual del disco
    if(!(ptbr +pag_del_proceso)->presente && (ptbr +pag_del_proceso)->framenumber > -1)
    {
      printf("La pagia %d esta en un marcho\n",pag_del_proceso );
		// Lee el marco virtual al buffer
    		readblock(buffer, (ptbr +pag_del_proceso)->framenumber);
        // Libera el frame virtual
        systemframetable[(ptbr +pag_del_proceso)->framenumber].assigned = 0;
    }


    // Cuenta los marcos asignados al proceso
    i=countframesassigned();

    // Si ya ocupó todos sus marcos, expulsa una página
    if(i>=RESIDENTSETSIZE)
    {
		// Buscar una página a expulsar
    printf("La pagia %d busca un marco\n",pag_del_proceso );
      posPagExit = getfifo();
      (ptbr + posPagExit)->presente = 0;
        if((ptbr + posPagExit)-> modificado)// Si la página ya fue modificada, grábala en disco
        {
			// Escribe el frame de la página en el archivo de respaldo y pon en 0 el bit de modificado

        (ptbr + posPagExit)->modificado = 0;
          saveframe((ptbr + posPagExit)->framenumber);
        }

        // Busca un frame virtual en memoria secundaria
        posFramVM = searchvirtualframe();
		// Si no hay frames virtuales en memoria secundaria regresa error
		if(posFramVM == -1 ){
          printf("La pagia %d ERROR al buscar un frame virtual\n",pag_del_proceso);
            return(-1);
        }

        printf("La pagia %d encontro un frame virtual en %d\n",pag_del_proceso,posFramVM );
        // Copia el frame a memoria secundaria, actualiza la tabla de páginas y libera el marco de la memoria principal
        printf("posPagExit:%d\n", posPagExit);
        printf("(ptbr + posPagExit)->framenumber:%d\n", (ptbr + posPagExit)->framenumber);
        copyframe((ptbr + posPagExit)->framenumber, posFramVM);
	      systemframetable[(ptbr + posPagExit)->framenumber].assigned = 0;
        (ptbr + posPagExit)->framenumber = posFramVM;

    }

    frameFree = getfreeframe();// Busca un marco físico libre en el sistema
	// Si no hay marcos físicos libres en el sistema regresa error
    if(frameFree == -1){
       printf("La pagia %d no encontro un marco libre\n",pag_del_proceso);
        return(-1); // Regresar indicando error de memoria insuficiente
    }

    printf("La pagia %d encontro un marco libre en %d\n",pag_del_proceso,frameFree );

    // Si la página estaba en memoria secundaria
    if( !(ptbr +pag_del_proceso)->presente && (ptbr +pag_del_proceso)->framenumber < 0){
        // Cópialo al frame libre encontrado en memoria principal y transfiérelo a la memoria física
        printf("La pagia %d estaba en memoria secundaria\n",pag_del_proceso );
        printf("frameFree:%d\n", frameFree);
        printf("framenumber:%d\n", (ptbr +pag_del_proceso)->framenumber);
          writeblock(buffer, frameFree);
		      (ptbr +pag_del_proceso)->framenumber = frameFree;
		      loadframe(frameFree);
    }

	// Poner el bit de presente en 1 en la tabla de páginas y el frame
  (ptbr +pag_del_proceso)->presente = 1;
	(ptbr +pag_del_proceso)->framenumber = frameFree;
	systemframetable[(ptbr +pag_del_proceso)->framenumber].assigned = 1;

    return(1); // Regresar todo bien
}


int getfifo() {/*
	unsigned long longestTime = ptbr[0].tlastaccess;
	int i=0;
  int posPagExit=0;
	for(i=1;i<ptlr;++i) {
		if( ptbr[i].tlastaccess > longestTime && ptbr[i].presente) {
			longestTime = ptbr[i].tlastaccess;
			posPagExit = i;
		}*/


	//return posPagExit};
  unsigned long menor = ptbr[0].tlastaccess;
int val = -1;

for(int i=0;i<ptlr;++i) {
  if(ptbr[i].presente)
  if(menor < ptbr[i].tlastaccess) {
    menor = ptbr[i].tlastaccess;
    val = i;
  }
}

return val;
}

int searchvirtualframe() {
	int i;
  int startMV = systemframetablesize+framesbegin;
  int endMV = (systemframetablesize+framesbegin)*2;
    // Busca un marco libre en el sistema
    for(i=startMV;i <endMV;i++){
        if(systemframetable[i].assigned ==0)
        {
            systemframetable[i].assigned=1;
            return i;
        }
    }

    return -1;
}

int getfreeframe()
{
    int i;
    // Busca un marco libre en el sistema
    for(i=framesbegin;i<systemframetablesize+framesbegin;i++)
        if(!systemframetable[i].assigned)
        {
            systemframetable[i].assigned=1;
            break;
        }
    if(i<systemframetablesize+framesbegin)
        systemframetable[i].assigned=1;
    else
        i=-1;
    return(i);
}
