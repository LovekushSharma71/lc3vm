# Little Computer 3 (LC-3) Virtual Machine (VM) in C

This repository contains a C implementation of the Little Computer 3 (LC-3) virtual machine, built from scratch based on the comprehensive article by J.M. Meiners: [https://www.jmeiners.com/lc3-vm/](https://www.jmeiners.com/lc3-vm/).

## About the LC-3

The LC-3 is a simplified 16-bit instruction set architecture (ISA) designed for educational purposes. It provides a foundational understanding of computer architecture and assembly language programming. This project aims to replicate the functionality of the LC-3 using C, enabling users to execute LC-3 assembly code.

## Features

* **Complete LC-3 Instruction Set Implementation:** All LC-3 opcodes and addressing modes are supported.
* **Memory Management:** Simulates the LC-3's memory space.
* **Register File:** Models the LC-3's general-purpose registers and program counter.
* **Condition Flags:** Implements the N, Z, and P condition flags.
* **Input/Output:** Basic I/O operations (e.g., keyboard input, console output).
* **Loading and Executing Object Files (.obj):** Loads LC-3 object files into memory and executes them.
* **Clean and Readable 

### Prerequisites

* A C compiler (e.g., GCC, Clang)
* Make (optional, but recommended)

Code:** Designed for educational purposes, emphasizing clarity and maintainability.

### Resources:
* J.M. Meiners' LC-3 VM Article: https://www.jmeiners.com/lc3-vm/
* LC-3 ISA Documentation : lc3-isa.pdf
<!-- ## Building

1.  **Clone the repository:**
    ```bash
    git clone [repository URL]
    cd lc3-vm-c
    ```
2.  **Compile the source code:**
    ```bash
    gcc -o lc3vm lc3vm.c
    ```
    (You may need to adjust the compiler and flags depending on your system.)

## Usage

1.  **Compile your LC-3 assembly code into an object file (.obj) using an LC-3 assembler.** (Many assemblers are available online or as part of LC-3 simulators.)
2.  **Run the LC-3 VM with the object file as an argument:**
    ```bash
    ./lc3vm your_program.obj
    ```

## File Structure -->