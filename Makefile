default:
	rm xiaomi -f
	gcc -o xiaomi xiaomi.c -lcurl -lpthread -Wall
run:
	./xiaomi
clean:
	rm -f login_out.log prebuy.log writedata.dat writeheader.dat cookie cookies/*
