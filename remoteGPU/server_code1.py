import torch
print("Torch version:", torch.__version__)  # Prints PyTorch version
print("CUDA available:", torch.cuda.is_available())  # Checks if CUDA is available
print("Number of GPUs:", torch.cuda.device_count())  # Number of GPUs
print("GPU Name:", torch.cuda.get_device_name(0) if torch.cuda.is_available() else "No GPU found")  
