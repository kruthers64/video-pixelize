TEST_IMAGE := test02.png
NAME := video-pixelize
HEADERS := video-pixelize-gegl-enum.h video-pixelize-patterns.h
INSTALL_DIR := $(HOME)/.local/share/gegl-0.4/plug-ins
TEST_DIR := test-images
TEST_IN := $(TEST_DIR)/test02.png
TEST_OUT := out

.PHONY: all test clean view run

CFLAGS := -shared -Werror

all: $(NAME).so

$(HEADERS) : generate-headers.pl patterns/*.xpm
	./generate-headers.pl

$(NAME).so: $(NAME).c config.h $(HEADERS)
	gcc $(CFLAGS) $(NAME).c `pkg-config --cflags --libs gegl-0.4` -I. -fpic -o $(NAME).so
	cp -pv $(NAME).so $(INSTALL_DIR)

test: all
	@for f in $(TEST_IN) ; do \
	    b=$$(basename $$f) ; \
	    b=$${b%%.*} ; \
	    e=$${f##*.} ; \
	    cp -v $$f $(TEST_OUT)/$$b-a.$$e ; \
	    echo make $(TEST_OUT)/$$b-b.$$e ; \
	    gegl $$f -o $(TEST_OUT)/$$b-b.$$e -- kruthers:$(NAME) ; \
	    echo make $(TEST_OUT)/$$b-c.$$e ; \
	    gegl $$f -o $(TEST_OUT)/$$b-c.$$e -- kruthers:$(NAME) brightness=true ; \
	done

testbr: all
	for f in $(TEST_IN) ; do gegl $$f -o $(TEST_OUT)/`basename $$f` -- kruthers:$(NAME) brightness=true ; done

clean:
	rm -vf *.so out/*

view:
	qimgv $(TEST_OUT) >> /dev/null 2>&1 &

run: all
	gimp $(TEST_DIR)/$(TEST_IMAGE)
