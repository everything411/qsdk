CFLAGS += -Wall -Werror
LDFLAGS += -lubox


OBJS=   mtd.o \
	jffs2.o \
	crc32.o \
	md5.o

.PHONY: all clean

mtd.o :
	$(CC) -c $(CFLAGS) mtd.c

jffs2.o :
	$(CC) -c $(CFLAGS) jffs2.c

crc32.o :
	$(CC) -c $(CFLAGS) crc32.c

md5.o :
	$(CC) -c $(CFLAGS) md5.c

mtd: $(OBJS)
	$(CC) $(OBJS) $(LDLIBS) $(LDFLAGS) -o mtd

clean:
	-$(RM) *.o
	-$(RM) mtd

