import os

# Prepare dataset
os.system("python ./prepare_dataset.py --images_dir ../data/T91/original --output_dir ../data/T91/ESPCN/train --image_size 70 --step 35 --num_workers 10")

# Split train and valid
os.system("python ./split_train_valid_dataset.py --train_images_dir ../data/T91/ESPCN/train --valid_images_dir ../data/T91/ESPCN/valid --valid_samples_ratio 0.1")
