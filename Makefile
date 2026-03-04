TEST_IMAGE := test02.png
RUN_IMAGE := test02-pink.xcf
META := video-pixelize
CORE := video-pixelize-core
KT01 := krtest01
KT02 := krtest02
HEADERS := video-pixelize-gegl-enum.h video-pixelize-patterns.h
INSTALL_DIR := $(HOME)/.local/share/gegl-0.4/plug-ins
TEST_DIR := test-images
TEST_IN := $(TEST_DIR)/$(TEST_IMAGE)
TEST_OUT := out

.PHONY: all test view run clean

CFLAGS := -g -shared -Werror

# includes from env vars
ifdef GIMP_LIB
    CFLAGS := $(CFLAGS) -I$(GIMP_LIB)
endif
ifdef GEGL_INC
    CFLAGS := $(CFLAGS) -I$(GEGL_INC)
endif
ifdef BABL_INC
    CFLAGS := $(CFLAGS) -I$(BABL_INC)
endif

all: $(CORE).so $(META).so $(KT01).so $(KT02).so

$(HEADERS) : generate-headers.pl patterns/*.xpm
	./generate-headers.pl

$(CORE).so: $(CORE).c config.h $(HEADERS)
$(META).so: $(META).c $(CORE).c config.h $(HEADERS)
$(KT01).so: $(KT01).c config.h
$(KT02).so: $(KT02).c config.h
%.so: %.c
	gcc $(CFLAGS) $< `pkg-config --cflags --libs glib-2.0` -I. -fpic -o $@
	cp -pv $@ $(INSTALL_DIR)

test: all
	@for filename in $(TEST_IN) ; do \
	    base=$$(basename $$filename) ; \
	    base=$${base%%.*} ; \
	    ext=$${filename##*.} ; \
	    a=$(TEST_OUT)/$$base-a-orig.$$ext ; \
	    b=$(TEST_OUT)/$$base-b-test1.$$ext ; \
	    c=$(TEST_OUT)/$$base-c-test2.$$ext ; \
	    echo copy $$a ; \
	    cp $$filename $$a ; \
	    echo make $$b ; \
	    gegl $$filename -o $$b -- kruthers:$(META) ; \
	    echo make $$c ; \
	    gegl $$filename -o $$c -- kruthers:$(META) pattern=plus-sign sampler-type=cubic scale=2.29 ; \
	done

view:
	qimgv $(TEST_OUT)/* >> /dev/null 2>&1 &

run: all
	gimp $(TEST_DIR)/$(RUN_IMAGE)

clean:
	rm -vf *.so $(TEST_OUT)/* $(HEADERS)
	rm -vf $(INSTALL_DIR)/$(CORE).so
	rm -vf $(INSTALL_DIR)/$(META).so
	rm -vf $(INSTALL_DIR)/$(KT01).so
	rm -vf $(INSTALL_DIR)/$(KT02).so
