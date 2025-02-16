TARGET   = libgltf.a
CLASSES  = gltf
SOURCE   = $(CLASSES:%=%.c)
OBJECTS  = $(SOURCE:.c=.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I.
ifeq ($(GLTF_DEBUG),1)
	CFLAGS += -DGLTF_DEBUG
endif
LDFLAGS  =
AR       = ar

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)

$(OBJECTS): $(HFILES)
