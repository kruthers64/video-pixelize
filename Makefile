VERSION := 0.9.0
RELEASE_NAME := kruthers-video-pixelize
RELEASE_BUILD_DIR := /tmp/$(RELEASE_NAME)

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
TEST_OUT := test-out
OUT := out

.PHONY: all test view run clean release readme

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

all: out $(OUT)/$(CORE).so $(OUT)/$(META).so $(OUT)/$(KT01).so $(OUT)/$(KT02).so

out:
	mkdir -p $(OUT)

$(HEADERS) : generate-headers.pl patterns/*.xpm
	./generate-headers.pl

$(OUT)/$(CORE).so: $(CORE).c config.h $(HEADERS)
$(OUT)/$(META).so: $(META).c $(CORE).c config.h $(HEADERS)
$(OUT)/$(KT01).so: $(KT01).c config.h
$(OUT)/$(KT02).so: $(KT02).c config.h
$(OUT)/%.so: %.c
	gcc $(CFLAGS) $< `pkg-config --cflags --libs glib-2.0` -I. -fpic -o $@
	cp -pv $@ $(INSTALL_DIR)

test: all
	mkdir -p $(TEST_OUT)
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
	    gegl $$filename -o $$c -- kruthers:$(META) pattern=plus sampler-type=cubic scale=2.29 ; \
	done

view:
	qimgv $(TEST_OUT)/* >> /dev/null 2>&1 &

run: all
	gimp $(TEST_DIR)/$(RUN_IMAGE)

clean:
	rm -vf *.so $(TEST_OUT)/* $(OUT)/* $(HEADERS)
	rm -vf $(INSTALL_DIR)/$(CORE).so
	rm -vf $(INSTALL_DIR)/$(META).so
	rm -vf $(INSTALL_DIR)/$(KT01).so
	rm -vf $(INSTALL_DIR)/$(KT02).so
	rm -rvf $(RELEASE_BUILD_DIR)

release: all
	mkdir -p $(RELEASE_BUILD_DIR)
	cp -p $(OUT)/$(CORE).so $(RELEASE_BUILD_DIR)
	cp -p $(OUT)/$(META).so $(RELEASE_BUILD_DIR)
	tar --directory $(RELEASE_BUILD_DIR)/.. --create --verbose --file $(OUT)/$(RELEASE_NAME)-$(VERSION).tgz $(RELEASE_NAME)

readme:
	pandoc -o README.html README.md
