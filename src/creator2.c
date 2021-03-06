/* creator2
Inputs:
	companies.dat
	corr.dat
Outputs:
	the.net
V1.7 there was a bug in normalize that has everything normalized to the first value.  This explains alot.
our conformance is better than ever before - with errors hovering around 0.7 but thy always hover and don't train.
So that makes me think to try the 5 variables approach yet again.
Things were mich more normal - PBG up 2% it said. and it did go up about 1% a while later.


V1.6 - back to V1.1

V1.5 reduce the hidden layer to 2 instead of 5

V1.4 More like 1.1 - variable dependant on friends only

V1.3 - learning weas slower.  Made a mistake of linking pos1 to pos1, pos2 to pos2. Will try it without this
error - so that the variable is dependant on its friends only.


V1.2 The training learning rate was cut by 1 fifth,  but the training was not nearly as good as the first one.  Now we have
error rates for specific stocks, which is nice,   but when taken to old data, it is not as good.  When taken to newer data, it is also not as good.
So we are going to try the crossover.


V1.2 - Trying a new approach - 5 variables for each output, no crossover. I might try it with crossover later.

a             b               c


a1 a2 a3 a4 a5 b1 b2 b3 b4 b5 c1 c2 c3 c4 c5

a b c 

This way each output will be trained independently.  This could be done more efficiently as a set of smaller networks.
But the advantage of a big network is that we can do crossover.
Crossover - makes the numbers more interdependant, but could cause the overtraining.


V1.1 - original matrix - with a middle layer equal to the input and output layer.
weights were just to the known correlations.
The thing trained to a low error in about 14 passes of 3 months of data, but did poorly on the next day, showing that it was overtrained.


This creates 1 neural nets based on the correlation
It currently creates a hidden layer with the same size as the input and output layer.
And the wiring is the same - minus some randomization.
It might be good to increase the number of hidden layer nodes by a factor of 2 and then wire it up that way

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

  number_base = number_companies;/* 2 variables per output */
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
	
     net_set_weight(net,1,pos1,pos2,factor * (rand01()-0.5));
      net_set_weight(net,2,pos1,pos2,factor * (rand01()-0.5));
      net_set_weight(net,1,pos2,pos1,factor * (rand01()-0.5));
      net_set_weight(net,2,pos2,pos1,factor * (rand01()-0.5));

      
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

