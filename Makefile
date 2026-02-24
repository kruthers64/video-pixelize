TEST_IMAGE := test02.png
RUN_IMAGE := test02-pink.xcf
NAME := video-pixelize
CORE := video-pixelize-core
HEADERS := video-pixelize-gegl-enum.h video-pixelize-patterns.h
INSTALL_DIR := $(HOME)/.local/share/gegl-0.4/plug-ins
TEST_DIR := test-images
TEST_IN := $(TEST_DIR)/$(TEST_IMAGE)
TEST_OUT := out

.PHONY: all test clean view run

CFLAGS := -shared -Werror

all: $(CORE).so $(NAME).so

$(HEADERS) : generate-headers.pl patterns/*.xpm
	./generate-headers.pl

$(CORE).so: $(CORE).c config.h $(HEADERS)
	gcc $(CFLAGS) $(CORE).c `pkg-config --cflags --libs gegl-0.4` -I. -fpic -o $(CORE).so
	cp -pv $(CORE).so $(INSTALL_DIR)

$(NAME).so: $(CORE).so $(NAME).c config.h $(HEADERS)
	gcc $(CFLAGS) $(NAME).c `pkg-config --cflags --libs gegl-0.4` -I. -fpic -o $(NAME).so
	cp -pv $(NAME).so $(INSTALL_DIR)

test: all
	@for f in $(TEST_IN) ; do \
	    b=$$(basename $$f) ; \
	    b=$${b%%.*} ; \
	    e=$${f##*.} ; \
	    cp -v $$f $(TEST_OUT)/$$b-a.$$e ; \
	    echo make $(TEST_OUT)/$$b-b.png ; \
	    gegl $$f -o $(TEST_OUT)/$$b-b.png -- kruthers:$(NAME) ; \
	    echo make $(TEST_OUT)/$$b-c.png ; \
	    gegl $$f -o $(TEST_OUT)/$$b-c.png -- kruthers:$(NAME) scale=17.23 ; \
	done

clean:
	rm -vf *.so $(TEST_OUT)/* $(HEADERS)

view:
	qimgv $(TEST_OUT) >> /dev/null 2>&1 &

run: all
	gimp $(TEST_DIR)/$(RUN_IMAGE)
