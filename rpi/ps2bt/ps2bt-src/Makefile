TARGET=ps2bt
INCDIR=include 
SRCDIR=src
OBJDIR=obj
BINDIR=bin
ARGS=
CC=gcc
CFLAGS=-Wall -std=c89 -pedantic-errors -O3 -I./$(INCDIR)
LDFLAGS=-lm -pthread

SOURCES:=$(wildcard $(SRCDIR)/*.c)
INCLUDES:=$(wildcard $(INCDIR)/*.h)
OBJECTS:=$(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RM=rm -f

$(BINDIR)/$(TARGET): $(OBJECTS)
	@$(CC) -o $@ $(OBJECTS) $(LDFLAGS)
	@echo "Created "$(BINDIR)"/"$(TARGET)

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo $<

.PHONY: clean
clean:
	@$(RM) $(OBJECTS)
	@$(RM) $(BINDIR)/$(TARGET)

.PHONY: benchmark
benchmark: CFLAGS+=-DBENCHMARK
benchmark: $(BINDIR)/$(TARGET)

