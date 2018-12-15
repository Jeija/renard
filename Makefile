TARGET := renard
SRCDIR := src/
BUILDDIR := build/

LIBRENARD_DIR := ./librenard/
LIBRENARD := $(LIBRENARD_DIR)librenard.a

INCDIRS := -I $(LIBRENARD_DIR)src
CFLAGS := -Wall
LIBS := $(LIBRENARD)

SRCS := $(wildcard  $(SRCDIR)*.c)

all: $(SRCS) $(LIBS)
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCDIRS) $(SRCS) $(LIBS) -D TARGETNAME=\"$(TARGET)\" -o $(BUILDDIR)$(TARGET)

run: all
	./$(BUILDDIR)$(TARGET) help

test: all
	./$(BUILDDIR)$(TARGET) test

FORCE:
$(LIBRENARD): FORCE
	$(MAKE) -C $(LIBRENARD_DIR)

clean:
	$(RM) -r $(BUILDDIR)
	$(MAKE) -C $(LIBRENARD_DIR) clean
