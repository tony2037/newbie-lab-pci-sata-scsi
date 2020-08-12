MODULE_NAME  = gpio-control
obj-m       += $(MODULE_NAME).o
obj-m       += pci-monitor.o

C_SRCS = user-control.c
TARGET = user-control

.PHONY: all
all: $(TARGET) pci-receiver

$(TARGET): $(C_SRCS)
	$(CC) $(CFLAGS) -o $@ $^

pci-receiver: pci-receiver.c
	$(CC) $(CFLAGS) -o $@ $^
