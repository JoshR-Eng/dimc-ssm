# CV-X-IF Analog SSM Coprocessor

This repository contains the RTL, verification and emulation framework for an ultra-low-power Edge AI accelerator targeting State-Space Models (SSMs).

The next generation of physical AI deamnds a shift from static, open-loop models to continous, closed-loop sequence processing. However, traditional hardware is stunted by the von Neumann memory wall.

This project aims to tackle the bandwidth constraint through a decoupled, mixed-singal architecture. Utilising the CV-X-IF interface, an RV32E host (CV32E40X) can be physically appended with a stochastic Analogue In-Memory Compute (AIMC) crossbar. By computing the weights *in situ* in the analogue domain, this architecture bypasses digital SRAM bottlenecks. This could enable real-time, ultra-low power AI for edge deployment such as smart hearing aids, autonomous micro-drones and continous medical wearables.

---

## Methodology: A Mixed-Signal Emulation Bridge

Simulating an AIMC crossbar alongside a cycle-accurate RISC-V host present a verification challenge. Standard analogue simulators (e.g. LTspice) are too slow to run SSM inference, while digital simulators (e.g. Verilator) cannot natively model analogue physics.

This project approaches the entire hardware stack using a tightly integrated emulation pipeline:
1. **Transistor-Level Simulation (LTspice):** Design a micro-tile (e.g. 16x16 SRAM-CIM array) at the transistor level then run Monte Carlo simluations for feature extraction
2. **Behavioural Macromodelling (C++):** Write a C++ hardware macromodel that performs the SSM vector-matrix multiplicaiton. The model actively injects the randomised noise profile extracted from the SPICE simulations.
3. **Digital Hardware Integration (SystemVerilog DPI-C):** Append the Behavioural model to the CV-X-IF interface using the Direct Programming Interface (DPI-C) to call the C++ macromodel. When the RISC-V host pushes an instruction, the C++ engine computes the noisy analogue result and passes it back to the digital RTL bus on the required clock edge.

---

## Project Roadmap
- [x] **Phase 1: Digital Baseline**
  - [x] Design parameterizable TCDM circular FIFO for instruction decoupling.
  - [x] Implement deterministic dummy MAC execution pipeline.
  - [x] Build CV-X-IF skid buffer for host backpressure handling.
  - [x] Construct cycle-accurate C++ Verilator testbench.

- [ ] **Phase 2: The DPI-C Analog Bridge \[ACTIVE\]**
  - [ ] Replace dummy MAC with a SystemVerilog DPI-C wrapper.
  - [ ] Write the C++ AIMC matrix-math engine with configurable Gaussian noise injection.
  - [ ] Verify FSM timing: Ensure the digital FIFO correctly stalls while the C++ analog engine "settles" its bitlines.

- [ ] **Phase 3: Dual-Bank Memory Architecture**
  - [ ] Enable support for custom sub-opcodes (Load, Compute, Read).
  - [ ] Implement a Ping-Pong Scratchpad memory map for continous processing.
  - [ ] Verify continuous SSM data streaming without host-side bus stalling.

- [ ] **Phase 4: SPICE & Algorithm Co-Design**
  - [ ] Design the analog crossbar micro-tile in LTspice and extract non-ideality parameters.
  - [ ] Build the PyTorch hardware-aware emulator for the SSM selective scan.
  - [ ] Inject the SPICE-derived $\sigma_{I_{read}}$ noise into PyTorch to prove the SSM converges despite physical analog variations.
