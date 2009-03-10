/* network.c -- lightweight backpropagation neural network, version 0.8
 * copyright (c) 1999-2005 Peter van Rossum <petervr@users.sourceforge.net>
 * released under the GNU Lesser General Public License
 * $Id$ */

/*!\file network.c
 * Lightweight backpropagation neural network. 
 *
 * This is a lightweight library implementating a neural network for use
 * in C and C++ programs. It is intended for use in applications that
 * just happen to need a simply neural network and do not want to use
 * needlessly complex neural network libraries. It features multilayer
 * feedforward perceptron neural networks, sigmoidal activation function
 * with bias, backpropagation training with settable learning rate and
 * momentum, and backpropagation training in batches.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "lwneuralnet.h"


/****************************************
 * Compile-time options
 ****************************************/

#define DEFAULT_MOMENTUM 0.1
#define DEFAULT_LEARNING_RATE 0.25
#define DEFAULT_WEIGHT_RANGE 1.0
#define MIN_ENTRY -12.47000026702880859375
#define MAX_ENTRY 12.47000026702880859375
#define absf(x) ( ((x) <0.) ? (-x) : (x) )


/****************************************
 * Initialization
 ****************************************/

/*!\brief Assign random values to all weights in the network.
 * \param net Pointer to neural network.
 * \param range Floating point number.
 *
 * All weights in the neural network are assigned a random value
 * from the interval [-range,range].
 */
void
net_randomize (network_t *net, float range)
{
  int l, nu, nl;

  assert (net != NULL);
  assert (range >= 0.0);

  for (l = 1; l < net->no_of_layers; l++) {
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      for (nl = 0; nl <= net->layer[l - 1].no_of_neurons; nl++) {
        net->layer[l].neuron[nu].weight[nl] =
          2.0 * range * ((float) random () / RAND_MAX - 0.5);
      }
    }
  }
}

#if 0
/*!\brief Set weights of the network to 0.
 * \param net Pointer to neural network.
 */
static void
net_reset_weights (network_t *net)
{
  int l, nu, nl;

  assert (net != NULL);

  for (l = 1; l < net->no_of_layers; l++) {
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      for (nl = 0; nl <= net->layer[l - 1].no_of_neurons; nl++) {
        net->layer[l].neuron[nu].weight[nl] = 0.0;
      }
    }
  }
}
#endif

/*!\brief Set deltas of the network to 0.
 * \param net Pointer to neural network.
 */
void
net_reset_deltas (network_t *net)
{
  int l, nu, nl;

  assert (net != NULL);

  for (l = 1; l < net->no_of_layers; l++) {
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      for (nl = 0; nl <= net->layer[l - 1].no_of_neurons; nl++) {
        net->layer[l].neuron[nu].delta[nl] = 0.0;
      }
    }
  }
}

/*! \brief Enable or disable use of bias.
 *  \param net Pointer to neural network.
 *  \param flag Boolean.
 *  Disable use of bias if flag is zero; enable otherwise.
 *  By default, bias is enabled.
 */
void
net_use_bias(network_t *net, int flag)
{
  int l;

  assert (net != NULL);

  if (flag != 0) {
    /* permanently set output of bias neurons to 1 */
    for (l = 0; l < net->no_of_layers; l++) {
      net->layer[l].neuron[net->layer[l].no_of_neurons].output = 1.0;
      net->layer[l].neuron[net->layer[l].no_of_neurons].activation = MAX_ENTRY;
      net->layer[l].neuron[net->layer[l].no_of_neurons].absolute_activation = absf(MAX_ENTRY);
    }
  } else {
    /* permanently set output of bias neurons to 0 */
    for (l = 0; l < net->no_of_layers; l++) {
      net->layer[l].neuron[net->layer[l].no_of_neurons].output = 0.0;
      net->layer[l].neuron[net->layer[l].no_of_neurons].activation = MIN_ENTRY;
      net->layer[l].neuron[net->layer[l].no_of_neurons].absolute_activation = 
        absf(MIN_ENTRY);
    }
  }
}

/****************************************
 * Memory Management
 ****************************************/
/*!\brief [Internal] Allocate memory for the neurons in a layer of a network.
 * \param layer Pointer to layer of a neural network.
 * \param no_of_neurons Integer.
 *
 * Allocate memory for a list of no_of_neuron + 1 neurons in the specified
 * layer. The extra neuron is used for the bias.
 */
static void
allocate_layer (layer_t *layer, int no_of_neurons)
{
  assert (layer != NULL);
  assert (no_of_neurons > 0);

  layer->no_of_neurons = no_of_neurons;
  layer->neuron = (neuron_t *) calloc (no_of_neurons + 1, sizeof (neuron_t));
}

/*!\brief [Internal] Allocate memory for the weights connecting two layers.
 * \param lower Pointer to one layer of a neural network.
 * \param upper Pointer to the next layer of a neural network.
 *
 * Allocate memory for the weights connecting two layers of a neural
 * network. The neurons in these layers should previously have been
 * allocated with allocate_layer().
 */
static void
allocate_weights (layer_t *lower, layer_t *upper)
{
  int n;

  assert (lower != NULL);
  assert (upper != NULL);

  for (n = 0; n < upper->no_of_neurons; n++) {
    upper->neuron[n].weight =
      (float *) calloc (lower->no_of_neurons + 1, sizeof (float));
    upper->neuron[n].delta =
      (float *) calloc (lower->no_of_neurons + 1, sizeof (float));
  }

  /* no incoming weights for bias neurons */
  upper->neuron[n].weight = NULL;
  upper->neuron[n].delta = NULL;
}

/*!\brief Allocate memory for a network.
 * \param no_of_layers Integer.
 * \param arglist Pointer to sequence of integers.
 * \return Pointer to newly allocated network.
 *
 * Allocate memory for a neural network with no_of_layer layers,
 * including the input and output layer. The number of neurons in each
 * layer is given in arglist, with arglist[0] being the number of
 * neurons in the input layer and arglist[no_of_layers-1] the number of
 * neurons in the output layer.
 */
network_t *
net_allocate_l (int no_of_layers, const int *arglist)
{
  int l;
  network_t *net;

  assert (no_of_layers >= 2);
  assert (arglist != NULL);

  /* allocate memory for the network */
  net = (network_t *) malloc (sizeof (network_t));
  net->no_of_layers = no_of_layers;
  net->layer = (layer_t *) calloc (no_of_layers, sizeof (layer_t));
  for (l = 0; l < no_of_layers; l++) {
    assert (arglist[l] > 0);
    allocate_layer (&net->layer[l], arglist[l]);
  }
  for (l = 1; l < no_of_layers; l++) {
    allocate_weights (&net->layer[l - 1], &net->layer[l]);
  }

  /* abbreviations for input and output layer */
  net->input_layer = &net->layer[0];
  net->output_layer = &net->layer[no_of_layers - 1];

  /* default values for network constants */
  net->momentum = DEFAULT_MOMENTUM;
  net->learning_rate = DEFAULT_LEARNING_RATE;

  /* initialize weights and deltas */
  net_randomize (net, DEFAULT_WEIGHT_RANGE);
  net_reset_deltas (net);

  /* permanently set output of bias neurons to 1 */
  net_use_bias (net, 1);

  return net;
}

/*!\brief Allocate memory for a network.
 * \param no_of_layers Integer.
 * \param ... Sequence of integers.
 * \return Pointer to newly allocated network.
 *
 * Allocate memory for a neural network with no_of_layer layers,
 * including the input and output layer. The number of neurons in each
 * layer is given as ..., starting with the input layer and ending with
 * the output layer.
 */
network_t *
net_allocate (int no_of_layers, ...)
{
  int l, *arglist;
  va_list args;
  network_t *net;

  assert (no_of_layers >= 2);

  arglist = calloc (no_of_layers, sizeof (int));
  va_start (args, no_of_layers);
  for (l = 0; l < no_of_layers; l++) {
    arglist[l] = va_arg (args, int);
  }
  va_end (args);
  net = net_allocate_l (no_of_layers, arglist);
  free (arglist);

  return net;
}

/*!\brief Free memory allocated for a network.
 * \param net Pointer to a neural network.
 */
void
net_free (network_t *net)
{
  int l, n;

  assert (net != NULL);

  for (l = 0; l < net->no_of_layers; l++) {
    if (l != 0) {
      for (n = 0; n < net->layer[l].no_of_neurons; n++) {
        free (net->layer[l].neuron[n].weight);
        free (net->layer[l].neuron[n].delta);
      }
    }
    free (net->layer[l].neuron);
  }
  free (net->layer);
  free (net);
}

/****************************************
 * Access
 ****************************************/

/*!\brief Change the momentum of a network.
 * \param net Pointer to a neural network.
 * \param momentum Floating point number.
 */
void
net_set_momentum (network_t *net, float momentum)
{
  assert (net != NULL);
  assert (momentum >= 0.0);

  net->momentum = momentum;
}

/*!\brief Retrieve the momentum of a network.
 * \param net Pointer to a neural network.
 * \return Momentum of the neural work.
 */
float
net_get_momentum (const network_t *net)
{
  assert (net != NULL);
  assert (net->momentum >= 0.0);

  return net->momentum;
}

/*!\brief Change the learning rate of a network.
 * \param net Pointer to a neural network.
 * \param learning_rate Floating point number.
 */
void
net_set_learning_rate (network_t *net, float learning_rate)
{
  assert (net != NULL);
  assert (learning_rate >= 0.0);

  net->learning_rate = learning_rate;
}

/*!\brief Retrieve the momentum of a network.
 * \param net Pointer to a neural network.
 * \return Learning rate of the neural work.
 */
float
net_get_learning_rate (const network_t *net)
{
  assert (net != NULL);

  return net->learning_rate;
}

/*!\brief Retrieve the number of inputs of a network.
 * \param net Pointer to a neural network.
 * \return Number of neurons in the input layer of the neural network.
 */
int
net_get_no_of_inputs (const network_t *net)
{
  assert (net != NULL);

  return net->input_layer->no_of_neurons;
}

/*!\brief Retrieve the number of outputs of a network.
 * \param net Pointer to a neural network.
 * \return Number of neurons in the output layer of the neural network.
 */
int
net_get_no_of_outputs (const network_t *net)
{
  assert (net != NULL);

  return net->output_layer->no_of_neurons;
}

/*!\brief Retrieve the number of layers of a network.
 * \param net Pointer to a neural network.
 * \return Number of layers, including the input and output layers, of the 
 * neural network.
 */
int
net_get_no_of_layers (const network_t *net)
{
  assert (net != NULL);

  return net->no_of_layers;
}

/*!\brief Retrieve the number of weights of a network.
 * \param net Pointer to a neural network.
 * \return The total number of weights in the neural network.
 */
int
net_get_no_of_weights (const network_t *net)
{
  int l, result;

  assert (net != NULL);

  result = 0;
  for (l = 1; l < net->no_of_layers; l++) {
    result += (net->layer[l-1].no_of_neurons + 1) * net->layer[l].no_of_neurons;
  }

  return result;
}

/*!\brief Set a weight of a network.
 * \param net Pointer to a neural network.
 * \param l Number of lower layer.
 * \param nl Number of neuron in the lower layer.
 * \param nu Number of neuron in the next layer.
 * \param weight Floating point number.
 * The weight connecting the neuron numbered nl in the layer
 * numbered l with the neuron numbered nu in the layer numbered l+1
 * is set to weight.
 */
void
net_set_weight (network_t *net, int l, int nl, int nu, float weight)
{
  assert (net != NULL);
  assert (0 <= l && l < net->no_of_layers);
  assert (0 <= nl && nl <= net->layer[l].no_of_neurons);
  assert (0 <= nu && nu < net->layer[l+1].no_of_neurons);

  net->layer[l].neuron[nu].weight[nl] = weight;
}

/*!\brief Retrieve a weight of a network.
 * \param net Pointer to a neural network.
 * \param l Number of lower layer.
 * \param nl Number of neuron in the lower layer.
 * \param nu Number of neuron in the next layer.
 * \return Weight connecting the neuron numbered nl in the layer
 * numbered l with the neuron numbered nu in the layer numbered l+1.
 */
float
net_get_weight (const network_t *net, int l, int nl, int nu)
{
  assert (net != NULL);
  assert (0 <= l && l < net->no_of_layers-1);
  assert (0 <= nl && nl <= net->layer[l].no_of_neurons);
  assert (0 <= nu && nu < net->layer[l+1].no_of_neurons);

  return net->layer[l].neuron[nu].weight[nl];
}

/*!\brief Retrieve a bias weight of a network.
 * \param net Pointer to a neural network.
 * \param l Number of layer.
 * \param nu Number of the layer.
 * \return Bias weight of the neuron numbered nu in the layer numbered l.
 *
 * [internal] Bias is implemented by having an extra neuron in every
 * layer. The output of this neuron is permanently set to 1. The bias
 * weight returned by this routine is simply the weight from this extra
 * neuron in the layer numbered l-1 to the neuron numbered nu in the
 * layer numbered l. */
float
net_get_bias (const network_t *net, int l, int nu)
{
  assert (net != NULL);
  assert (0 < l && l < net->no_of_layers);
  assert (0 <= nu && nu < net->layer[l].no_of_neurons);

  return net_get_weight(net, l-1, net->layer[l-1].no_of_neurons, nu);
}

/*!\brief Retrieve a bias weight of a network.
 * \param net Pointer to a neural network.
 * \param l Number of layer.
 * \param nu Number of the layer.
 * \param weight Floating point number.
 * Set the bias weight of the neuron numbered nu in the layer numbered l.
 *
 * [internal] Bias is implemented by having an extra neuron in every
 * layer. The output of this neuron is permanently set to 1. This
 * routine simply sets the the weight from this extra neuron in the
 * layer numbered l-1 to the neuron numbered nu in the layer numbered l.
 */
void
net_set_bias (network_t *net, int l, int nu, float weight)
{
  assert (net != NULL);
  assert (0 < l && l < net->no_of_layers);
  assert (0 <= nu && nu < net->layer[l].no_of_neurons);

  net_set_weight(net, l-1, net->layer[l-1].no_of_neurons, nu, weight);
}

/****************************************
 * File I/O
 ****************************************/

/*!\brief Write a network to a file.
 * \param file Pointer to file descriptor.
 * \param net Pointer to a neural network.
 * \return 0 on success, a negative number of failure.
 */
int
net_fprint (FILE *file, const network_t *net)
{
  int l, nu, nl, result;

  assert (file != NULL);
  assert (net != NULL);

  /* write network dimensions */
  result = fprintf (file, "%i\n", net->no_of_layers);
  if (result < 0) {
    return result;
  }
  for (l = 0; l < net->no_of_layers; l++) {
    result = fprintf (file, "%i\n", net->layer[l].no_of_neurons);
    if (result < 0) {
      return result;
    }
  }

  /* write network constants */
  result = fprintf (file, "%f\n", net->momentum);
  if (result < 0) {
    return result;
  }
  result = fprintf (file, "%f\n", net->learning_rate);
  if (result < 0) {
    return result;
  }
  result = fprintf (file, "%f\n", net->global_error);
  if (result < 0) {
    return result;
  }

  /* write network weights */
  for (l = 1; l < net->no_of_layers; l++) {
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      for (nl = 0; nl <= net->layer[l - 1].no_of_neurons; nl++) {
        result = fprintf (file, "%f\n", net->layer[l].neuron[nu].weight[nl]);
        if (result < 0) {
          return result;
        }
      }
    }
  }

  return 0;
}

/*!\brief Read a network from a file.
 * \param file Pointer to a file descriptor.
 * \return Pointer to the read neural network on success, NULL on failure.
 */
network_t *
net_fscan (FILE *file)
{
  int no_of_layers, l, nu, nl, *arglist, result;
  network_t *net;

  assert (file != NULL);

  /* read network dimensions */
  result = fscanf (file, "%i", &no_of_layers);
  if (result <= 0) {
    return NULL;
  }
  arglist = calloc (no_of_layers, sizeof (int));
  if (arglist == NULL) {
    return NULL;
  }
  for (l = 0; l < no_of_layers; l++) {
    result = fscanf (file, "%i", &arglist[l]);
    if (result <= 0) {
      return NULL;
    }
  }

  /* allocate memory for the network */
  net = net_allocate_l (no_of_layers, arglist);
  free (arglist);
  if (net == NULL) {
    return NULL;
  }

  /* read network constants */
  result = fscanf (file, "%f", &net->momentum);
  if (result <= 0) {
    net_free (net);
    return NULL;
  }
  result = fscanf (file, "%f", &net->learning_rate);
  if (result <= 0) {
    net_free (net);
    return NULL;
  }
  result = fscanf (file, "%f", &net->global_error);
  if (result <= 0) {
    net_free (net);
    return NULL;
  }

  /* read network weights */
  for (l = 1; l < net->no_of_layers; l++) {
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      for (nl = 0; nl <= net->layer[l - 1].no_of_neurons; nl++) {
        result = fscanf (file, "%f", &net->layer[l].neuron[nu].weight[nl]);
        if (result <= 0) {
          net_free (net);
          return NULL;
        }
      }
    }
  }

  return net;
}

/*!\brief Write a network to a stdout.
 * \param net Pointer to a neural network.
 * \return 0 on success, a negative number on failure.
 */
int
net_print (const network_t *net)
{
  int l, nu, nl, result;
  FILE *file;
  file=stdout;

  assert (net != NULL);

  /* write network dimensions */
  result = fprintf (file, "layers %i\n", net->no_of_layers);
  if (result < 0) {
    return result;
  }
  
  for (l = 0; l < net->no_of_layers; l++) {
    result = fprintf (file, "neurons %i %i\n", l,net->layer[l].no_of_neurons);
    if (result < 0) {
      return result;
    }
  }

  /* write network constants */
  result = fprintf (file, "momentum %f\n", net->momentum);
  if (result < 0) {
    return result;
  }
  result = fprintf (file, "learning rate %f\n", net->learning_rate);
  if (result < 0) {
    return result;
  }
  result = fprintf (file, "global error %f\n", net->global_error);
  if (result < 0) {
    return result;
  }

  /* write network weights */
  for (l = 1; l < net->no_of_layers; l++) {
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      for (nl = 0; nl <= net->layer[l - 1].no_of_neurons; nl++) {
        result = fprintf (file, "w %i %i %i %f\n", l,nu,nl,net->layer[l].neuron[nu].weight[nl]);
        if (result < 0) {
          return result;
        }
      }
    }
  }

  return 0;
}

/*!\brief Write a network to a file.
 * \param filename Pointer to name of file to write to.
 * \param net Pointer to a neural network.
 * \return 0 on success, a negative number on failure.
 */
int
net_save (const char *filename, const network_t *net)
{
  int result;
  FILE *file;

  assert (filename != NULL);
  assert (net != NULL);

  file = fopen (filename, "w");
  if (file == NULL) {
    return EOF;
  }
  result = net_fprint (file, net);
  if (result < 0) {
    fclose (file);
    return result;
  }
  return fclose (file);
}

/*!\brief Read a network from a file.
 * \param filename Pointer to name of file to read from.
 * \return Pointer to the read neural network on success, NULL on failure.
 */
network_t *
net_load (const char *filename)
{
  FILE *file;
  network_t *net;

  assert (filename != NULL);

  file = fopen (filename, "r");
  if (file == NULL) {
    return NULL;
  }
  net = net_fscan (file);
  fclose (file);

  return net;
}

/*!\brief Write a network to a binary file.
 * \param file Pointer to file descriptor.
 * \param net Pointer to a neural network.
 * \return 0 on success, a negative number on failure.
 *
 * Write a binary representation of a neural network to a file. Note
 * that the resulting file is not necessarily portable across
 * platforms.
 */
int
net_fbprint (FILE *file, const network_t *net)
{
  int l, nu;
  size_t info_dim = net->no_of_layers + 1;
  int info[info_dim];
  float constants[3];

  assert (file != NULL);
  assert (net != NULL);

  /* write network dimensions */
  info[0] = net->no_of_layers;
  for (l = 0; l < net->no_of_layers; l++) {
    info[l + 1] = net->layer[l].no_of_neurons;
  }
  if (fwrite (info, sizeof (int), info_dim, file) < info_dim) {
    return -1;
  }

  /* write network constants */
  constants[0] = net->momentum;
  constants[1] = net->learning_rate;
  constants[2] = net->global_error;
  fwrite (constants, sizeof (float), 3, file);

  /* write network weights */
  for (l = 1; l < net->no_of_layers; l++) {
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      fwrite (net->layer[l].neuron[nu].weight, sizeof (float),
              net->layer[l - 1].no_of_neurons + 1, file);
    }
  }

  return 0;
}

/*!\brief Read a network from a binary file.
 * \param file Pointer to a file descriptor.
 * \return Pointer to the read neural network on success, NULL on failure.
 */
network_t *
net_fbscan (FILE *file)
{
  int no_of_layers, l, nu, *arglist;
  network_t *net;

  assert (file != NULL);

  /* read network dimensions */
  if (fread (&no_of_layers, sizeof (int), 1, file) < 1) {
    return NULL;
  }
  arglist = calloc (no_of_layers, sizeof (int));
  if (fread (arglist,sizeof(int),no_of_layers,file) < (size_t) no_of_layers) {
    free (arglist);
    return NULL;
  }

  /* allocate memory for the network */
  net = net_allocate_l (no_of_layers, arglist);
  free (arglist);

  /* read network constants */
  fread (&net->momentum, sizeof (float), 1, file);
  fread (&net->learning_rate, sizeof (float), 1, file);
  fread (&net->global_error, sizeof (float), 1, file);

  /* read network weights */
  for (l = 1; l < net->no_of_layers; l++) {
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      fread (net->layer[l].neuron[nu].weight, sizeof (float),
             net->layer[l - 1].no_of_neurons + 1, file);
    }
  }

  return net;
}

/*!\brief Write a network to a binary file.
 * \param filename Pointer to name of file to write to.
 * \param net Pointer to a neural network.
 * \return 0 on success, a negative number on failure.
 *
 * Write a binary representation of a neural network to a file. Note
 * that the resulting file is not necessarily portable across
 * platforms.
 */
int
net_bsave (const char *filename, const network_t *net)
{
  FILE *file;
  int result;

  assert (filename != NULL);
  assert (net != NULL);

  file = fopen (filename, "wb");
  result = net_fbprint (file, net);
  if (result < 0) {
    fclose (file);
    return result;
  }
  return fclose (file);
}

/*!\brief Read a network from a binary file.
 * \param filename Pointer to name of file to read from.
 * \return Pointer to the read neural network on success, NULL on failure.
 */
network_t *
net_bload (const char *filename)
{
  FILE *file;
  network_t *net;

  assert (filename != NULL);

  file = fopen (filename, "rb");
  net = net_fbscan (file);
  fclose (file);

  return net;
}

/****************************************
 * Input and Output
 ****************************************/

/*!\brief [Internal] Copy inputs to input layer of a network.
 */
static inline void
set_input (network_t *net, const float *input)
{
  int n;

  assert (net != NULL);
  assert (input != NULL);

  for (n = 0; n < net->input_layer->no_of_neurons; n++) {
    net->input_layer->neuron[n].output = input[n];
  }
}

/*!\brief [Interal] Copy outputs from output layer of a network.
 */
static inline void
get_output (const network_t *net, float *output)
{
  int n;
  
  assert (net != NULL);
  assert (output != NULL);

  for (n = 0; n < net->output_layer->no_of_neurons; n++) {
    output[n] = net->output_layer->neuron[n].output;
  }
}

/****************************************
 * Sigmoidal function
 ****************************************/

#if 1

#include "interpolation.c"

/*!\brief [Internal] Activation function of a neuron.
 */
static inline float
sigma (float x)
{
  int index;

  index = (int) ((x - min_entry) * one_over_interval);

  if (index <= 0) {
    return interpolation[0];
  } else if (index >= num_entries) {
    return interpolation[num_entries - 1];
  } else {
    return interpolation[index];
  }
}

#else /* reference implementation of sigma */

/*!\brief [Internal] Activation function of a neuron.
 */
static inline float
sigma (float x)
{
  return 1.0 / (1.0 + exp (-x));
}

#endif






/****************************************
 * Inverse Sigmoidal function
 ****************************************/

#if 1


/*!\brief [Internal] Activation function of a neuron.
 */
static inline float
inverse_sigma (float x)
{
  int index;

  index = (int) ((x - a_min_entry) * a_one_over_interval);

  if (index <= 0) {
    return a_interpolation[0];
  } else if (index >= a_num_entries) {
    return a_interpolation[a_num_entries - 1];
  } else {
    return a_interpolation[index];
  }
}

#else /* reference implementation of sigma */

/*!\brief [Internal] Activation function of a neuron.
 */
static inline float
inverse_sigma (float x)
{
return( - log(1.0 / x - 1.0));
}

#endif


/****************************************
 * Forward and Backward Propagation
 ****************************************/

/*!\brief [Internal] Forward propagate inputs from one layer to next layer.
Note - activation and absolute_Activation only need to be computed for
training,  not running.  So having a separate function for run only
would make the net work faster in run mode.
 */
static inline void
propagate_layer (layer_t *lower, layer_t *upper)
{
  int nu, nl;
  float value;
  float absolute_value;

  assert (lower != NULL);
  assert (upper != NULL);

  for (nu = 0; nu < upper->no_of_neurons; nu++) {
    value = 0.0;
    absolute_value=0.;
    for (nl = 0; nl <= lower->no_of_neurons; nl++) {
      float cur_value;
      cur_value = upper->neuron[nu].weight[nl] * lower->neuron[nl].output;
      value += cur_value;
      absolute_value += absf(cur_value);
    }
    upper->neuron[nu].activation = value;
    upper->neuron[nu].absolute_activation = value;
    upper->neuron[nu].output = sigma (value);
  }
}

/*!\brief [Internal] Forward propagate inputs through a network.
 */
static inline void
forward_pass (network_t *net)
{
  int l;

  assert (net != NULL);

  for (l = 1; l < net->no_of_layers; l++) {
    propagate_layer (&net->layer[l - 1], &net->layer[l]);
  }
}

/*!\brief Compute the output error of a network.
 * \param net Pointer to a neural network.
 * \param target Pointer to a sequence of floating point numbers.
 * \return Output error of the neural network.
 *
 * Before calling this routine, net_compute() should have been called to
 * compute the ouputs for given inputs. This routine compares the
 * actual output of the neural network (which is stored internally in
 * the neural network) and the intended output (in target). The return
 * value is the half the square of the Euclidean distance between the 
 * actual output and the target. This routine also prepares the network for
 * backpropagation training by storing (internally in the neural
 * network) the errors associated with each of the outputs. Note
 * that the targets shoud lie in the interval [0,1], since the outputs
 * of the neural network will always lie in the interval (0,1). */
float
net_compute_output_error (network_t *net, const float *target)
{
  int n;
  float activation, absolute_activation, error;

  assert (net != NULL);
  assert (target != NULL);

  net->global_error = 0.0;
  for (n = 0; n < net->output_layer->no_of_neurons; n++) {
    neuron_t *neuron;
    float target_activation;
    
    neuron =  net->output_layer->neuron + n;
    activation =  neuron->activation;
    target_activation = inverse_sigma(target[n]);
    
    if (neuron->stubborn) {
      if ((target_activation > 8.)&&(activation >= MAX_ENTRY)) 
        activation = MAX_ENTRY;
      else if ((target_activation < -8.) && (activation <= MIN_ENTRY))
        activation = MIN_ENTRY;
      }
    
    target_activation = inverse_sigma(target[n]);
    error = target_activation - activation; 
    neuron->error = error;
    /* now make a dummy error for the global error */
    error = target[n] - neuron->output;
    net->global_error += error * error;
  }
  net->global_error *= 0.5;

  return net->global_error;
}

/*!\brief Retrieve the output error of a network.
 * \param net Pointer to a neural network.
 * \return Output error of the neural network.
 *
 * Before calling this routine, net_compute() and
 * net_compute_output_error() should have been called to compute outputs
 * for given inputs and to acually compute the output error. This
 * routine merely returns the output error (which is stored internally
 * in the neural network).
 */
float
net_get_output_error (const network_t *net)
{
  assert (net != NULL);

  return net->global_error;
}

/*!\brief [Internal] Backpropagate error from one layer to previous layer.
 */
static inline void
backpropagate_layer (layer_t *lower, layer_t *upper,int upper_layer_number,
 network_t *net)
{
  int nl, nu;
  float output, error;
  float weight_chunk;
  float error_chunk;
  assert (lower != NULL);
  assert (upper != NULL);
  
  
  weight_chunk = 1.0 /((float)upper_layer_number);
  error_chunk = 1.0 - weight_chunk;
    /* the error is distributed between weights and errors
    as such:
     for a 3 layer node:
       layer 2 to layer 1 gets 50% in weights from 2 to 1,  and
        50% of the error is brought down to the lower layer.
       layer 1 to layer 0 gets 100% in weights from 1 to 0.
       We cannot say that there is any error on the input side.
     In other words,  50% of the error goes to change the top level weights
      and 50% to the bottom.
       
     for a 4 layer node:
       layer 3 gets 33% of the change in weights and 66% propogated down
         to layer 2.  Layer 2 gets 50% of the error in weights and 50%
	 to layer 1 (note - 50% of 66% is 33%).
	 Layer 1 gets 100% of the error to its own weights.
	 In other words,  the weight change gets distributed as 33%, 33% and 33%
     
       */
  
  for (nl = 0; nl <= lower->no_of_neurons; nl++) {
    float activation;
    float target_activation;
    neuron_t *lower_neuron = lower->neuron + nl;
    
    error = 0.0;
    for (nu = 0; nu < upper->no_of_neurons; nu++) {
      float new_weight;
      float one_over_absolute_activation; 
      
      neuron_t *upper_neuron = upper->neuron + nu;
      float absolute_activation = upper_neuron->absolute_activation;
      
      if (absolute_activation <= 0.) {
        absolute_activation=0.001; /*fudge */
	}
      one_over_absolute_activation = 1.0 / absolute_activation;
      
      new_weight = upper_neuron->weight[nl] + upper_neuron->error
         * upper_neuron->weight[nl] * 
        one_over_absolute_activation * lower_neuron->output *
	net->learning_rate * weight_chunk + net->momentum 
	 * upper_neuron->delta[nl];
 	
      if (absf(new_weight)<=0.0) {
        if (new_weight <0.) new_weight = -0.001;
	else new_weight = 0.001;
	}
	
      error += upper_neuron->error * error_chunk / new_weight;
        
      /* ok - so error is in the exp function units */
    }
    /* ok - so error is in the exp function units 
       we need to conver that to weight units */
    target_activation = inverse_sigma(error+lower_neuron->output);
    activation = lower_neuron->activation;
    if (lower_neuron->stubborn) {
      if ((target_activation > 8.)&&(activation >= MAX_ENTRY)) 
        activation = MAX_ENTRY;
      else if ((target_activation < -8.) && (activation <= MIN_ENTRY))
        activation = MIN_ENTRY;
      }
    lower_neuron->error = target_activation - activation;

  }
}


/*!\brief [Internal] Backpropagate output error through a network.
 */
static inline void
backward_pass (network_t *net)
{
  int l;

  assert (net != NULL);

  for (l = net->no_of_layers - 1; l > 1; l--) {
    backpropagate_layer (&net->layer[l - 1], &net->layer[l],l,net);
  }
}

/*!\brief [Internal] Adjust weights based on (backpropagated) output error.
 */
static inline void
adjust_weights (network_t *net)
{
  int l, nu, nl;
  float error, delta;
  
  assert (net != NULL);



  for (l = net->no_of_layers - 1; l > 1; l--) {
    float weight_chunk;
    float error_chunk;
  
    weight_chunk = 1.0 /((float)l);
    error_chunk = 1.0 - weight_chunk;
    /* the error is distributed between weights and errors
    as such:
     for a 3 layer node:
       layer 2 to layer 1 gets 50% in weights from 2 to 1,  and
        50% of the error is brought down to the lower layer.
       layer 1 to layer 0 gets 100% in weights from 1 to 0.
       We cannot say that there is any error on the input side.
     In other words,  50% of the error goes to change the top level weights
      and 50% to the bottom.
       
     for a 4 layer node:
       layer 3 gets 33% of the change in weights and 66% propogated down
         to layer 2.  Layer 2 gets 50% of the error in weights and 50%
	 to layer 1 (note - 50% of 66% is 33%).
	 Layer 1 gets 100% of the error to its own weights.
	 In other words,  the weight change gets distributed as 33%, 33% and 33%
     
       
       */
       
     
     
    
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      float one_over_absolute_activation;
      
      neuron_t *upper_neuron = net->layer[l].neuron + nu;
      float absolute_activation = upper_neuron->absolute_activation;
      
      /* hack the activation if it is really close to 0 */
      if (absolute_activation <= 0.0) absolute_activation = 0.001; 
      one_over_absolute_activation = 1.0 / absolute_activation;
      
 	      
      for (nl = 0; nl <= net->layer[l - 1].no_of_neurons; nl++) {
        float weight;
	neuron_t *lower_neuron = net->layer[l-1].neuron + nl;
	
        delta = upper_neuron->error
           * upper_neuron->weight[nl] * 
           one_over_absolute_activation * lower_neuron->output *
	    net->learning_rate * weight_chunk + net->momentum 
	     *  upper_neuron->delta[nl];
	     
        upper_neuron->weight[nl] += delta;
        upper_neuron->delta[nl] = delta;
      }
    }
  }
}

/****************************************
 * Evaluation and Training
 ****************************************/

/*!\brief Compute outputs of a network for given inputs.
 * \param net Pointer to a neural network.
 * \param input Pointer to sequence of floating point numbers.
 * \param output Pointer to sequence of floating point numbers or NULL.
 *
 * Compute outputs of a neural network for given inputs by forward
 * propagating the inputs through the layers. If output is non-NULL, the
 * outputs are copied to output (otherwise they are only stored
 * internally in the network). Note that the outputs of the neural
 * network will always lie in the interval (0,1); the caller will have to
 * rescale them if neccesary.
 */
void
net_compute (network_t *net, const float *input, float *output)
{
  assert (net != NULL);
  assert (input != NULL);

  set_input (net, input);
  forward_pass (net);
  if (output != NULL) {
    get_output (net, output);
  }
}

/*!\brief Train a network.
 * \param net Pointer to a neural network.
 *
 * Before calling this routine, net_compute() and
 * net_compute_output_error() should have been called to compute outputs
 * for given inputs and to prepare the neural network for training by
 * computing the output error. This routine performs the actual training
 * by backpropagating the output error through the layers.
 */
void
net_train (network_t *net)
{
  assert (net != NULL);
  backward_pass (net);
  adjust_weights (net);
}

/****************************************
 * Batch Training
 ****************************************/

/*!\brief [Internal] Adjust deltas based on (backpropagated) output error.
 * \param net Pointer to a neural network.
 */
static inline void
adjust_deltas_batch (network_t *net)
{
  int l, nu, nl;
  float error;

  assert (net != NULL);

  for (l = 1; l < net->no_of_layers; l++) {
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      error = net->layer[l].neuron[nu].error;
      for (nl = 0; nl <= net->layer[l - 1].no_of_neurons; nl++) {
        net->layer[l].neuron[nu].delta[nl] +=
          net->learning_rate * error * net->layer[l - 1].neuron[nl].output;
      }
    }
  }
}

/*!\brief [Internal] Adjust weights based on deltas determined by batch
 * training.
 * \param net Pointer to a neural network.
 */
static inline void
adjust_weights_batch (network_t *net)
{
  int l, nu, nl;

  assert (net != NULL);

  for (l = 1; l < net->no_of_layers; l++) {
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      for (nl = 0; nl <= net->layer[l - 1].no_of_neurons; nl++) {
        net->layer[l].neuron[nu].weight[nl] +=
          net->layer[l].neuron[nu].delta[nl] / net->no_of_patterns;
      }
    }
  }
}

/*!\brief Begin training in batch mode.
 * \param net Pointer to a neural network.
 *
 * Note that batch training does not care about momentum.
 */
void
net_begin_batch (network_t *net)
{
  assert (net != NULL);

  net->no_of_patterns = 0;
  net_reset_deltas (net);
}

/*!\brief Train a network in batch mode.
 * \param net Pointer to a neural network.
 *
 * Before calling this routine, net_begin_batch() should have been
 * called (at the start of the batch) to begin batch training.
 * Furthermore, for the current input/target pair, net_compute() and
 * net_compute_output_error() should have been called to compute outputs
 * for given the inputs and to prepare the neural network for training
 * by computing the output error using the given targets. This routine
 * performs the actual training by backpropagating the output error
 * through the layers, but does not change the weights. The weights
 * will be changed when (at the end of the batch) net_end_batch() is
 * called.
 */
void
net_train_batch (network_t *net)
{
  assert (net != NULL);

  net->no_of_patterns++;
  backward_pass (net);
  adjust_deltas_batch (net);
}

/*!\brief End training in batch mode adjusting weights.
 * \param net Pointer to a neural network.
 *
 * Adjust the weights in the neural network according to the average 
 * delta of all patterns in the batch.
 */
void
net_end_batch (network_t *net)
{
  assert (net != NULL);

  adjust_weights_batch (net);
}

/****************************************
 * Modification
 ****************************************/

/*!\brief Make small random changes to the weights of a network.
 * \param net Pointer to a neural network.
 * \param factor Floating point number.
 * \param range Floating point number.
 *
 * All weights in the neural network that are in absolute value smaller
 * than range become a random value from the interval [-range,range].
 * All other weights get multiplied by a random value from the interval
 * [1-factor,1+factor].
 */
void
net_jolt (network_t *net, float factor, float range)
{
  int l, nu, nl;

  assert (net != NULL);
  assert (factor >= 0.0);
  assert (range >= 0.0);

  /* modify weights */
  for (l = 1; l < net->no_of_layers; l++) {
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      for (nl = 0; nl <= net->layer[l - 1].no_of_neurons; nl++) {
        if (fabs (net->layer[l].neuron[nu].weight[nl]) < range) {
          net->layer[l].neuron[nu].weight[nl] =
            2.0 * range * ((float) random () / RAND_MAX - 0.5);
        } else {
          net->layer[l].neuron[nu].weight[nl] *=
            1.0 + 2.0 * factor * ((float) random () / RAND_MAX - 0.5);
        }
      }
    }
  }
}




/*!\brief Add neurons to a network.
 * \param net Pointer to a neural network.
 * \param layer Integer
 * \param neuron Integer
 * \param number Integer
 * \param range Floating point number
 */
void
net_add_neurons (network_t *net, int layer, int neuron, int number,
                 float range)
{
  int l, nu, nl, new_nu, new_nl, *arglist;
  network_t *new_net, *tmp_net;

  assert (net != NULL);
  assert (0 <= layer && layer < net->no_of_layers);
  assert (0 <= neuron);
  assert (number >= 0);
  assert (range >= 0.0);

  /* special case to conveniently add neurons at the end of the layer */
  if (neuron == -1) {
    neuron = net->layer[layer].no_of_neurons;
  }

  /* allocate memory for the new network */
  arglist = calloc (net->no_of_layers, sizeof (int));
  for (l = 0; l < net->no_of_layers; l++) {
    arglist[l] = net->layer[l].no_of_neurons;
  }
  arglist[layer] += number;
  new_net = net_allocate_l (net->no_of_layers, arglist);
  free (arglist);

  /* the new neuron will be connected with small, random weights */
  net_randomize (net, range);

  /* copy the original network's weights and deltas into the new one */
  for (l = 1; l < net->no_of_layers; l++) {
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      new_nu = (l == layer) && (nu >= neuron) ? nu + number : nu;
      for (nl = 0; nl <= net->layer[l - 1].no_of_neurons; nl++) {
        new_nl = (l == layer + 1) && (nl >= neuron) ? nl + number : nl;
        new_net->layer[l].neuron[new_nu].weight[new_nl] =
          net->layer[l].neuron[nu].weight[nl];
        new_net->layer[l].neuron[new_nu].delta[new_nl] =
          net->layer[l].neuron[nu].delta[nl];
      }
    }
  }

  /* copy the original network's constants into the new one */
  new_net->momentum = net->momentum;
  new_net->learning_rate = net->learning_rate;

  /* switch new_net and net, so it is possible to keep the same pointer */
  tmp_net = malloc (sizeof (network_t));
  memcpy (tmp_net, new_net, sizeof (network_t));
  memcpy (new_net, net, sizeof (network_t));
  memcpy (net, tmp_net, sizeof (network_t));
  free (tmp_net);

  /* free allocated memory */
  net_free (new_net);
}

/*!\brief Remove neurons from a network.
 * \param net Pointer to a neural network.
 * \param layer Integer
 * \param neuron Integer
 * \param number Integer
 */
void
net_remove_neurons (network_t *net, int layer, int neuron, int number)
{
  int l, nu, nl, orig_nu, orig_nl, *arglist;
  network_t *new_net, *tmp_net;

  assert (net != NULL);
  assert (0 <= layer && layer < net->no_of_layers);
  assert (0 <= neuron);
  assert (number >= 0);

  /* allocate memory for the new network */
  arglist = calloc (net->no_of_layers, sizeof (int));
  for (l = 0; l < net->no_of_layers; l++) {
    arglist[l] = net->layer[l].no_of_neurons;
  }
  arglist[layer] -= number;
  new_net = net_allocate_l (net->no_of_layers, arglist);
  free (arglist);

  /* copy the original network's weights and deltas into the new one */
  for (l = 1; l < new_net->no_of_layers; l++) {
    for (nu = 0; nu < new_net->layer[l].no_of_neurons; nu++) {
      orig_nu = (l == layer) && (nu >= neuron) ? nu + number : nu;
      for (nl = 0; nl <= new_net->layer[l - 1].no_of_neurons; nl++) {
        orig_nl = (l == layer + 1) && (nl >= neuron) ? nl + number : nl;
        new_net->layer[l].neuron[nu].weight[nl] =
          net->layer[l].neuron[orig_nu].weight[orig_nl];
        new_net->layer[l].neuron[nu].delta[nl] =
          net->layer[l].neuron[orig_nu].delta[orig_nl];
      }
    }
  }

  /* copy the original network's constants into the new one */
  new_net->momentum = net->momentum;
  new_net->learning_rate = net->learning_rate;

  /* switch new_net and net, so it is possible to keep the same pointer */
  tmp_net = malloc (sizeof (network_t));
  memcpy (tmp_net, new_net, sizeof (network_t));
  memcpy (new_net, net, sizeof (network_t));
  memcpy (net, tmp_net, sizeof (network_t));
  free (tmp_net);

  /* free allocated memory */
  net_free (new_net);
}

/*!\brief Copy a network.
 * \param net Pointer to a neural network.
 * \return Pointer to a copy of the neural network.
 */
network_t *
net_copy (const network_t *net)
{
  int l, nu, nl, *arglist;
  network_t *new_net;

  assert (net != NULL);

  /* allocate memory for the new network */
  arglist = calloc (net->no_of_layers, sizeof (int));
  for (l = 0; l < net->no_of_layers; l++) {
    arglist[l] = net->layer[l].no_of_neurons;
  }
  new_net = net_allocate_l (net->no_of_layers, arglist);
  free (arglist);

  /* copy the original network's weights and deltas into the new one */
  for (l = 1; l < net->no_of_layers; l++) {
    for (nu = 0; nu < net->layer[l].no_of_neurons; nu++) {
      for (nl = 0; nl <= net->layer[l - 1].no_of_neurons; nl++) {
        new_net->layer[l].neuron[nu].weight[nl] =
          net->layer[l].neuron[nu].weight[nl];
        new_net->layer[l].neuron[nu].delta[nl] =
          net->layer[l].neuron[nu].delta[nl];
      }
    }
  }

  /* copy the original network's constants into the new one */
  new_net->momentum = net->momentum;
  new_net->learning_rate = net->learning_rate;
  new_net->no_of_patterns = net->no_of_patterns;

  return new_net;
}




/*!\brief spawn a more intelligent child of a network
 * \param net Pointer to a neural network.
 * \return Pointer to a copy of the neural network.
 * 
 * This is similar to a copy,  except that the child has More nodes in the layers
 * and these nodes are choosen by random. 
 * The weights are adjusted so that the exact same output will appear between the parent and child.
 * each intermediate layer.  The weights of these layers are adjusted so that the 
 * child neural network will produce the same value as the parent neural network.
 * In fact,  this child network will probably train the same as the parent, although 4 times slower
 * unless you mutate the child slightly
 */
network_t *
net_spawn (const network_t *net,float ratio_to_add   /* ratio_to_add is between 0 and 1.  0 is dumb - does nothing.  1 doubles the nodes,  quadrupling the time */)
{
  int l; /* layer we are fixing */
  int  *arglist;
  network_t *new_net;
  int *lowermap;
  int *uppermap;
  float *lowerbias; /* between 0 and 1 - the bias for the given node for weights compared to the previous weights on the old network */
  float *upperbias;

  assert (net != NULL);
  
  /* if the layers are only 2,  this is the same as a copy,  so call that */
  if (net->no_of_layers <=2)
    return (net_copy(net));
    
  /* finesse ratio_to_Add */
  if (ratio_to_add <=0.) 
    return (net_copy(net));

  if (ratio_to_add >1.) {
    ratio_to_add = 1.;
    }
    
    

  /*init the maps
   the lower map is to link to the lower values properly. 
   The upper map is for the next stage.
   We do some tricks so that there are less calls to malloc - by mallocing the largest possilbe size fro both maps */
  {
    int maxsize;
    int i;
    
    maxsize=0;
    for (i=0;i<net->no_of_layers;i++) {
      int s= net->layer[i].no_of_neurons +   (int)( ((float)(net->layer[i].no_of_neurons)) * ratio_to_add); /* slightly off for input nodes, but good enough */
      if (s>maxsize) maxsize=s;
      }
    maxsize++; /* because of the bias */
    lowermap = calloc(maxsize,sizeof(int));
    uppermap = calloc(maxsize,sizeof(int));
    lowerbias = calloc(maxsize,sizeof(float));
    upperbias = calloc(maxsize,sizeof(float));
    
    }
    
    
  /* populate the lower map,  which will have the same number of inputs as the upper map */
  {
    int i;
    for (i=0;i<=net->layer[0].no_of_neurons;i++) { /* <= to include bias mapping */
      lowermap[i] = i;
      lowerbias[i] = 1.;
      }
    }
  


  /* allocate memory for the new network */
  arglist = calloc (net->no_of_layers, sizeof (int));
  
  arglist[0] = net->layer[0].no_of_neurons; /* input is the same */
  for (l = 1; l < net->no_of_layers-1; l++) {
    arglist[l] = net->layer[l].no_of_neurons+  (int)( ((float)(net->layer[l].no_of_neurons)) * ratio_to_add); /* middle is double */
  }
  arglist[net->no_of_layers-1] = net->layer[net->no_of_layers-1].no_of_neurons; /* output is the same */
  
  new_net = net_allocate_l (net->no_of_layers, arglist);
  free (arglist);



  for (l = 1; l < net->no_of_layers; l++) {
    /* copy the original network's weights and deltas into the new one, expanding as we go */
    if (l == net->no_of_layers -1) { /* if we are the output layer */
      /* much simpler,  we do not add output layers */
      int i;
      for (i=0;i<=net->layer[l].no_of_neurons;i++) { /* <= because of bias */
        uppermap[i] = i;
        upperbias[i] = 1.;
        }
      }
    else /* need to expand   */
      { /* first node 1 to node 0
      here we add the replicated copy nodes -- with different weights
      After all,  these nodes have learned things,  so their weights should be reinforced
      It might be better in the future to deal with the relative usage of a node on the outputs to 
      determine what we should reinforce
      Say if node 3 has a sum of weights of 10 and node 5 has a sum of weights of 100
      then node 5 would be more likely a candidate to be replicated.
      But for now,  This will do a promote everyine; demote the bad eggs
      and the demotion will be in the cull function not yet written.
      Also,  for the copies of the nodes,  all that is needed for the result to be the same,  is that the weights to the parents are equivilant.
      The way we are going to do this - is to have a random weight factor - .125 - .875.  And its duplicate will hae 1-weight.
      along the way. */
      
      int oldpointer;
      int newpointer;
      float chance;
    
      chance=ratio_to_add; /* first we will do a 10 percent chance.  But as we go,  we will add chance,   as the number is not done.*/
      oldpointer=0;
      for (newpointer=0;newpointer < new_net->layer[l].no_of_neurons;newpointer++) {
        if ( ((float)(random()) / RAND_MAX  ) < chance) { /* we are going to do this */
          float r1;
  	  r1 =    (float)((int)(( (float)(random()) / RAND_MAX) * 7.) + 1) * 0.125; /* 1/8 to 7/8 random */ 
      	
          uppermap[newpointer] = oldpointer;
	  upperbias[newpointer] = r1;
	  newpointer++;
	  uppermap[newpointer] = oldpointer;
	  upperbias[newpointer] = (1.0 - r1);	
          }
        else {
          uppermap[newpointer] = oldpointer; /* same as before.  no expansion */
	  upperbias[newpointer] = 1.; /* expansion */
	  }
	/*hacky here.  Don't let oldpointer go beyond range
	  but this could cause oldpointer to be replicated multiple times
	  The proper way would be to random fill an array - hash table
	  it would take long for a all full array,  so I guess
	  one would do up to 50% full,  and do >50% as empty 
	  */
	if (oldpointer < net->layer[l].no_of_neurons) oldpointer++; 
	
        if (oldpointer >= net->layer[l].no_of_neurons-1) chance=1.; /* make sure that we have all the nodes */
        if ( (newpointer-oldpointer) >=  (new_net->layer[l].no_of_neurons - net->layer[l].no_of_neurons) ) chance=-1.; /* if we filled quota,  stop */
        } /* for */
      uppermap[new_net->layer[l].no_of_neurons]=net->layer[l].no_of_neurons; /* bias */
      upperbias[new_net->layer[l].no_of_neurons]=1.;
      } /* building the upper map and bias */
    
    { /* copy the values */
      int nu; /* upper enumerator */
      int nl; /* lower enumerator */

      for (nu = 0; nu < new_net->layer[l].no_of_neurons; nu++) {
        int oldnu;
        oldnu = uppermap[nu];
        for (nl = 0; nl <= new_net->layer[l-1].no_of_neurons; nl++) {
          int oldnl;
 	  oldnl = lowermap[nl];
          new_net->layer[l].neuron[nu].weight[nl] =
            net->layer[l].neuron[oldnu].weight[oldnl]*lowerbias[nl];
	
          new_net->layer[l].neuron[nu].delta[nl] =
            net->layer[l].neuron[oldnu].delta[oldnl]*lowerbias[nl];
          } /* for each lower neuron */
        } /* for each upper neuron */
      } /* copy the values */
    
    { /* now swap uppermap and upperbias to loweremap and lowerbias */
      int *t;
      float *t2;
      t=uppermap;uppermap=lowermap;lowermap=t;
      t2=upperbias;upperbias=lowerbias;lowerbias=t2;
      }
    
    } /* for each layer from 1 to the output layer */
  new_net->momentum = net->momentum;
  new_net->learning_rate = net->learning_rate;
  new_net->no_of_patterns = net->no_of_patterns;

  free(lowermap);
  free(uppermap);
  free(lowerbias);
  free(upperbias);
  return new_net;
}



/*!\brief Overwrite one network with another.
 * \param dest Pointer to a neural network.
 * \param src Pointer to a neural network.
 *
 * The neural network dest becomes a copy of the neural network src.
 * Note that dest must be an allocated neural network and its original
 * contents is discarded (with net_free()).
 */
void
net_overwrite (network_t *dest, const network_t *src)
{
  network_t *new_net, *tmp_net;

  assert (dest != NULL);
  assert (src != NULL);

  new_net = net_copy (src);

  /* switch new_net and dest, so it is possible to keep the same pointer */
  tmp_net = malloc (sizeof (network_t));
  memcpy (tmp_net, new_net, sizeof (network_t));
  memcpy (new_net, dest, sizeof (network_t));
  memcpy (dest, tmp_net, sizeof (network_t));
  free (tmp_net);

  net_free (new_net);
}




