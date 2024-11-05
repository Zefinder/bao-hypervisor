# Dynamic Priority, set CPU to lower prio when its tasks go faster than their WCET
ifeq ($(MEMORY_REQUEST_WAIT),y)
build_macros+=-DMEMORY_REQUEST_WAIT
endif