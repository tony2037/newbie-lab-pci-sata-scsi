MODULE_NAME  = gpio-control
obj-m       := $(MODULE_NAME).o

C_SRCS = user-control.c
TARGET = user-control

.PHONY: all
all: $(TARGET)

$(TARGET): $(C_SRCS)
	$(CC) $(CFLAGS) -o $@ $^
