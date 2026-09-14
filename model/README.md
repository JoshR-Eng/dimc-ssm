# State Space 'Golden' Model
This `~/model` directory contains the machine learning model that is used to determine the structure of the hardware accelerator and for future validation.

State Space Models directly addresses the primary structural limitations of Transformers by shifting from a quadratic complexity model to a linear complexity model with respect to sequency length. However, this comes at the cost of parallelisation. SSMs contain recurrent layers and its computation is not as well suited for the GPU architecture.

Given the trade-off, SSMs suit as an more optimal alternative for long-sequence processing on memory constrained devices.

SSMs are developing at a rapid pace, so targetting a singular architecture would soon make a dedicated hardware accelerator obselete. Instead, the goal is to accelerate the fundemental SSM computations that cannot be well optimised for GPUs. Hence, the 'Golden Model' is a relatively simple S4D.

## Design Questions
### Q1. Total Bit Width


### Q2. Uniform or Per-Channel Q-format


### Q3. Integer/Fraction Split per signal class


### Q4. Truncate or Round-to-nearest at the post-multiply shift


### Q5. How should overflow be handled
Clipping


### Q6. Accumulator/gaurd-bit width for the channel-reduction sum



