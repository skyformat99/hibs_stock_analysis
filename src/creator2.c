/* creator
This creates 1 neural nets based on the correlation
argv[1]

*/
      
#include "gen.h"
#include "lwneuralnet.h"      

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>


char *companies[10000];
int number_companies;


float rand01 ()
{
        return ((float)rand())/((float)RAND_MAX);
}
	
	


int read_companies(FILE *pf) {
char line[20000];

if (! fgets(line,19999,pf)) return(0);
{
  int position;
  char *cp;
  line[strlen(line)-1] ='\0'; /* clip the \n off */
  if (!(*line)) return(read_companies(pf));
  companies[number_companies++] = strdup(line);
  }
return(1);
}
  



                            
                     

			    
							                            
                            

			    
							
										    
														
																	    






 
  


    


	

main(int argc,char *argv[]) {
/* argument 1 - file name for the list of companies
argument 3 - training data in .flt format (floating point array)

*/
int i,j,k,l;
int flag;
int mode;
int size;
float *input;
float *output;
float *accuracy;
int number_spots;
network_t *net;
int number_input;
int number_output;
int number_base;
int max_input;
int total_iterations;
FILE *roster;

roster=fopen("companies.dat","r");
number_companies=0;
while (read_companies(roster));

{
  max_input=1720;
  number_input = number_companies;
  number_output = number_companies;

  number_base = number_companies;/* ups to 1712 which is close to our limit */
  number_spots = number_base;



  fprintf(stderr,"allocated.  number_input %d number_base %d number_output %d\n",number_input,number_base,number_output);
  /* create the neural networks */
  {
    char name[2000];
    net = net_allocate (3,number_input,number_base,number_output);
     net_use_bias(net,1);
    net_set_learning_rate(net,0.05);
    {
      layer_t *upper;
      layer_t *lower;
      int i; 
      for (i=1;i<3;i++) {
        neuron_t *neurons;
        int n;
	for (n=0;n<number_input;n++) {
	  int m;
	  for (m=0;m<=number_input;m++) {
	    net_set_weight(net,i,m,n,0.f);
            }
	  }
	} /* for each of the two layers */
      net->layer[0].activation = NET_ACT_TANH;
      net->layer[1].activation = NET_ACT_TANH;
      net->layer[2].activation = NET_ACT_TANH;
      }
    {
    FILE *xf;
    char line[20000];
    char comp1[20];
    char comp2[20];
    int pos1;
    int pos2;
    float factor;
    xf = fopen("corr.dat","r");
    if (!xf) exit(-1);
    while ( fgets(line,19999,xf)) {
      line[strlen(line)-1] ='\0'; /* clip the \n off */
      sscanf(line,"%s	%s	%f",comp1,comp2,&factor);
      pos1=0;
      while (pos1<number_companies) {
        if (strcmp(companies[pos1],comp1)==0) break;
        pos1++;
	}
      pos2=0;
      while (pos2<number_companies) {
        if (strcmp(companies[pos2],comp2)==0) break;
        pos2++;
	}
      net_set_weight(net,1,pos1,pos2,factor * rand01());
      net_set_weight(net,2,pos1,pos2,factor * rand01());
      net_set_weight(net,1,pos2,pos1,factor * rand01());
      net_set_weight(net,2,pos2,pos1,factor * rand01());
      
      }
    fclose(xf);  
    }
    
    sprintf(name,"the.net");
    net_bsave(name,net);
    net_free(net);
    }
  
  fprintf(stderr,"saved\n");
  for (i=0;i<number_companies;i++) {
    if (companies[i]) {
      free(companies[i]);
      companies[i]=NULL;
      }
    }
  number_companies=0;
  
  } /* for each company set */
exit(0);
}
