SUBDIRS = libupload tools lib elflink/ldlinux gpllib libutil modules mboot \
	  menu  elflink rosh  sysdump  chain

.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS):
	@mkdir -p $(OBJ)/$@
	$(MAKE) -C $(OBJ)/$@ SRC="$(SRC)"/$@ OBJ="$(OBJ)"/$@/ \
		-f $(SRC)/$@/Makefile $(MAKECMDGOALS)

all tidy dist clean spotless install: subdirs

# Parallel dependencies
chain mboot menu: lib libutil gpllib
elflink/ldlinux: lib
modules: lib libutil gpllib
rosh: lib libutil
sysdump: lib libutil libupload gpllib
