ifdef one_bit
	CFLAGS = -Done_bit
endif
ifdef two_bit
	CFLAGS = -Dtwo_bit
endif
ifdef three_bit
	CFLAGS = -Dthree_bit
endif
ifdef four_bit
	CFLAGS = -Dfour_bit
endif

object = main.o node.o
tree : $(object)
	gcc -o tree $(object) $(CFLAGS)
main.o: main.c node.h
	gcc -c main.c $(CFLAGS)
node.o: node.c node.h
	gcc -c node.c $(CFLAGS)
.PHONY:clean
clean:
	-rm $(object) tree
