TARGET_ISA=arm64

GEM5_HOME=$(realpath ../../../..)
$(info   GEM5_HOME is $(GEM5_HOME))

CXX=aarch64-linux-gnu-g++

CFLAGS=-I$(GEM5_HOME)/include

#FLAGS=-static -fno-tree-vectorize -O3 -ffp-contract=off
FLAGS=-static -O3

LDFLAGS=-L$(GEM5_HOME)/util/m5/build/$(TARGET_ISA)/out -lm5

all: daxpy iaxpy iax saxpy sax dax
asm: daxpy-asm iaxpy-asm iax-asm saxpy-asm sax-asm dax-asm

daxpy-asm:
	$(CXX) -S $(FLAGS) -o daxpy-asm.S daxpy.cc $(CFLAGS) $(LDFLAGS)

dax-asm:
	$(CXX) -S $(FLAGS) -o dax-asm.S dax.cc $(CFLAGS) $(LDFLAGS)

iaxpy-asm:
	$(CXX) -S $(FLAGS) -o iaxpy-asm.S iaxpy.cc $(CFLAGS) $(LDFLAGS)

iax-asm:
	$(CXX) -S $(FLAGS) -o iax-asm.S iax.cc $(CFLAGS) $(LDFLAGS)

saxpy-asm:
	$(CXX) -S $(FLAGS) -o saxpy-asm.S saxpy.cc $(CFLAGS) $(LDFLAGS)

sax-asm:
	$(CXX) -S $(FLAGS) -o sax-asm.S sax.cc $(CFLAGS) $(LDFLAGS)


daxpy:
	$(CXX) $(FLAGS) -o daxpy daxpy.cc $(CFLAGS) $(LDFLAGS)

dax:
	$(CXX) $(FLAGS) -o dax dax.cc $(CFLAGS) $(LDFLAGS)

iaxpy:
	$(CXX) $(FLAGS) -o iaxpy iaxpy.cc $(CFLAGS) $(LDFLAGS)

iax:
	$(CXX) $(FLAGS) -o iax iax.cc $(CFLAGS) $(LDFLAGS)

saxpy:
	$(CXX) $(FLAGS) -o saxpy saxpy.cc $(CFLAGS) $(LDFLAGS)

sax:
	$(CXX) $(FLAGS) -o sax sax.cc $(CFLAGS) $(LDFLAGS)

clean:
	rm -f daxpy iaxpy iax saxpy sax dax
clean-asm:
	rm -f daxpy-asm.S iaxpy-asm.S saxpy-asm.S iax-asm.S sax-asm.S dax-asm.S
