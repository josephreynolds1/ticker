CC = cc
CFLAGS = -Wall -Wextra -O2
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

# Detect Operating System
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    # macOS: Check for Homebrew (Apple Silicon vs Intel)
    ifeq ($(shell [ -d /opt/homebrew ] && echo yes),yes)
        CFLAGS += -I/opt/homebrew/include
        LDFLAGS += -L/opt/homebrew/lib
    else ifeq ($(shell [ -d /usr/local/include ] && echo yes),yes)
        CFLAGS += -I/usr/local/include
        LDFLAGS += -L/usr/local/lib
    endif
    LDFLAGS += -lcurl
else
    # Linux / Alpine defaults
    LDFLAGS += -lcurl
endif

TARGET = ticker
SRC = main.c

all: $(TARGET)

# Auto-generate config.h from config.def.h if missing (suckless style)
config.h:
	@echo "Creating config.h from config.def.h..."
	cp config.def.h config.h

$(TARGET): $(SRC) config.h
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(TARGET)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR)
	cp -f $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	chmod 755 $(DESTDIR)$(BINDIR)/$(TARGET)
	@echo "Successfully installed $(TARGET) to $(DESTDIR)$(BINDIR)/$(TARGET)"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	@echo "Successfully removed $(DESTDIR)$(BINDIR)/$(TARGET)"

clean:
	rm -f $(TARGET)

.PHONY: all install uninstall clean