
CLIENT		= client
INCLUDES	= -I. 
SUBLIBS		= -lpthread -lm -lrt

.PHONY:all
all: $(CLIENT)

CLIENT_SRC		= client.c

$(CLIENT):$(CLIENT_SRC)
	gcc $(CLIENT_SRC) -o $(CLIENT) $(SUBLIBS) $(INCLUDES)


clean:
	rm $(CLIENT)  *~
