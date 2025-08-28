Universal Machine (UM) Interpreter in C

A fast, minimal, single-file implementation of the Universal Machine (UM) from the classic “Comp40/UMASM” assignment. It executes 32-bit UM binaries using segmented memory, eight registers, and the full 14-opcode instruction set.

Features

Complete 14-instruction UM ISA:
CMOV, SEGLOAD, SEGSTORE, ADD, MUL, DIV, NAND, HALT, MAPSEG, UNMAPSEG, OUT, IN, LOADPROG, LOADVAL

Segmented memory with free-list ID reuse and amortized growth

LOADPROG duplication semantics for segment 0

Unbuffered byte I/O via POSIX read/write

Big-endian word loader, zero-initialized mapped segments

Tight interpreter loop with fast paths for common ops

Build
Simple compile (Linux/macOS)
cc -std=c17 -O2 -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-align \
   -Wstrict-aliasing=2 -Wconversion -Wsign-conversion \
   -o um um.c


Optional flags for local performance:

-O3 -march=native (omit -march=native for portable builds)

Makefile (optional)
CC      := cc
CFLAGS  := -std=c17 -O2 -Wall -Wextra -Wshadow -Wpointer-arith \
           -Wcast-align -Wstrict-aliasing=2 -Wconversion -Wsign-conversion

um: um.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f um

Run
./um path/to/program.um


Input: read from stdin by the IN instruction (one byte at a time).

Output: written to stdout by the OUT instruction (one byte at a time).

Exit: returns 0 on success; prints an error and returns non-zero on argument errors.

UM Binary Format

File is a sequence of big-endian 32-bit words.

Each word encodes a single instruction.

The interpreter reads the entire binary into segment 0 at startup.

Registers and Encoding

Eight unsigned 32-bit registers: r[0] … r[7], all initialized to 0.

ABC-form (opcodes 0–12 except 13)
bits 31..28: opcode
bits  8.. 6: reg A
bits  5.. 3: reg B
bits  2.. 0: reg C

LOADVAL (opcode 13)
bits 31..28: 13
bits 27..25: reg A
bits 24.. 0: 25-bit immediate value

Instruction Set (Quick Reference)
Opcode	Mnemonic	Semantics (unsigned arithmetic)
0	CMOV	if r[C] != 0 then r[A] = r[B]
1	SEGLOAD	r[A] = M[r[B]][r[C]]
2	SEGSTORE	M[r[A]][r[B]] = r[C]
3	ADD	r[A] = (r[B] + r[C]) mod 2^32
4	MUL	r[A] = (r[B] * r[C]) mod 2^32
5	DIV	r[A] = r[B] / r[C] (unsigned)
6	NAND	r[A] = ~(r[B] & r[C])
7	HALT	Stop execution
8	MAPSEG	Map new segment of length r[C], zero-filled; set r[B] to its ID
9	UNMAPSEG	Unmap segment r[C] and push its ID onto the free-list
10	OUT	Write byte r[C] (0–255) to stdout
11	IN	Read a byte from stdin into r[C]; on EOF set r[C] = 0xFFFFFFFF
12	LOADPROG	If r[B] != 0: copy M[r[B]] into M[0]; set PC to r[C]
13	LOADVAL	r[A] = immediate25
Memory Model

Segmented memory: array of segment pointers.

m[i] → uint32_t* data for segment i

len[i] → active length in words

capacity[i] → allocated capacity in words

Segment IDs are recycled using a free-list (unusedIDs).

Segment 0 always holds the currently executing program.

MAPSEG zero-fills new segments; capacities grow by doubling when needed.

Implementation Notes

Uses setvbuf to disable stdio buffering for deterministic I/O around IN/OUT.

Loader constructs big-endian words from file bytes.

Fast-paths for LOADVAL, SEGLOAD, SEGSTORE avoid the switch’s overhead.

Assumes well-formed programs; out-of-bounds is undefined (matching typical UM specs).

POSIX dependency: unistd.h for read/write. On Windows, use WSL or provide shims.

Performance Tips

Build with -O2 or -O3; consider -march=native for local benchmarking.

Keep inputs/outputs buffered externally if driving large I/O through IN/OUT.

Avoid unnecessary debug printing in the hot loop.

Testing

Obtain UM test binaries (e.g., halt.um, hello.um, course-provided suites).

Run tests:

./um tests/halt.um
./um tests/hello.um < input.txt > output.txt


Compare outputs to reference results when provided.

Repository Layout

um.c — main interpreter (this file)

README.md — this document

(optional) Makefile, .gitignore, tests/

Example .gitignore entries:

um
*.o
.DS_Store

License

Add a license of your choice (e.g., MIT or Apache-2.0) to clarify reuse and contributions.
