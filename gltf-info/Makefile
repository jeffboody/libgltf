export CC_USE_MATH  = 1

TARGET   = gltf-info
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  =  -Llibgltf -lgltf -Ljsmn/wrapper -ljsmn -Llibcc -lcc -lm
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) libgltf libcc jsmn
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: libgltf libcc jsmn

libgltf:
	$(MAKE) -C libgltf

libcc:
	$(MAKE) -C libcc

jsmn:
	$(MAKE) -C jsmn/wrapper

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C libgltf clean
	$(MAKE) -C libcc clean
	$(MAKE) -C jsmn/wrapper clean
	rm libgltf libcc jsmn

$(OBJECTS): $(HFILES)
