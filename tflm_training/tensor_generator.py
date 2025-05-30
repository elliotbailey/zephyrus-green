
import tensorflow as tf
import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Input, Conv1D, Flatten, Dense
from tensorflow.keras.layers import Dropout, BatchNormalization
from tensorflow.keras.callbacks import EarlyStopping


# Constants
NUM_CLASSES = 4
INPUT_LENGTH = 30

# Load and preprocess dataset
data = pd.read_csv("new_data.csv")
x = data.drop(columns=['label']).values.astype(np.float32)

v_arr = []
a_arr = []

for i in range(x.shape[0]):
    for j in range(np.size(x[i])):
        x[i][j] = min(3, x[i][j])
    sample_x = x[i]
    sample_v = np.gradient(sample_x)
    sample_a = np.gradient(sample_v)
    v_arr.append(sample_v)
    a_arr.append(sample_a)

v = np.array(v_arr)
a = np.array(a_arr)
x_v_a = np.stack([x, v, a], axis=-1)

y = data['label'].values.astype(np.int32) - 1

y_cat = tf.keras.utils.to_categorical(y, NUM_CLASSES)


x_train, x_test, y_train, y_test = train_test_split(x_v_a, y_cat, test_size=0.8, random_state=41, stratify=y)


model = Sequential([
    Input(shape=(30, 3)),
    Conv1D(32, kernel_size=3, activation='relu'),         
    Conv1D(32, kernel_size=3, activation='relu'),          
    Flatten(),
    Dense(128, activation='relu'),                                                    
    Dense(NUM_CLASSES, activation='softmax')               
])

model.compile(optimizer='adam', loss='categorical_crossentropy', metrics=['accuracy'])

# Training the model
model.fit(x_train, y_train, epochs=20, batch_size=16)

# Evaluating the model
loss, acc = model.evaluate(x_test, y_test)
print(f"Accuracy: {acc:.4f}")

# Define representative dataset
def representative_dataset():
    for i in range(100):
        yield [x_train[i:i+1]]

converter = tf.lite.TFLiteConverter.from_keras_model(model)

# Set up quantization
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.uint8
converter.inference_output_type = tf.uint8

# Convert
tflite_model = converter.convert()

# Save to file
with open('model_3.tflite', 'wb') as f:
    f.write(tflite_model)