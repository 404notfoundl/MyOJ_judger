TARGET := dispatcher
SUBDIRS := $(shell ls -d */)
SUBDIRS := $(SUBDIRS:%/=%)
FILTER_OUT := test_case
FILTER_OUT += $(TARGET)
SUBDIRS := $(filter-out $(FILTER_OUT),$(SUBDIRS))
OPT ?= -O2


all: $(SUBDIRS)
	@cd $(TARGET) && $(MAKE) OPT=$(OPT) && cd ..
	
$(SUBDIRS):
	@cd $@ && $(MAKE) OPT=$(OPT) && cd ..

print:
	@echo $(SUBDIRS)
clean:
	@for sub in $(SUBDIRS); \
	do \
	cd $$sub && $(MAKE) clean && cd ..; \
	done
	cd $(TARGET) && $(MAKE) clean && cd ..
	@echo "clean finish"
	
.PHONY:clean $(SUBDIRS)
