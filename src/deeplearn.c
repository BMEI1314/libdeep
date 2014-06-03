/*
 libdeep - a library for deep learning
 Copyright (C) 2013  Bob Mottram <bob@robotics.uk.to>

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:
 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
 3. Neither the name of the University nor the names of its contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.
 .
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE HOLDERS OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "deeplearn.h"

/**
* @brief Update the learning history
* @param learner Deep learner object
*/
static void deeplean_update_history(deeplearn * learner)
{
    int i;
    float error_value;

    learner->history_ctr++;
    if (learner->history_ctr >= learner->history_step) {
        error_value = learner->BPerror;
        if (error_value == DEEPLEARN_UNKNOWN_ERROR) {
            error_value = 0;
        }

        learner->history[learner->history_index] =
            error_value;
        learner->history_index++;
        learner->history_ctr = 0;

        if (learner->history_index >= DEEPLEARN_HISTORY_SIZE) {
            for (i = 0; i < learner->history_index; i++) {
                learner->history[i/2] = learner->history[i];
            }
            learner->history_index /= 2;
            learner->history_step *= 2;
        }
    }
}

/**
* @brief Initialise a deep learner
* @param learner Deep learner object
* @param no_of_inputs The number of input units
* @param no_of_hiddens The number of hidden units within each layer
* @param hidden_layers The number of hidden layers
* @param no_of_outputs The number of output units
* @param error_threshold Minimum training error for each hidden layer plus
*        the output layer
* @param random_seed Random number generator seed
*/
void deeplearn_init(deeplearn * learner,
                    int no_of_inputs,
                    int no_of_hiddens,
                    int hidden_layers,
                    int no_of_outputs,
                    float error_threshold[],
                    unsigned int * random_seed)
{
    /* has not been trained */
    learner->training_complete = 0;

    /* create the error thresholds for each layer */
    learner->error_threshold =
        (float*)malloc((hidden_layers+1)*sizeof(float));
    memcpy((void*)learner->error_threshold,
           (void*)error_threshold,
           (hidden_layers+1)*sizeof(float));

    /* clear history */
    learner->history_index = 0;
    learner->history_ctr = 0;
    learner->history_step = 1;

    /* clear the number of training itterations */
    learner->itterations = 0;

    /* set the current layer being trained */
    learner->current_hidden_layer = 0;

    /* create the network */
    learner->net = (bp*)malloc(sizeof(bp));

    /* initialise the network */
    bp_init(learner->net,
            no_of_inputs, no_of_hiddens,
            hidden_layers, no_of_outputs,
            random_seed);

    /* create the autocoder */
    learner->autocoder = (bp*)malloc(sizeof(bp));
    bp_create_autocoder(learner->net,
                        learner->current_hidden_layer,
                        learner->autocoder);

    learner->BPerror = DEEPLEARN_UNKNOWN_ERROR;
}

/**
* @brief Feeds the input values through the network towards the outputs
* @param learner Deep learner object
*/
void deeplearn_feed_forward(deeplearn * learner)
{
    bp_feed_forward(learner->net);
}

/**
* @brief Performs training initially using autocoders for each hidden
*        layer and eventually for the entire network.
* @param learner Deep learner object
*/
void deeplearn_update(deeplearn * learner)
{
    float max_backprop_error = 0;

    /* only continue if training is not complete */
    if (learner->training_complete == 1) return;

    /* get the maximum backprop error after which a layer
        will be considered to have been trained */
    max_backprop_error =
        learner->error_threshold[learner->current_hidden_layer];

    /* pretraining */
    if (learner->current_hidden_layer <
        learner->net->HiddenLayers) {

        /* train the autocoder */
        bp_pretrain(learner->net, learner->autocoder,
                    learner->current_hidden_layer);

        /* update the backprop error value */
        learner->BPerror = learner->autocoder->BPerrorAverage;

        /* If below the error threshold.
            Only do this after a minimum number of itterations
            in order to allow the running average to stabilise */
        if ((learner->BPerror != DEEPLEARN_UNKNOWN_ERROR) &&
            (learner->BPerror < max_backprop_error) &&
            (learner->autocoder->itterations > 100)) {

            /* copy the hidden units */
            bp_update_from_autocoder(learner->net,
                                     learner->autocoder,
                                     learner->current_hidden_layer);

            /* delete the existing autocoder */
            bp_free(learner->autocoder);
            free(learner->autocoder);
            learner->autocoder = 0;

            /* advance to the next hidden layer */
            learner->current_hidden_layer++;

            /* if not the final hidden layer */
            if (learner->current_hidden_layer <
                learner->net->HiddenLayers) {

                /* make a new autocoder */
                learner->autocoder = (bp*)malloc(sizeof(bp));
                bp_create_autocoder(learner->net,
                                    learner->current_hidden_layer,
                                    learner->autocoder);
            }

            /* reset the error value */
            learner->BPerror = DEEPLEARN_UNKNOWN_ERROR;
        }
    }
    else {
        /* ordinary network training */
        bp_update(learner->net);

        /* update the backprop error value */
        learner->BPerror = learner->net->BPerrorAverage;

        /* set the training completed flag */
        if (learner->BPerror < max_backprop_error) {
            learner->training_complete = 1;
        }
    }

    /* record the history of error values */
    deeplean_update_history(learner);

    /* increment the number of itterations */
    if (learner->net->itterations < UINT_MAX) {
        learner->net->itterations++;
    }
}

/**
* @brief Deallocates memory for the given deep learner object
* @param learner Deep learner object
*/
void deeplearn_free(deeplearn * learner)
{
    /* free the learner */
    bp_free(learner->net);
    free(learner->net);

    /* free the error thresholds */
    free(learner->error_threshold);

    /* free the autocoder */
    if (learner->autocoder != 0) {
        bp_free(learner->autocoder);
        free(learner->autocoder);
    }
}

/**
* @brief Sets the value of an input to the network
* @param learner Deep learner object
* @param index Index number of the input unit
* @param value Value to set the input unit to in the range 0.0 to 1.0
*/
void deeplearn_set_input(deeplearn * learner, int index, float value)
{
    bp_set_input(learner->net, index, value);
}

/**
* @brief Sets the value of an output unit
* @param learner Deep learner object
* @param index Index number of the output unit
* @param value Value to set the output unit to in the range 0.0 to 1.0
*/
void deeplearn_set_output(deeplearn * learner, int index, float value)
{
    bp_set_output(learner->net, index, value);
}

/**
* @brief Returns the value of an output unit
* @param learner Deep learner object
* @param index Index number of the output unit
* @return Value of the output unit to in the range 0.0 to 1.0
*/
float deeplearn_get_output(deeplearn * learner, int index)
{
    return bp_get_output(learner->net, index);
}

/**
* @brief Saves the given deep learner object to a file
* @param fp File pointer
* @param learner Deep learner object
* @return Non-zero value on success
*/
int deeplearn_save(FILE * fp, deeplearn * learner)
{
    int retval,val;

    retval = fwrite(&learner->training_complete, sizeof(int), 1, fp);
    retval = fwrite(&learner->itterations, sizeof(unsigned int), 1, fp);
    retval = fwrite(&learner->current_hidden_layer, sizeof(int), 1, fp);
    retval = fwrite(&learner->BPerror, sizeof(float), 1, fp);

    retval = bp_save(fp, learner->net);
    if (learner->autocoder != 0) {
        val = 1;
        retval = fwrite(&val, sizeof(int), 1, fp);
        retval = bp_save(fp, learner->autocoder);
    }
    else {
        val = 0;
        retval = fwrite(&val, sizeof(int), 1, fp);
    }

    /* save error thresholds */
    retval = fwrite(learner->error_threshold, sizeof(float),
                    learner->net->HiddenLayers+1, fp);

    /* save the history */
    retval = fwrite(&learner->history_index, sizeof(int), 1, fp);
    retval = fwrite(&learner->history_ctr, sizeof(int), 1, fp);
    retval = fwrite(&learner->history_step, sizeof(int), 1, fp);
    retval = fwrite(learner->history, sizeof(float),
                    learner->history_index, fp);

    return retval;
}

/**
* @brief Loads a deep learner object from file
* @param fp File pointer
* @param learner Deep learner object
* @param random_seed Random number generator seed
* @return Non-zero value on success
*/
int deeplearn_load(FILE * fp, deeplearn * learner,
                   unsigned int * random_seed)
{
    int retval,val=0;

    retval = fread(&learner->training_complete, sizeof(int), 1, fp);
    retval = fread(&learner->itterations, sizeof(unsigned int), 1, fp);
    retval = fread(&learner->current_hidden_layer, sizeof(int), 1, fp);
    retval = fread(&learner->BPerror, sizeof(float), 1, fp);

    learner->net = (bp*)malloc(sizeof(bp));
    retval = bp_load(fp, learner->net, random_seed);
    retval = fread(&val, sizeof(int), 1, fp);
    if (val == 1) {
        learner->autocoder = (bp*)malloc(sizeof(bp));
        retval = bp_load(fp, learner->autocoder, random_seed);
    }
    else {
        learner->autocoder = 0;
    }

    /* load error thresholds */
    learner->error_threshold =
        (float*)malloc((learner->net->HiddenLayers+1)*
                       sizeof(float));
    retval = fread(learner->error_threshold, sizeof(float),
                   learner->net->HiddenLayers+1, fp);

    /* load the history */
    retval = fread(&learner->history_index, sizeof(int), 1, fp);
    retval = fread(&learner->history_ctr, sizeof(int), 1, fp);
    retval = fread(&learner->history_step, sizeof(int), 1, fp);
    retval = fread(learner->history, sizeof(float),
                   learner->history_index, fp);

    return retval;
}

/**
* @brief Compares two deep learners and returns a greater
*        than zero value if they are the same
* @param learner1 First deep learner object
* @param learner2 Second deep learner object
* @return Greater than zero if the two learners are the same
*/
int deeplearn_compare(deeplearn * learner1,
                      deeplearn * learner2)
{
    int retval,i;

    if (learner1->current_hidden_layer !=
        learner2->current_hidden_layer) {
        return -1;
    }
    if (learner1->BPerror != learner2->BPerror) {
        return -2;
    }
    retval = bp_compare(learner1->net,learner2->net);
    if (retval < 1) return -3;
    if ((learner1->autocoder==0) !=
        (learner2->autocoder==0)) {
        return -4;
    }
    if (learner1->history_index !=
        learner2->history_index) {
        return -5;
    }
    if (learner1->history_ctr !=
        learner2->history_ctr) {
        return -6;
    }
    if (learner1->history_step !=
        learner2->history_step) {
        return -7;
    }
    for (i = 0; i < learner1->history_index; i++) {
        if (learner1->history[i] !=
            learner2->history[i]) {
            return -8;
        }
    }
    if (learner1->itterations !=
        learner2->itterations) {
        return -9;
    }
    for (i = 0; i < learner1->net->HiddenLayers+1; i++) {
        if (learner1->error_threshold[i] !=
            learner2->error_threshold[i]) {
            return -10;
        }
    }
    return 1;
}

/**
* @brief Uses gnuplot to plot the training error for the given learner
* @param learner Deep learner object
* @param filename Filename for the image to save as
* @param title Title of the graph
* @param image_width Width of the image in pixels
* @param image_height Height of the image in pixels
* @return zero on success
*/
int deeplearn_plot_history(deeplearn * learner,
                           char * filename, char * title,
                           int image_width, int image_height)
{
    int index,retval;
    FILE * fp;
    char data_filename[256];
    char plot_filename[256];
    char command_str[256];
    float value;
    float max_value = 0.01f;

    sprintf(data_filename,"%s%s",DEEPLEARN_TEMP_DIRECTORY,"libgpr_data.dat");
    sprintf(plot_filename,"%s%s",DEEPLEARN_TEMP_DIRECTORY,"libgpr_data.plot");

    /* save the data */
    fp = fopen(data_filename,"w");
    if (!fp) return -1;
    for (index = 0; index < learner->history_index; index++) {
        value = learner->history[index];
        fprintf(fp,"%d    %.10f\n",
                index*learner->history_step,value);
        /* record the maximum error value */
        if (value > max_value) {
            max_value = value;
        }
    }
    fclose(fp);

    /* create a plot file */
    fp = fopen(plot_filename,"w");
    if (!fp) return -1;
    fprintf(fp,"%s","reset\n");
    fprintf(fp,"set title \"%s\"\n",title);
    fprintf(fp,"set xrange [0:%d]\n",
            learner->history_index*learner->history_step);
    fprintf(fp,"set yrange [0:%f]\n",max_value*102/100);
    fprintf(fp,"%s","set lmargin 9\n");
    fprintf(fp,"%s","set rmargin 2\n");
    fprintf(fp,"%s","set xlabel \"Time Step\"\n");
    fprintf(fp,"%s","set ylabel \"Training Error\"\n");

    fprintf(fp,"%s","set grid\n");
    fprintf(fp,"%s","set key right top\n");

    fprintf(fp,"set terminal png size %d,%d\n",
            image_width, image_height);
    fprintf(fp,"set output \"%s\"\n", filename);
    fprintf(fp,"plot \"%s\" using 1:2 notitle with lines\n",
            data_filename);
    fclose(fp);

    /* run gnuplot using the created files */
    sprintf(command_str,"gnuplot %s", plot_filename);
    retval = system(command_str); /* I assume this is synchronous */

    /* remove temporary files */
    sprintf(command_str,"rm %s %s", data_filename,plot_filename);
    retval = system(command_str);
    return retval;
}

/**
* @brief Updates the input units from a patch within a larger image
* @param learner Deep learner object
* @param img Image buffer (1 byte per pixel)
* @param image_width Width of the image in pixels
* @param image_height Height of the image in pixels
* @param tx Top left x coordinate of the patch
* @param ty Top left y coordinate of the patch
*/
void deeplearn_inputs_from_image_patch(deeplearn * learner,
                                       unsigned char * img,
                                       int image_width, int image_height,
                                       int tx, int ty)
{
    bp_inputs_from_image_patch(learner->net,
                               img, image_width, image_height,
                               tx, ty);
}

/**
* @brief Updates the input units from an image
* @param learner Deep learner object
* @param img Image buffer (1 byte per pixel)
* @param image_width Width of the image in pixels
* @param image_height Height of the image in pixels
*/
void deeplearn_inputs_from_image(deeplearn * learner,
                                 unsigned char * img,
                                 int image_width, int image_height)
{
    bp_inputs_from_image(learner->net, img, image_width, image_height);
}

/**
* @brief Sets the learning rate
* @param learner Deep learner object
* @param rate the learning rate in the range 0.0 to 1.0
*/
void deeplearn_set_learning_rate(deeplearn * learner, float rate)
{
    learner->net->learningRate = rate;
    if (learner->autocoder != 0) {
        learner->autocoder->learningRate = rate;
    }
}

/**
* @brief Sets the percentage of units which drop out during training
* @param learner Deep learner object
* @param dropout_percent Percentage of units which drop out in the range 0 to 100
*/
void deeplearn_set_dropouts(deeplearn * learner, float dropout_percent)
{
    learner->net->DropoutPercent = dropout_percent;
    if (learner->autocoder != 0) {
        learner->autocoder->DropoutPercent = dropout_percent;
    }
}
