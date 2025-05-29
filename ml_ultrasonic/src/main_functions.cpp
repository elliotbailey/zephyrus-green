#include "main_functions.h"
#include "ul_model_data.h"
#include "ultrasonic_handler.h"
#include <tensorflow/lite/micro/micro_log.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/schema/schema_generated.h>

/* Globals, used for compatibility with Arduino-style sketches. */
namespace {
	const tflite::Model *model = nullptr;
	tflite::MicroInterpreter *interpreter = nullptr;
	TfLiteTensor *model_input = nullptr;
	int input_length;

	/* Create an area of memory to use for input, output, and intermediate arrays.
	* The size of this will depend on the model you're using, and may need to be
	* determined by experimentation.
	*/
	constexpr int kTensorArenaSize = 20 * 1024;
	uint8_t tensor_arena[kTensorArenaSize];
} /* namespace */

/* The name of this function is important for Arduino compatibility. */
void setup(void)
{
	/* Map the model into a usable data structure. This doesn't involve any
	 * copying or parsing, it's a very lightweight operation.
	 */
	 
	model = tflite::GetModel(model_tflite);
	if (model->version() != TFLITE_SCHEMA_VERSION) {
		printf("Model provided is schema version %d not equal "
				    "to supported version %d.",
				    model->version(), TFLITE_SCHEMA_VERSION);
		return;
	}

	/* Pull in only the operation implementations we need.
	 * This relies on a complete list of all the ops needed by this graph.
	 * An easier approach is to just use the AllOpsResolver, but this will
	 * incur some penalty in code space for op implementations that are not
	 * needed by this graph.
	 */
	
	static tflite::MicroMutableOpResolver < 10 > micro_op_resolver; /* NOLINT */
	micro_op_resolver.AddConv2D();
	micro_op_resolver.AddRelu();
	micro_op_resolver.AddFullyConnected();
	micro_op_resolver.AddSoftmax();
	micro_op_resolver.AddReshape();
	micro_op_resolver.AddQuantize();
	micro_op_resolver.AddDequantize();
	micro_op_resolver.AddExpandDims();

	/* Build an interpreter to run the model with. */
	static tflite::MicroInterpreter static_interpreter(
		model, micro_op_resolver, tensor_arena, kTensorArenaSize);
	interpreter = &static_interpreter;

	/* Allocate memory from the tensor_arena for the model's tensors. */
	interpreter->AllocateTensors();

	/* Obtain pointer to the model's input tensor. */
	model_input = interpreter->input(0);
	if (!model_input) {
    	printf("model_input is NULL\n");
    	return;
	}
	if ((model_input->dims->size != 3) ||
	    (model_input->dims->data[0] != 1) ||
	    (model_input->dims->data[1] != 30) ||
        (model_input->dims->data[2] != 3) ||
	    (model_input->type != kTfLiteUInt8)) {
		printf("model_input->type = %d (expected %d)\n", model_input->type, kTfLiteUInt8);
		printf("Bad input tensor parameters in model\n");
		for (int i = 0; i < model_input->dims->size; i++) {
    		printf("%d ", model_input->dims->data[i]);
		}
		return;
	}

	input_length = model_input->bytes / sizeof(int8_t);
	
	ultrasonic_init();
}

void loop(void)
{
    static float ul_buff[30];
    static int ref_index = 0;
	
	ul_buff[ref_index++] = ultrasonic_read();
	//printf("%f\n", ul_buff[ref_index - 1]);

	//printf("%d, %f\n", ref_index - 1, ul_buff[ref_index - 1]);
	
    if (ref_index < 30) {
        // Needs more sampling to complete the time series
        return;
    }

    // Full time series collected
    ref_index = 0;

	// Calculate necessary information from time series
	static float vel[30];
	for (int i = 0; i < 29; i++) {
		vel[i] = ul_buff[i + 1] - ul_buff[i];
	}
	vel[29] = vel[28];
	static float accel[30];
	for (int i = 0; i < 28; i++) {
		accel[i] = vel[i + 1] - vel[i];
	}
	accel[28] = accel[27];
	accel[29] = accel[28];
	
    // Quantisation
    for (int i = 0; i < 30; i++) {
        int32_t quantized_value = static_cast<int32_t>(round(ul_buff[i] / model_input->params.scale) + model_input->params.zero_point);
        // Clip Quantised Values
        quantized_value = std::max(-128, quantized_value);
        quantized_value = std::min(127, quantized_value);
        model_input->data.int8[i] = static_cast<int8_t>(quantized_value);

		quantized_value = static_cast<int32_t>(round(vel[i] / model_input->params.scale) + model_input->params.zero_point);
        // Clip Quantised Values
        quantized_value = std::max(-128, quantized_value);
        quantized_value = std::min(127, quantized_value);
        model_input->data.int8[i + 30] = static_cast<int8_t>(quantized_value);

		quantized_value = static_cast<int32_t>(round(accel[i] / model_input->params.scale) + model_input->params.zero_point);
        // Clip Quantised Values
        quantized_value = std::max(-128, quantized_value);
        quantized_value = std::min(127, quantized_value);
        model_input->data.int8[i + 60] = static_cast<int8_t>(quantized_value);
    }
	

	/* Run inference, and report any error */
	
	TfLiteStatus invoke_status = interpreter->Invoke();
	if (invoke_status != kTfLiteOk) {
		printf("Invoke failed on index: %d\n", ref_index);
		return;
	}
	TfLiteTensor* output = interpreter->output(0);
	int8_t* quantized_prob_dist = output->data.int8;
	float dequantized_prob_dist[3] = {0};
    int max_likelihood_index = 0;
	//printf("Zero Point: %d\n", output->params.zero_point);
	//printf("Scale: %f\n", output->params.scale);
	for (int i = 0; i < 3; i++) {
		dequantized_prob_dist[i] = output->params.scale * (quantized_prob_dist[i] - output->params.zero_point);
		//printf("Prob-Dist: %d, %d, %f\n", i, quantized_prob_dist[i], dequantized_prob_dist[i]);
	}
    for (int i = 0; i < 3; i++) {
        if ((dequantized_prob_dist[i] > dequantized_prob_dist[max_likelihood_index]) && i != 2) {
            max_likelihood_index = i;
        }
		//printf("Prob-Dist: %d, %d\n", i, quantized_prob_dist[i]);
		
    }
    
    int selected_category = max_likelihood_index + 1;
	if (quantized_prob_dist[2] > 2) {
		selected_category = 3;
	} else if (quantized_prob_dist[1] > -6) {
		selected_category = 2;
	} else {
		selected_category = 1;
	}
	//printf("selected=%d\n", selected_category);
	ultrasonic_publish(selected_category);
	if (selected_category == 1) {
        // Swipe down towards
        printf("Lower Volume\n");
    } else if (selected_category == 2) {
        // Swipe up away
        printf("Higher Volume\n");
    } else if (selected_category == 3) {
        // Swipe across
        printf("No Gesture Detected ...\n");
    }
    // Else remain IDLE
}