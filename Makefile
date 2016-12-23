objects = main.o serial.o net.o cam.o
output_file = serial
all : $(objects)
	gcc $^ -lpthread -ljpeg -o $(output_file) 
	@-rm *.o   *~ 

.PHONY:clean
clean:
	rm *.o   *~  $(output_file) 
