import pandas
import numpy

orig_data = pandas.read_csv('new_data.csv')

# Time series data input
x = orig_data.iloc[:, :-1].values

# Output category
y = orig_data.iloc[:, -1].values

std_dev = 0.03

x_noise = numpy.round(x + numpy.random.normal(0, std_dev, x.shape), 2)

x_noise = numpy.clip(x_noise, 0.05, None)

noisified_data = pandas.DataFrame(x_noise, columns=orig_data.columns[:-1])
noisified_data['label'] = y
noisified_data.to_csv('new_data_noisified.csv', index=False)

print('Augmented data saved with shape:', noisified_data.shape)
