ifeq ($(MEMORY_REQUEST_WAIT),y)
    core-objs-y+=scheds/dp_wcet.o
else
    core-objs-y+=scheds/fp_classic.o
endif