import torch
from torchview import draw_graph
from torch import nn
import torch.nn.functional as F
from torch.export import export, export_for_training, ExportedProgram
from executorch.exir import ExecutorchBackendConfig, ExecutorchProgramManager
import executorch.exir as exir
import pytorch_lightning as pl
from pytorch_lightning.callbacks import ModelCheckpoint
from torch.utils.data import DataLoader, TensorDataset
from sklearn.preprocessing import OneHotEncoder
import pandas as pd
import glob
import os
import torchmetrics
import numpy as np
import matplotlib.pyplot as plt
from sklearn.model_selection import train_test_split

# Function to downsample 600 values into 100 values using averaging
def downsample_last_600(df):
    """
    Downsamples the last 600 columns of a DataFrame into 100 columns by averaging groups of 6 values.
    This function works independently of column names, using the last 600 columns.
    """
    if df.shape[1] < 600:
        print("Error: DataFrame has fewer than 600 columns")
        return df

    # Select the last 600 columns
    last_600_columns = df.iloc[:, -600:].to_numpy()

    # Reshape and downsample using mean (grouping every 6 values)
    downsampled_values = last_600_columns.reshape(df.shape[0], 100, 6).mean(axis=2)

    # Create new column names for downsampled data
    new_columns = [f"downsampled_{i}" for i in range(100)]

    # Create a new DataFrame with downsampled values
    downsampled_df = pd.DataFrame(downsampled_values, columns=new_columns, index=df.index)

    # Drop the last 600 columns from the original DataFrame and add the downsampled data
    df = pd.concat([df.iloc[:, :-600], downsampled_df], axis=1)

    return df

def split_data(df):
    # Shuffle the entire dataset before splitting
    df = df.sample(frac=1, random_state=42).reset_index(drop=True)
    
    # First split: 80% train + val, 20% test
    analysis_df, test_df = train_test_split(df, test_size=0.2, random_state=42, stratify=df['Heat'])
    
    # Second split: 80% train, 20% validation (without overlap)
    train_df, val_df = train_test_split(analysis_df, test_size=0.2, random_state=42)
    
    return train_df, val_df, test_df

def df_to_tensor(df, y_column):
    x = torch.tensor(df.iloc[:, -100:].values, dtype=torch.float32).unsqueeze(1)  # Last 100 columns  # Add channel dimension
   # Extract and reshape target column
    y = df[[y_column]].values  # Ensure y is 2D for OneHotEncoder

    # Apply one-hot encoding
    encoder = OneHotEncoder(sparse_output=False)  # Convert to dense array
    y_encoded = encoder.fit_transform(y)  # Convert categorical to one-hot

    # Convert one-hot encoded y to tensor
    y_tensor = torch.tensor(y_encoded, dtype=torch.float32)

    return x,y_tensor

def plot_model_input(df):
    # Extract the last 100 columns for y-axis values
    y_columns = df.columns[-100:]

    # Extract time column (assuming the first column represents time)
    time_column = df.columns[0]

    # Separate data for HEAT=1 and HEAT=0
    df_heat_0 = df[df["Heat"] == 0][y_columns]
    df_heat_1 = df[df["Heat"] == 1][y_columns]

    # Compute mean and standard deviation for both HEAT classes
    y_mean_heat_0 = df_heat_0.mean().to_numpy(dtype=np.float64)
    y_std_heat_0 = df_heat_0.std().to_numpy(dtype=np.float64)

    y_mean_heat_1 = df_heat_1.mean().to_numpy(dtype=np.float64)
    y_std_heat_1 = df_heat_1.std().to_numpy(dtype=np.float64)

    x_values = np.linspace(0, 10, 100)  # 100 points over 10 minutes

    # Plot mean with standard deviation for both HEAT = 0 and HEAT = 1
    plt.figure(figsize=(10, 6))

    # Plot HEAT = 0
    plt.plot(x_values, y_mean_heat_0, label="HEAT = 0", color="blue")
    plt.fill_between(x_values, y_mean_heat_0 - y_std_heat_0, y_mean_heat_0 + y_std_heat_0, color="blue", alpha=0.2)

    # Plot HEAT = 1
    plt.plot(x_values, y_mean_heat_1, label="HEAT = 1", color="red")
    plt.fill_between(x_values, y_mean_heat_1 - y_std_heat_1, y_mean_heat_1 + y_std_heat_1, color="red", alpha=0.2)

    plt.xlabel("Time (minutes)")
    plt.ylabel("Value")
    plt.title("Time Series (Mean and Standard Deviation) Over 10 Minutes by HEAT Class")
    plt.legend()
    plt.grid(True)
    plt.show()

# Read files from all training_data directories
path_pattern = "/home/chris/experiment_data/*/training_data"
training_dirs = glob.glob(path_pattern)
df_list = []
# Loop through each training_data directory
for training_dir in training_dirs:
    if os.path.isdir(training_dir):  # Check if it exists
        csv_files = glob.glob(os.path.join(training_dir, "*.csv"))  # Get all CSV files
        
        for file in csv_files:
            df = pd.read_csv(file)  # Read CSV file
            df_list.append(df)  # Add to list

# Concatenate all data into a single DataFrame
if df_list:
    final_df = pd.concat(df_list, ignore_index=True)
else:
    final_df = pd.DataFrame()  # Return empty DataFrame if no data found

# Downsampling 600 to 100 values
final_df = downsample_last_600(final_df)
print(final_df.describe())
plot_model_input(final_df)
#final_df.to_csv("/home/chris/watchplant_classification_dl/pipline_test/input.csv", index=False)




# Define the Conv1D Model with LightningModule
class Conv1DModel(pl.LightningModule):
    def __init__(self, input_channels, output_channels, kernel_size, lr):
        super().__init__()
        self.model_name = "CNN"

        self.conv1d_1 = nn.Conv1d(input_channels, output_channels, kernel_size)
        self.pool = nn.MaxPool1d(3, stride=3) # window size 3, how far windows slided 3

        flattened_size = self._compute_flattened_size(input_channels, output_channels, kernel_size)

        self.linear = nn.Linear(flattened_size, 48, bias=False) # fully connected layer
        self.output = nn.Linear(48, 2, bias=False)
        self.loss_fn = nn.CrossEntropyLoss()
        self.accuracy = torchmetrics.Accuracy(task="multiclass", num_classes=2)
        self.f1_score = torchmetrics.F1Score(task="multiclass", num_classes=2)
        self.lr = lr


    def _compute_flattened_size(self, input_channels, output_channels, kernel_size):
        """Computes the correct flattened size after convolution and pooling layers."""
        with torch.no_grad():
            dummy_input = torch.zeros(1, input_channels, 100)  # Assume sequence length = 100
            x = self.conv1d_1(dummy_input)
            x = F.relu(x)
            x = self.pool(x)
            x = torch.transpose(x, 1, 2)
            x = torch.cat((x[:, :, 0], x[:, :, 1]), dim=1)  # No flattening, just concatenation
            return x.shape[1]

    def forward(self, x):
        # Ensure input is [batch_size, input_channels, seq_length]
        # Conv Layer
        x = self.conv1d_1(x)
        x = F.relu(x)

        x = self.pool(x) # compress to one convolution block
        # **Flatten**
        x = torch.transpose(x, 1, 2)
        x = torch.cat((x[:,:,0],x[:,:,1]), dim=1)
        #print("Shape before Linear:", x.shape)

        x = self.linear(x)
        x = F.relu(x) # ReLu is not linear. At least one non-linear to recognize non-linear pattern
        x = F.softmax(self.output(x),dim=1) # Sum = 1
        return x

    def _common_step(self, batch, batch_idx):
        x, y = batch
        output = self(x)
        y = torch.argmax(y, dim=1)
        loss = self.loss_fn(output, y)  # Example loss
        #self.log("train_loss", loss)
        y_pred = torch.argmax(output, dim=1)
        return loss, output, y, y_pred

    def training_step(self, batch, batch_idx):
        loss, scores, y, y_pred = self._common_step(batch, batch_idx)
        # print(f"scores: {scores}")
        accuracy = self.accuracy(y_pred, y)
        self.log_dict(
            {
                "train_loss": loss,
                "train_accuracy": accuracy,
            },
            on_step=False,
            on_epoch=True,
            prog_bar=True,
        )
        return {"loss": loss, "train_accuracy": accuracy, 
                "train_scores": scores, "train_y": y}

    def validation_step(self, batch, batch_idx):
        loss, scores, y, y_pred = self._common_step(batch, batch_idx)
        accuracy = self.accuracy(y_pred, y)
        self.log_dict(
            {
                "val_loss": loss,
                "val_accuracy": accuracy
            },
            on_step=False,
            on_epoch=True,
            prog_bar=True,
        )
        return {"val_loss": loss, "val_accuracy": accuracy}

    def test_step(self, batch, batch_idx):
        loss, scores, y, y_pred = self._common_step(batch, batch_idx)
        accuracy = self.accuracy(y_pred, y)
        f1_score = self.f1_score(y_pred, y)
        self.log_dict(
            {
                "test_loss": loss,
                "test_accuracy": accuracy,
                "test_f1_score": f1_score,
            },
            on_step=False,
            on_epoch=True,
            prog_bar=True,
        )        
        return {"test_loss": loss, "test_accuracy": accuracy, "test_f1_score": f1_score}

    def configure_optimizers(self):
        return torch.optim.Adam(self.parameters(), lr=self.lr)

    def plot(self):
        draw_graph(self, input_size=(1,1,100), expand_nested=True, save_graph=True, filename=self.model_name,
                   directory="results/model_viz/")

    can_delegate = False

# Early stopping training --> over 50 Epochs no improvement --> stop

train_df, val_df, test_df = split_data(final_df)

#train_df.to_csv("/home/chris/watchplant_classification_dl/pipline_test/train.csv")

x_train, y_train = df_to_tensor(train_df, "Heat")  
x_val, y_val = df_to_tensor(val_df, "Heat")  
x_test, y_test = df_to_tensor(test_df, "Heat")

train_dataset = TensorDataset(x_train, y_train)
val_dataset = TensorDataset(x_val, y_val)
test_dataset = TensorDataset(x_test, y_test)

train_loader = DataLoader(train_dataset, batch_size=8, num_workers=7)
val_loader = DataLoader(val_dataset, batch_size=8, num_workers=7)
test_loader = DataLoader(test_dataset, batch_size=8, num_workers=7)

# Train the Model
model = Conv1DModel(input_channels=1, output_channels=32, kernel_size=5, lr=0.0007503666240513933)

# Define checkpoint callback
checkpoint_callback = ModelCheckpoint(
    monitor="val_accuracy",  # Track validation loss
    mode="max",          # Lower is better
    save_top_k=1,        # Save only the best model
    filename="best_model-{epoch:02d}-{val_loss:.4f}"
)
trainer = pl.Trainer(max_epochs=100, callbacks=[checkpoint_callback], enable_progress_bar=True)
trainer.fit(model, train_loader, val_loader)

#Retrieve best model path and score
best_model_path = checkpoint_callback.best_model_path
best_model_score = checkpoint_callback.best_model_score

print(f"Best Model Path: {best_model_path}")
print(f"Best Model Score (val_loss): {best_model_score}")

model = Conv1DModel.load_from_checkpoint(
    best_model_path,
    input_channels=1,
    output_channels=32,
    kernel_size=5,
    lr=0.0007503666240513933)

trainer.test(model, test_loader)

model.eval()

torch.save(model, "60-min-max-model.pt")

# Example Input
final_example_input = (torch.randn(1, 1, 100),)  # [batch_size, input_channels, seq_length]

# Define fixed input tensor should be classified as HEAT=1
fixed_input = torch.tensor([[[ 0.4289, 0.4291, 0.4288, 0.4287, 0.4288, 0.4288, 0.4286, 0.4286, 0.4286,
         0.4286, 0.4288, 0.4288, 0.4288, 0.4287, 0.4284, 0.4282, 0.4280, 0.4279,
         0.4278, 0.4278, 0.4279, 0.4279, 0.4281, 0.4282, 0.4283, 0.4284, 0.4284,
         0.4283, 0.4281, 0.4279, 0.4279, 0.4278, 0.4277, 0.4280, 0.4281, 0.4281,
         0.4281, 0.4281, 0.4284, 0.4284, 0.4285, 0.4286, 0.4286, 0.4287, 0.4288,
         0.4287, 0.4287, 0.4285, 0.4283, 0.4281, 0.4280, 0.4280, 0.4280, 0.4280,
         0.4280, 0.4279, 0.4281, 0.4282, 0.4283, 0.4284, 0.4286, 0.4288, 0.4292,
         0.4295, 0.4295, 0.4296, 0.4296, 0.4295, 0.4293, 0.4291, 0.4290, 0.4290,
         0.4291, 0.4295, 0.4295, 0.4295, 0.4294, 0.4293, 0.4290, 0.4288, 0.4287,
         0.4287, 0.4286, 0.4286, 0.4285, 0.4286, 0.4286, 0.4286, 0.4287, 0.4288,
         0.4289, 0.4291, 0.4293, 0.4294, 0.4295, 0.4295, 0.4294, 0.4294, 0.4294,
         0.4293]]],)

# [[9.9968e-01, 3.1811e-04]]
#output = model(fixed_input)  
# print("Model Input:", fixed_input)
#print("Model Output:", output)

numpy_input = fixed_input[0].detach().cpu().numpy()

# Flatten to a 1D list
flattened_input = numpy_input.flatten().tolist()

# Print the vector representation
print("C++ std::vector<float> representation:")
print(flattened_input)

# Export the Model
pre_autograd_aten_dialect = export_for_training(
        model,
        final_example_input
    ).module()


aten_dialect: ExportedProgram = export(pre_autograd_aten_dialect, final_example_input)

print("After:",aten_dialect)

edge_program: exir.EdgeProgramManager = exir.to_edge(aten_dialect)

executorch_program: exir.ExecutorchProgramManager = edge_program.to_executorch(
    ExecutorchBackendConfig(
        passes=[],  # User-defined passes
    )
)

with open("model.pte", "wb") as file:
    file.write(executorch_program.buffer)

print("CNN model saved as model.pte")