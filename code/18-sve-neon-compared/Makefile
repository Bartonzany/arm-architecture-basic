CROSS       ?= aarch64-linux-gnu-
CC          := $(CROSS)gcc
OBJDUMP     := $(CROSS)objdump
QEMU        ?= qemu-aarch64
CFLAGS      := -O3 -march=armv8-a+sve
SRCDIR      := src
BUILDDIR    := build
BINDIR      := $(BUILDDIR)/bin
DISASMDIR   := $(BUILDDIR)/disasm
SOURCES     := $(wildcard $(SRCDIR)/*.c)
PROGRAMS    := $(patsubst $(SRCDIR)/%.c,$(BINDIR)/%,$(SOURCES))
.PHONY: all disasm run clean
all: $(PROGRAMS)
$(BINDIR)/%: $(SRCDIR)/%.c | $(BINDIR)
	$(CC) $(CFLAGS) $< -o $@
disasm: all
	@mkdir -p $(DISASMDIR)
	@for src in $(SOURCES); do \
		n=$$(basename $$src .c); \
		$(CC) $(CFLAGS) -c $$src -o $(DISASMDIR)/$$n.o; \
		$(OBJDUMP) -d --no-show-raw-insn $(DISASMDIR)/$$n.o > $(DISASMDIR)/$$n.txt; \
		rm -f $(DISASMDIR)/$$n.o; \
	done
run: all
	@for p in $(PROGRAMS); do echo "===== $$(basename $$p) ====="; $(QEMU) -cpu max $$p; done
$(BINDIR) $(DISASMDIR):
	mkdir -p $@
clean:
	rm -rf $(BUILDDIR)
