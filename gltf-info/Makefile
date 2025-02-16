export CC_USE_JSMN  = 1
export CC_USE_MATH  = 1
export GLTF_DEBUG   = 1

TARGET   = gltf-info
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  =  -Llibgltf -lgltf -Llibcc -lcc -lm
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) libgltf libcc
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: libgltf libcc

libgltf:
	$(MAKE) -C libgltf

libcc:
	$(MAKE) -C libcc

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C libgltf clean
	$(MAKE) -C libcc clean
	rm libgltf libcc jsmn

$(OBJECTS): $(HFILES)
