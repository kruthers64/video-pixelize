TEST_IMAGE := test02.png
RUN_IMAGE := test02-pink.xcf
META := video-pixelize
CORE := video-pixelize-core
KRTEST01 := krtest01
HEADERS := video-pixelize-gegl-enum.h video-pixelize-patterns.h
INSTALL_DIR := $(HOME)/.local/share/gegl-0.4/plug-ins
TEST_DIR := test-images
TEST_IN := $(TEST_DIR)/$(TEST_IMAGE)
TEST_OUT := out
GIMP_DEV := $(HOME)/apps/gimp-dev
GIMP_LIB := $(GIMP_DEV)/lib/x86_64-linux-gnu
GEGL_INC := $(GIMP_DEV)/include/gegl-0.4
BABL_INC := $(GIMP_DEV)/include/babl-0.1

.PHONY: all test clean view run

CFLAGS := -g -shared -Werror -I$(GIMP_LIB) -I$(GEGL_INC) -I$(BABL_INC)

all: $(CORE).so $(META).so $(KRTEST01).so

$(HEADERS) : generate-headers.pl patterns/*.xpm
	./generate-headers.pl

$(CORE).so: $(CORE).c config.h $(HEADERS)
	gcc $(CFLAGS) $(CORE).c `pkg-config --cflags --libs glib-2.0` -I. -fpic -o $(CORE).so
	cp -pv $(CORE).so $(INSTALL_DIR)

$(META).so: $(META).c $(CORE).c config.h $(HEADERS)
	gcc $(CFLAGS) $(META).c `pkg-config --cflags --libs glib-2.0` -I. -fpic -o $(META).so
	cp -pv $(META).so $(INSTALL_DIR)

$(KRTEST01).so: $(KRTEST01).c config.h
	gcc $(CFLAGS) $(KRTEST01).c `pkg-config --cflags --libs glib-2.0` -I. -fpic -o $(KRTEST01).so
	cp -pv $(KRTEST01).so $(INSTALL_DIR)

test: all
	@for f in $(TEST_IN) ; do \
	    b=$$(basename $$f) ; \
	    b=$${b%%.*} ; \
	    e=$${f##*.} ; \
	    cp -v $$f $(TEST_OUT)/$$b-a.$$e ; \
	    #echo make $(TEST_OUT)/$$b-b.png ; \
	    #gegl $$f -o $(TEST_OUT)/$$b-b.png -- kruthers:$(META) ; \
	    echo make $(TEST_OUT)/$$b-c.png ; \
	    gegl $$f -o $(TEST_OUT)/$$b-c.png -- kruthers:$(META) pattern=plus-3x3 sampler-type=cubic scale=2.29 ; \
	done

clean:
	rm -vf *.so $(TEST_OUT)/* $(HEADERS) $(INSTALL_DIR)/*.so

view:
	qimgv $(TEST_OUT) >> /dev/null 2>&1 &

run: all
	gimp $(TEST_DIR)/$(RUN_IMAGE)
