#------------------------------------------------------------------------------
#
#
#
#
#------------------------------------------------------------------------------ 
APP_NAME = main 
HEADERS = intel_hex_parser.h
OBJECTS = main.o intel_hex_parser.o serial_lib.o

default: $(APP_NAME)

%.o: %.c $(HEADERS)
	gcc -c $< -o $@ 

$(APP_NAME): $(OBJECTS)
	gcc $(OBJECTS) -o $@ 

clean:
	-rm -f $(OBJECTS)
	-rm -f $(APP_NAME)

upload:
	./main payload.hex

iterate:
	clear && make clean && make && ./main payload.hex